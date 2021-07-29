/*
 * tio - a simple TTY terminal I/O application
 *
 * Copyright (c) 2014-2017  Martin Lund
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>
#include "config.h"
#include "tio/tty.h"
#include "tio/print.h"
#include "tio/options.h"
#include "tio/time.h"
#include "tio/log.h"
#include "tio/error.h"
#include "tio/lineedit.h"

#ifdef HAVE_TERMIOS2
extern int setspeed2(int fd, int baudrate);
#endif

#define ESCAPED_BUFFER_SIZE 9   // max 8 binary digit + terminating 0 

static struct termios tio, tio_old, stdout_new, stdout_old, stdin_new, stdin_old;
static unsigned long rx_total = 0, tx_total = 0;
static bool connected = false;
static bool tainted = false;
static bool print_mode = NORMAL;
static bool standard_baudrate = true;
static void (*print)(char c);
static int fd;
static bool map_i_nl_crnl = false;
static bool map_o_cr_nl = false;
static bool map_o_nl_crnl = false;
static bool map_o_del_bs = false;
static char hex_chars[3] = { 0 };
static unsigned char hex_char_index = 0;

static char parsed_line[LINE_SIZE];
static unsigned int parsed_line_ctr;

void handle_command_sequence(char input_char, char previous_char, char *output_char, bool *forward);

#define tio_printf(format, args...) \
{ \
    if (tainted) putchar('\n'); \
    color_printf("[%s] " format, current_time(), ## args); \
    tainted = false; \
}

static void optional_local_echo(char c)
{
    if (!option.local_echo)
        return;
    print(c);
    fflush(stdout);
    if (option.log)
        log_write(c);
}


inline static bool is_valid_bin(char c) 
{
    return (c >= '0' && c <= '1');
}

inline static bool is_valid_hex(char c) 
{
    return ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
}

inline static bool is_valid_dec(char c) 
{
    return (c >= '0' && c <= '9');
}
    
static void output_hex(char c)
{
    hex_chars[hex_char_index++] = c;

    if(hex_char_index == 2){
        unsigned char hex_value = (unsigned char)strtol(hex_chars, NULL, 16); 
        hex_char_index = 0;
        
        optional_local_echo(hex_value);
        
        ssize_t status = write(fd, &hex_value, 1);
        if (status < 0){
            warning_printf("Could not write to tty device");
        } else {
            tx_total++;
        }
            
        fsync(fd);
    }
}

static void print_hex(char c)
{
    if (((c == '\n') || (c == '\r')) && option.newline_in_hex)
        printf("%c", c);
    else
        printf("%02x ", (unsigned char) c);

    fflush(stdout);
}

static void print_normal(char c)
{
    putchar(c);
    fflush(stdout);
}

static void send_line(const char * line)
{
    int len = strlen(line);
    
    if(option.local_echo){
        fprintf(stdout, "\r\n");
        for(int i = 0; i < len; i++){
            optional_local_echo(line[i]);
        }
    
        if(option.no_newline_in_line_edit){
            fprintf(stdout, "\r\n");
        }
    } else {
    	get_line(0);
    }

    ssize_t status = write(fd, line, len);
    if (status < 0){
        warning_printf("Could not write to tty device");
    } else {
        tx_total += len;
    }
}

static void parse_line(const char * line)
{
    int len = strlen(line);
    
    if(len == 0){
        return; 
    }
    
    /* escaped characters will collapse */
    if(len > LINE_SIZE - 3){
        warning_printf("Line is too long (%d): %s", len, line);
        return;
    }

    parsed_line_ctr = 0;
    memset(parsed_line, 0, LINE_SIZE);
    
    /* handle commands */
    if(len == 2 && line[0] == ':'){
        switch(line[1]){
        case '?':
                tio_printf("Commands:");
                tio_printf(":?   List available commands");
                tio_printf(":q   Quit");
                tio_printf(":v   Show version");
                tio_printf("::   Send ':'");
                break;
         
         case 'q':
                handle_command_sequence(KEY_Q, KEY_CTRL_T, NULL, NULL);
                break;
                
         case 'v':
                handle_command_sequence(KEY_V, KEY_CTRL_T, NULL, NULL);
                break;
                
         case ':':
                parsed_line[parsed_line_ctr++] = ':';
                break;
                
         default:
                warning_printf("Unknown command: %c", line[1]);
                return;
                
        }
    /* interpret line */
    } else {
        int characters_to_read = 0;
        char buffer[ESCAPED_BUFFER_SIZE];
        unsigned int buffer_ctr = 0;
        int base = 10;
        memset(buffer, 0, ESCAPED_BUFFER_SIZE);
        
        unsigned long value;
        
        /* handle escape characters */
        for(unsigned int i = 0; i < len; i++){
            if(characters_to_read > 0){
                if(base == 16 && !is_valid_hex(line[i])){
                    warning_printf("Invalid hexadecimal character: %c", line[i]);
                    return;
                } else if(base == 10 && !is_valid_dec(line[i])){
                    warning_printf("Invalid decimal character: %c", line[i]);
                    return;
                } else if(base == 2 && !is_valid_bin(line[i])){
                    warning_printf("Invalid binary character: %c", line[i]);
                    return;
                } else {
                    buffer[buffer_ctr++] = line[i];

                    if(buffer_ctr == characters_to_read){
                        value = (unsigned long)strtol(buffer, NULL, base);
                  
                        if(value > 255){
                            warning_printf("Value is too large for a byte: %lu", value);
                            return;
                        }

                        parsed_line[parsed_line_ctr++] = (unsigned char)value;
                   
                        memset(buffer, 0, ESCAPED_BUFFER_SIZE);
                        buffer_ctr = 0;
                        characters_to_read = 0;
                    } 
                 }
            } else {
                if(line[i] == '\\'){
                    if(i + 1 < len && line[i+1] == '\\'){
                        parsed_line[parsed_line_ctr++] = '\\';
                        i++;
                    } else if(i + 1 < len && line[i+1] == 'x'){
                        base = 16;
                        characters_to_read = 2;
                        i++;
                    } else if(i + 1 < len && line[i+1] == 'd'){
                        base = 10;
                        characters_to_read = 3;
                        i++;
                    } else if(i + 1 < len && line[i+1] == 'b'){
                        base = 2;
                        characters_to_read = 8;
                        i++;
                    } else if(i + 1 < len){
                        warning_printf("Unknown base: %c", line[i+1]);
                        return;
                    } else {
                        warning_printf("Invalid escape: %s", line);
                        return;
                    }
                } else {
                    parsed_line[parsed_line_ctr++] = line[i];
                }
            }
        }
        
        if(characters_to_read != 0){
            warning_printf("Unescaped line: %s", line);
            return;
        }
        
        if(option.no_newline_in_line_edit == false){
            parsed_line[parsed_line_ctr++] = '\r';
            parsed_line[parsed_line_ctr++] = '\n'; 
        }
        
        parsed_line[parsed_line_ctr++] = '\0';
    }

	send_line(parsed_line);
}

static void toggle_line(const char *line_name, int mask)
{
    int state;

    if (ioctl(fd, TIOCMGET, &state) < 0)
    {
        error_printf("Could not get line state: %s", strerror(errno));
    }
    else
    {
        if (state & mask)
        {
            state &= ~mask;
            tio_printf("set %s to LOW", line_name);
        }
        else
        {
            state |= mask;
            tio_printf("set %s to HIGH", line_name);
        }
        if (ioctl(fd, TIOCMSET, &state) < 0)
            error_printf("Could not set line state: %s", strerror(errno));
    }
}

void handle_command_sequence(char input_char, char previous_char, char *output_char, bool *forward)
{
    char unused_char;
    bool unused_bool;
    int state;

    /* Ignore unused arguments */
    if (output_char == NULL)
        output_char = &unused_char;

    if (forward == NULL)
        forward = &unused_bool;

    /* Handle escape key commands */
    if (previous_char == KEY_CTRL_T)
    {
        /* Do not forward input char to output by default */
        *forward = false;

        switch (input_char)
        {
            case KEY_QUESTION:
                tio_printf("Key commands:");
                tio_printf(" ctrl-t ?   List available key commands");
                tio_printf(" ctrl-t b   Send break");
                tio_printf(" ctrl-t c   Show configuration");
                tio_printf(" ctrl-t e   Toggle local echo mode");
                tio_printf(" ctrl-t h   Toggle hexadecimal mode");
                tio_printf(" ctrl-t l   Clear screen");
                tio_printf(" ctrl-t q   Quit");
                tio_printf(" ctrl-t s   Show statistics");
                tio_printf(" ctrl-t t   Send ctrl-t key code");
                tio_printf(" ctrl-t T   Toggle timestamps");
                tio_printf(" ctrl-t L   Show lines");
                tio_printf(" ctrl-t d   Toggle DTR");
                tio_printf(" ctrl-t r   Toggle RTS");
                tio_printf(" ctrl-t v   Show version");
                break;

            case KEY_SHIFT_L:
                if (ioctl(fd, TIOCMGET, &state) < 0)
                {
                    error_printf("Could not get line state: %s", strerror(errno));
                    break;
                }
                tio_printf("Lines state:");
                tio_printf(" DTR: %s", (state & TIOCM_DTR) ? "HIGH" : "LOW");
                tio_printf(" RTS: %s", (state & TIOCM_RTS) ? "HIGH" : "LOW");
                tio_printf(" CTS: %s", (state & TIOCM_CTS) ? "HIGH" : "LOW");
                tio_printf(" DSR: %s", (state & TIOCM_DSR) ? "HIGH" : "LOW");
                tio_printf(" DCD: %s", (state & TIOCM_CD) ? "HIGH" : "LOW");
                tio_printf(" RI : %s", (state & TIOCM_RI) ? "HIGH" : "LOW");
                break;
            case KEY_D:
                toggle_line("DTR", TIOCM_DTR);
                break;

            case KEY_R:
                toggle_line("RTS", TIOCM_RTS);
                break;

            case KEY_B:
                tcsendbreak(fd, 0);
                break;

            case KEY_C:
                tio_printf("Configuration:");
                tio_printf(" TTY device: %s", option.tty_device);
                tio_printf(" Baudrate: %u", option.baudrate);
                tio_printf(" Databits: %d", option.databits);
                tio_printf(" Flow: %s", option.flow);
                tio_printf(" Stopbits: %d", option.stopbits);
                tio_printf(" Parity: %s", option.parity);
                tio_printf(" Local Echo: %s", option.local_echo ? "yes":"no");
                tio_printf(" Timestamps: %s", option.timestamp ? "yes" : "no");
                tio_printf(" Output delay: %d", option.output_delay);
                if (option.map[0] != 0)
                    tio_printf(" Map flags: %s", option.map);
                if (option.log)
                    tio_printf(" Log file: %s", option.log_filename);
                break;

            case KEY_E:
                option.local_echo = !option.local_echo;
                break;

            case KEY_H:
                /* Toggle hexadecimal printing mode */
                if (print_mode == NORMAL)
                {
                    print = print_hex;
                    print_mode = HEX;
                    tio_printf("Switched to hexadecimal mode");
                }
                else
                {
                    print = print_normal;
                    print_mode = NORMAL;
                    tio_printf("Switched to normal mode");
                }
                break;

            case KEY_L:
                /* Clear screen using ANSI/VT100 escape code */
                printf("\033c");
                fflush(stdout);
                break;

            case KEY_Q:
                /* Exit upon ctrl-t q sequence */
                exit(EXIT_SUCCESS);

            case KEY_S:
                /* Show tx/rx statistics upon ctrl-t s sequence */
                tio_printf("Statistics:");
                tio_printf(" Sent %lu bytes, received %lu bytes", tx_total, rx_total);
                break;

            case KEY_T:
                /* Send ctrl-t key code upon ctrl-t t sequence */
                *output_char = KEY_CTRL_T;
                *forward = true;
                break;

            case KEY_SHIFT_T:
                option.timestamp = !option.timestamp;
                break;

            case KEY_V:
                tio_printf("tio v%s", VERSION);
                break;

            default:
                /* Ignore unknown ctrl-t escaped keys */
                break;
        }
    }
}

void stdin_configure(void)
{
    int status;

    /* Save current stdin settings */
    if (tcgetattr(STDIN_FILENO, &stdin_old) < 0)
    {
        error_printf("Saving current stdin settings failed");
        exit(EXIT_FAILURE);
    }

    /* Prepare new stdin settings */
    memcpy(&stdin_new, &stdin_old, sizeof(stdin_old));

    /* Reconfigure stdin (RAW configuration) */
    stdin_new.c_iflag &= ~(ICRNL); // Do not translate CR -> NL on input
    stdin_new.c_oflag &= ~(OPOST);
    stdin_new.c_lflag &= ~(ECHO|ICANON|ISIG|ECHOE|ECHOK|ECHONL);

    /* Control characters */
    stdin_new.c_cc[VTIME] = 0; /* Inter-character timer unused */
    stdin_new.c_cc[VMIN]  = 1; /* Blocking read until 1 character received */

    /* Activate new stdin settings */
    status = tcsetattr(STDIN_FILENO, TCSANOW, &stdin_new);
    if (status == -1)
    {
        error_printf("Could not apply new stdin settings (%s)", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Make sure we restore old stdin settings on exit */
    atexit(&stdin_restore);
}

void stdin_restore(void)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &stdin_old);
}

void stdout_configure(void)
{
    int status;

    /* Disable line buffering in stdout. This is necessary if we
     * want things like local echo to work correctly. */
    setbuf(stdout, NULL);

    /* Save current stdout settings */
    if (tcgetattr(STDOUT_FILENO, &stdout_old) < 0)
    {
        error_printf("Saving current stdio settings failed");
        exit(EXIT_FAILURE);
    }

    /* Prepare new stdout settings */
    memcpy(&stdout_new, &stdout_old, sizeof(stdout_old));

    /* Reconfigure stdout (RAW configuration) */
    stdout_new.c_oflag &= ~(OPOST);
    stdout_new.c_lflag &= ~(ECHO|ICANON|ISIG|ECHOE|ECHOK|ECHONL);

    /* Control characters */
    stdout_new.c_cc[VTIME] = 0; /* Inter-character timer unused */
    stdout_new.c_cc[VMIN]  = 1; /* Blocking read until 1 character received */

    /* Activate new stdout settings */
    status = tcsetattr(STDOUT_FILENO, TCSANOW, &stdout_new);
    if (status == -1)
    {
        error_printf("Could not apply new stdout settings (%s)", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Print launch hints */
    tio_printf("tio v%s", VERSION);
    if(option.line_edit)
    {
        tio_printf("Type :q to quit, :? for help.");
    } else {
        tio_printf("Press ctrl-t q to quit, ctrl-t ? for help.");
    }

    /* At start use normal print function */
    print = print_normal;

    /* Make sure we restore old stdout settings on exit */
    atexit(&stdout_restore);
}

void stdout_restore(void)
{
    tcsetattr(STDOUT_FILENO, TCSANOW, &stdout_old);
}

void tty_configure(void)
{
    bool token_found = true;
    char *token = NULL;
    char *buffer;
    int status;
    speed_t baudrate;

    memset(&tio, 0, sizeof(tio));

    /* Set speed */
    switch (option.baudrate)
    {
        /* The macro below expands into switch cases autogenerated by the
         * configure script. Each switch case verifies and configures the baud
         * rate and is of the form:
         *
         * case $baudrate: baudrate = B$baudrate; break;
         *
         * Only switch cases for baud rates detected supported by the host
         * system are inserted.
         *
         * To see which baud rates are being probed see configure.ac
         */
        AUTOCONF_BAUDRATE_CASES

        default:
#ifdef HAVE_TERMIOS2
            standard_baudrate = false;
            break;
#else
            error_printf("Invalid baud rate");
            exit(EXIT_FAILURE);
#endif
    }

    if (standard_baudrate)
    {
        // Set input speed
        status = cfsetispeed(&tio, baudrate);
        if (status == -1)
        {
            error_printf("Could not configure input speed (%s)", strerror(errno));
            exit(EXIT_FAILURE);
        }

        // Set output speed
        status = cfsetospeed(&tio, baudrate);
        if (status == -1)
        {
            error_printf("Could not configure output speed (%s)", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    /* Set databits */
    tio.c_cflag &= ~CSIZE;
    switch (option.databits)
    {
        case 5:
            tio.c_cflag |= CS5;
            break;
        case 6:
            tio.c_cflag |= CS6;
            break;
        case 7:
            tio.c_cflag |= CS7;
            break;
        case 8:
            tio.c_cflag |= CS8;
            break;
        default:
            error_printf("Invalid data bits");
            exit(EXIT_FAILURE);
    }

    /* Set flow control */
    if (strcmp("hard", option.flow) == 0)
    {
        tio.c_cflag |= CRTSCTS;
        tio.c_iflag &= ~(IXON | IXOFF | IXANY);
    }
    else if (strcmp("soft", option.flow) == 0)
    {
        tio.c_cflag &= ~CRTSCTS;
        tio.c_iflag |= IXON | IXOFF;
    }
    else if (strcmp("none", option.flow) == 0)
    {
        tio.c_cflag &= ~CRTSCTS;
        tio.c_iflag &= ~(IXON | IXOFF | IXANY);
    }
    else
    {
        error_printf("Invalid flow control");
        exit(EXIT_FAILURE);
    }

    /* Set stopbits */
    switch (option.stopbits)
    {
        case 1:
            tio.c_cflag &= ~CSTOPB;
            break;
        case 2:
            tio.c_cflag |= CSTOPB;
            break;
        default:
            error_printf("Invalid stop bits");
            exit(EXIT_FAILURE);
    }

    /* Set parity */
    if (strcmp("odd", option.parity) == 0)
    {
        tio.c_cflag |= PARENB;
        tio.c_cflag |= PARODD;
    }
    else if (strcmp("even", option.parity) == 0)
    {
        tio.c_cflag |= PARENB;
        tio.c_cflag &= ~PARODD;
    }
    else if (strcmp("none", option.parity) == 0)
        tio.c_cflag &= ~PARENB;
    else
    {
        error_printf("Invalid parity");
        exit(EXIT_FAILURE);
    }

    /* Control, input, output, local modes for tty device */
    tio.c_cflag |= CLOCAL | CREAD;
    tio.c_oflag = 0;
    tio.c_lflag = 0;

    /* Control characters */
    tio.c_cc[VTIME] = 0; // Inter-character timer unused
    tio.c_cc[VMIN]  = 1; // Blocking read until 1 character received

    /* Configure any specified input or output mappings */
    buffer = strdup(option.map);
    while (token_found == true)
    {
        if (token == NULL)
            token = strtok(buffer,",");
        else
            token = strtok(NULL, ",");

        if (token != NULL)
        {
            if (strcmp(token,"INLCR") == 0)
                tio.c_iflag |= INLCR;
            else if (strcmp(token,"IGNCR") == 0)
                tio.c_iflag |= IGNCR;
            else if (strcmp(token,"ICRNL") == 0)
                tio.c_iflag |= ICRNL;
            else if (strcmp(token,"OCRNL") == 0)
                map_o_cr_nl = true;
            else if (strcmp(token,"ODELBS") == 0)
                map_o_del_bs = true;
            else if (strcmp(token,"INLCRNL") == 0)
                map_i_nl_crnl = true;
            else if (strcmp(token, "ONLCRNL") == 0)
                map_o_nl_crnl = true;
            else
            {
                printf("Error: Unknown mapping flag %s\n", token);
                exit(EXIT_FAILURE);
            }
        }
        else
            token_found = false;
    }
    free(buffer);
}

void tty_wait_for_device(void)
{
    fd_set rdfs;
    int    status;
    struct timeval tv;
    static char input_char, previous_char = 0;
    static bool first = true;
    static int last_errno = 0;

    /* Loop until device pops up */
    while (true)
    {
        if (first)
        {
            /* Don't wait first time */
            tv.tv_sec = 0;
            tv.tv_usec = 1;
            first = false;
        } else
        {
            /* Wait up to 1 second */
            tv.tv_sec = 1;
            tv.tv_usec = 0;
        }

        FD_ZERO(&rdfs);
        FD_SET(STDIN_FILENO, &rdfs);

        /* Block until input becomes available or timeout */
        status = select(STDIN_FILENO + 1, &rdfs, NULL, NULL, &tv);
        if (status > 0)
        {
            /* Input from stdin ready */

            /* Read one character */
            status = read(STDIN_FILENO, &input_char, 1);
            if (status <= 0)
            {
                error_printf("Could not read from stdin");
                exit(EXIT_FAILURE);
            }

            /* Handle commands */
            handle_command_sequence(input_char, previous_char, NULL, NULL);

            previous_char = input_char;

        } else if (status == -1)
        {
            error_printf("select() failed (%s)", strerror(errno));
            exit(EXIT_FAILURE);
        }

        /* Test for accessible device file */
        int rc = access(option.tty_device, R_OK);
        if (rc == 0) {
            last_errno = 0;
            return;
        }
        else if (last_errno != errno) {
            tio_printf("%s: %s. Waiting...", option.tty_device, strerror(errno));
            last_errno = errno;
        }
    }
}

void tty_disconnect(void)
{
    if (connected)
    {
        tio_printf("Disconnected");
        flock(fd, LOCK_UN);
        close(fd);
        connected = false;
    }
}

void tty_restore(void)
{
    tcsetattr(fd, TCSANOW, &tio_old);

    if (connected)
        tty_disconnect();
}

int tty_connect(void)
{
    fd_set rdfs;           /* Read file descriptor set */
    int    maxfd;          /* Maximum file descriptor used */
    char   input_char, output_char;
    static char previous_char = 0;
    static bool first = true;
    int    status;
    time_t next_timestamp = 0;
    char*  now = NULL;
    char*  raw_line = NULL;

    /* Open tty device */
#ifdef __APPLE__
    fd = open(option.tty_device, O_RDWR | O_NOCTTY | O_NONBLOCK );
#else
    fd = open(option.tty_device, O_RDWR | O_NOCTTY);
#endif
    if (fd < 0)
    {
        error_printf_silent("Could not open tty device (%s)", strerror(errno));
        goto error_open;
    }

    /* Make sure device is of tty type */
    if (!isatty(fd))
    {
        error_printf("Not a tty device");
        exit(EXIT_FAILURE);;
    }

    /* Lock device file */
    status = flock(fd, LOCK_EX | LOCK_NB);
    if ((status == -1) && (errno == EWOULDBLOCK))
    {
        error_printf("Device file is locked by another process");
        exit(EXIT_FAILURE);
    }

    /* Flush stale I/O data (if any) */
    tcflush(fd, TCIOFLUSH);

    /* Print connect status */
    tio_printf("Connected");
    connected = true;
    tainted = false;

    if (option.timestamp)
        next_timestamp = time(NULL);
        
    if (option.hex_mode)
    {
        print = print_hex;
        print_mode = HEX;
        tio_printf("Switched to hexadecimal mode");
    }
    else
    {
        print = print_normal;
        print_mode = NORMAL;
        tio_printf("Switched to normal mode");
    }

    /* Save current port settings */
    if (tcgetattr(fd, &tio_old) < 0)
        goto error_tcgetattr;

    /* Make sure we restore tty settings on exit */
    if (first)
    {
        atexit(&tty_restore);
        first = false;
    }

    /* Activate new port settings */
    status = tcsetattr(fd, TCSANOW, &tio);
    if (status == -1)
    {
        error_printf_silent("Could not apply port settings (%s)", strerror(errno));
        goto error_tcsetattr;
    }

#ifdef HAVE_TERMIOS2
    if (!standard_baudrate)
    {
        if (setspeed2(fd, option.baudrate) != 0)
        {
            error_printf_silent("Could not set baudrate speed (%s)", strerror(errno));
            goto error_setspeed2;
        }
    }
#endif

    maxfd = MAX(fd, STDIN_FILENO) + 1;  /* Maximum bit entry (fd) to test */
    
    /* display initial prompt */
    if(option.line_edit)
    {
    	get_line(0);
    }

	/* Input loop */
	while (true)
	{
	    FD_ZERO(&rdfs);
	    FD_SET(fd, &rdfs);
	    FD_SET(STDIN_FILENO, &rdfs);

	    /* Block until input becomes available */
	    status = select(maxfd, &rdfs, NULL, NULL, NULL);
	    if (status > 0)
	    {
			if (FD_ISSET(fd, &rdfs))
			{
				/* Input from tty device ready */
				if (read(fd, &input_char, 1) > 0)
				{
				    /* Update receive statistics */
				    rx_total++;

				    /* Print timestamp on new line, if desired. */
				    if (next_timestamp && input_char != '\n' && input_char != '\r')
				    {
				        now = current_time();
				        fprintf(stdout, ANSI_COLOR_GRAY "[%s] " ANSI_COLOR_RESET, now);
				        if (option.log)
				        {
				            log_write('[');
				            while (*now != '\0')
				            {
				                log_write(*now);
				                ++now;
				            }
				            log_write(']');
				            log_write(' ');
				        }
				        next_timestamp = 0;
				    }

					// FIXME: make it better in line edit mode
				    /* Map input character */
				    if ((input_char == '\n') && (map_i_nl_crnl))
				    {
				        print('\r');
				        print('\n');
				        if (option.timestamp)
				            next_timestamp = time(NULL);
				    } else
				    {
				        /* Print received tty character to stdout */
				        print(input_char);
				    }
				    fflush(stdout);

				    /* Write to log */
				    if (option.log)
				        log_write(input_char);

				    tainted = true;

				    if (input_char == '\n' && option.timestamp)
				        next_timestamp = time(NULL);
				} else
				{
				    /* Error reading - device is likely unplugged */
				    error_printf_silent("Could not read from tty device");
				    goto error_read;
				}
			}
			
			if (FD_ISSET(STDIN_FILENO, &rdfs))
			{
				bool forward = true;

				/* Input from stdin ready */
				status = read(STDIN_FILENO, &input_char, 1);
				if (status <= 0)
				{
				    error_printf_silent("Could not read from stdin");
				    goto error_read;
				}

				if(option.line_edit){
					if((raw_line = get_line(input_char)) != NULL){
                		add_to_history(raw_line);
                		
                		parse_line(raw_line);
                		
                		free_line(raw_line);
            		}
				} else {
					/* Forward input to output except ctrl-t key */
					output_char = input_char;
					if (input_char == KEY_CTRL_T)
						forward = false;

					/* Handle commands */
					handle_command_sequence(input_char, previous_char, &output_char, &forward);

					if (forward)
					{
						if(print_mode == HEX){
						    if(!is_valid_hex(input_char)){
						        warning_printf("Invalid hex character: '%c' (0x%02x)", input_char, input_char);
						        hex_char_index = 0;
						        continue;        
						    }
						}

						/* Map output character */
						if ((output_char == 127) && (map_o_del_bs))
						    output_char = '\b';
						if ((output_char == '\r') && (map_o_cr_nl))
						    output_char = '\n';

						/* Map newline character */
						if ((output_char == '\n') && (map_o_nl_crnl)) {
						    char r = '\r';

						    optional_local_echo(r);
						    status = write(fd, &r, 1);
						    if (status < 0)
						        warning_printf("Could not write to tty device");

						    tx_total++;
						    delay(option.output_delay);
						}
						
						
						if(print_mode == HEX){
						    output_hex(output_char);
						} else {
						    /* Send output to tty device */
						    optional_local_echo(output_char);
						    
						    status = write(fd, &output_char, 1);
						    if (status < 0)
						        warning_printf("Could not write to tty device");
						    fsync(fd);

						    /* Update transmit statistics */
						    tx_total++;
						}

						/* Insert output delay */
						delay(option.output_delay);
					}

					/* Save previous key */
					previous_char = input_char;

				}
			}
	    } else if (status == -1)
	    {
			error_printf("Error: select() failed (%s)", strerror(errno));
			exit(EXIT_FAILURE);
	    }
	}

    return TIO_SUCCESS;

#ifdef HAVE_TERMIOS2
error_setspeed2:
#endif
error_tcsetattr:
error_tcgetattr:
error_read:
    tty_disconnect();
error_open:
    return TIO_ERROR;
}
