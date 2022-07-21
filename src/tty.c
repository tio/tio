/*
 * tio - a simple serial terminal I/O tool
 *
 * Copyright (c) 2014-2022  Martin Lund
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
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <termios.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>
#include <dirent.h>
#include "config.h"
#include "configfile.h"
#include "tty.h"
#include "print.h"
#include "options.h"
#include "misc.h"
#include "log.h"
#include "error.h"
#include "socket.h"
#include "setspeed.h"

#ifdef __APPLE__
#define PATH_SERIAL_DEVICES "/dev/"
#else
#define PATH_SERIAL_DEVICES "/dev/serial/by-id/"
#endif

#ifndef CMSPAR
#define CMSPAR   010000000000
#endif

#define KEY_0 0x30
#define KEY_1 0x31
#define KEY_2 0x32
#define KEY_3 0x33
#define KEY_4 0x34
#define KEY_5 0x35
#define KEY_QUESTION 0x3f
#define KEY_B 0x62
#define KEY_C 0x63
#define KEY_E 0x65
#define KEY_G 0x67
#define KEY_H 0x68
#define KEY_L 0x6C
#define KEY_P 0x70
#define KEY_Q 0x71
#define KEY_S 0x73
#define KEY_T 0x74
#define KEY_U 0x55
#define KEY_V 0x76
#define KEY_SHIFT_L 0x4C

#define NORMAL 0
#define HEX 1

enum line_mode_t
{
    LINE_OFF,
    LINE_TOGGLE,
    LINE_PULSE
};

bool interactive_mode = true;

static struct termios tio, tio_old, stdout_new, stdout_old, stdin_new, stdin_old;
static unsigned long rx_total = 0, tx_total = 0;
static bool connected = false;
static bool print_mode = NORMAL;
static bool standard_baudrate = true;
static void (*print)(char c);
static int fd;
static bool map_i_nl_crnl = false;
static bool map_o_cr_nl = false;
static bool map_o_nl_crnl = false;
static bool map_o_del_bs = false;
static bool map_o_ltu = false;
static char hex_chars[2];
static unsigned char hex_char_index = 0;
static char tty_buffer[BUFSIZ*2];
static size_t tty_buffer_count = 0;
static char *tty_buffer_write_ptr = tty_buffer;

static void optional_local_echo(char c)
{
    if (!option.local_echo)
    {
        return;
    }
    print(c);
    if (option.log)
    {
        log_putc(c);
    }
}

inline static bool is_valid_hex(char c)
{
    return ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
}

inline static unsigned char char_to_nibble(char c)
{
    if(c >= '0' && c <= '9')
    {
        return c - '0';
    }
    else if (c >= 'a' && c <= 'f')
    {
        return c - 'a' + 10;
    }
    else if (c >= 'A' && c <= 'F')
    {
        return c - 'A' + 10;
    }
    else
    {
        return 0;
    }
}

void tty_flush(int fd)
{
    ssize_t count;

    while (tty_buffer_count > 0)
    {
        count = write(fd, tty_buffer, tty_buffer_count);
        if (count < 0)
        {
            // Error
            tio_debug_printf("Write error while flushing tty buffer (%s)", strerror(errno));
            break;
        }
        tty_buffer_count -= count;
        fsync(fd);
    }

    // Reset
    tty_buffer_write_ptr = tty_buffer;
    tty_buffer_count = 0;
    fsync(fd);
}

ssize_t tty_write(int fd, const void *buffer, size_t count)
{
    ssize_t retval = 0, bytes_written = 0;
    size_t i;

    if (map_o_ltu)
    {
        // Convert lower case to upper case
        for (i = 0; i<count; i++)
        {
            *((unsigned char*)buffer+i) = toupper(*((unsigned char*)buffer+i));
        }
    }

    if (option.output_delay || option.output_line_delay)
    {
        // Write byte by byte with output delay
        for (i=0; i<count; i++)
        {
            retval = write(fd, buffer, 1);
            if (retval < 0)
            {
                // Error
                tio_debug_printf("Write error (%s)", strerror(errno));
                break;
            }
            bytes_written += retval;

            if (option.output_line_delay && *(unsigned char*)buffer == '\r')
            {
                delay(option.output_line_delay);
            }

            fsync(fd);

            if (option.output_delay)
            {
                delay(option.output_delay);
            }
        }
    }
    else
    {
        // Flush tty buffer if too full
        if ((tty_buffer_count + count) > BUFSIZ)
        {
            tty_flush(fd);
        }

        // Copy bytes to tty write buffer
        memcpy(tty_buffer_write_ptr, buffer, count);
        tty_buffer_write_ptr += count;
        tty_buffer_count += count;
        bytes_written = count;
    }

    return bytes_written;
}

static void output_hex(char c)
{
    hex_chars[hex_char_index++] = c;

    if (hex_char_index == 2)
    {
        unsigned char hex_value = char_to_nibble(hex_chars[0]) << 4 | (char_to_nibble(hex_chars[1]) & 0x0F);
        hex_char_index = 0;

        optional_local_echo(hex_value);

        ssize_t status = tty_write(fd, &hex_value, 1);
        if (status < 0)
        {
            tio_warning_printf("Could not write to tty device");
        }
        else
        {
            tx_total++;
        }
    }
}

static void toggle_line(const char *line_name, int mask, enum line_mode_t line_mode)
{
    int state;

    if (line_mode == LINE_TOGGLE)
    {
        // Toggle line
        if (ioctl(fd, TIOCMGET, &state) < 0)
        {
            tio_warning_printf("Could not get line state (%s)", strerror(errno));
        }
        else
        {
            if (state & mask)
            {
                state &= ~mask;
                tio_printf("Setting %s to LOW", line_name);
            }
            else
            {
                state |= mask;
                tio_printf("Setting %s to HIGH", line_name);
            }
            if (ioctl(fd, TIOCMSET, &state) < 0)
                tio_warning_printf("Could not set line state (%s)", strerror(errno));
        }
    } else if (line_mode == LINE_PULSE)
    {
        int duration = 0;
        // Pulse line
        toggle_line(line_name, mask, LINE_TOGGLE);
        switch (mask)
        {
            case TIOCM_DTR:
                duration = option.dtr_pulse_duration;
                break;
            case TIOCM_RTS:
                duration = option.rts_pulse_duration;
                break;
            case TIOCM_CTS:
                duration = option.cts_pulse_duration;
                break;
            case TIOCM_DSR:
                duration = option.dsr_pulse_duration;
                break;
            case TIOCM_CD:
                duration = option.dcd_pulse_duration;
                break;
            case TIOCM_RI:
                duration = option.ri_pulse_duration;
                break;
            default:
                duration = 0;
                break;
        }
        if (duration > 0)
        {
            tio_printf("Waiting %d ms", duration);
            delay(duration);
        }
        toggle_line(line_name, mask, LINE_TOGGLE);
    }
}

void handle_command_sequence(char input_char, char previous_char, char *output_char, bool *forward)
{
    char unused_char;
    bool unused_bool;
    int state;
    static enum line_mode_t line_mode = LINE_OFF;

    /* Ignore unused arguments */
    if (output_char == NULL)
    {
        output_char = &unused_char;
    }

    if (forward == NULL)
    {
        forward = &unused_bool;
    }

    if (line_mode)
    {
        // Handle line toggle number action
        *forward = false;
        switch (input_char)
        {
            case KEY_0:
                toggle_line("DTR", TIOCM_DTR, line_mode);
                break;
            case KEY_1:
                toggle_line("RTS", TIOCM_RTS, line_mode);
                break;
            case KEY_2:
                toggle_line("CTS", TIOCM_CTS, line_mode);
                break;
            case KEY_3:
                toggle_line("DSR", TIOCM_DSR, line_mode);
                break;
            case KEY_4:
                toggle_line("DCD", TIOCM_CD, line_mode);
                break;
            case KEY_5:
                toggle_line("RI", TIOCM_RI, line_mode);
                break;
            default:
                tio_warning_printf("Invalid line number");
                break;
        }

        line_mode = LINE_OFF;

        return;
    }

    /* Handle escape key commands */
    if (previous_char == option.prefix_code)
    {
        /* Do not forward input char to output by default */
        *forward = false;

        switch (input_char)
        {
            case KEY_QUESTION:
                tio_printf("Key commands:");
                tio_printf(" ctrl-%c ?   List available key commands", option.prefix_key);
                tio_printf(" ctrl-%c b   Send break", option.prefix_key);
                tio_printf(" ctrl-%c c   Show configuration", option.prefix_key);
                tio_printf(" ctrl-%c e   Toggle local echo mode", option.prefix_key);
                tio_printf(" ctrl-%c g   Toggle serial port line", option.prefix_key);
                tio_printf(" ctrl-%c h   Toggle hexadecimal mode", option.prefix_key);
                tio_printf(" ctrl-%c l   Clear screen", option.prefix_key);
                tio_printf(" ctrl-%c L   Show line states", option.prefix_key);
                tio_printf(" ctrl-%c p   Pulse serial port line", option.prefix_key);
                tio_printf(" ctrl-%c q   Quit", option.prefix_key);
                tio_printf(" ctrl-%c s   Show statistics", option.prefix_key);
                tio_printf(" ctrl-%c t   Toggle line timestamp mode", option.prefix_key);
                tio_printf(" ctrl-%c U   Toggle conversion to uppercase on output", option.prefix_key);
                tio_printf(" ctrl-%c v   Show version", option.prefix_key);
                break;

            case KEY_SHIFT_L:
                if (ioctl(fd, TIOCMGET, &state) < 0)
                {
                    tio_warning_printf("Could not get line state (%s)", strerror(errno));
                    break;
                }
                tio_printf("Line states:");
                tio_printf(" DTR: %s", (state & TIOCM_DTR) ? "HIGH" : "LOW");
                tio_printf(" RTS: %s", (state & TIOCM_RTS) ? "HIGH" : "LOW");
                tio_printf(" CTS: %s", (state & TIOCM_CTS) ? "HIGH" : "LOW");
                tio_printf(" DSR: %s", (state & TIOCM_DSR) ? "HIGH" : "LOW");
                tio_printf(" DCD: %s", (state & TIOCM_CD) ? "HIGH" : "LOW");
                tio_printf(" RI : %s", (state & TIOCM_RI) ? "HIGH" : "LOW");
                break;

            case KEY_G:
                tio_printf("Please enter which serial line number to toggle:");
                tio_printf(" DTR (0)");
                tio_printf(" RTS (1)");
                tio_printf(" CTS (2)");
                tio_printf(" DSR (3)");
                tio_printf(" DCD (4)");
                tio_printf(" RI  (5)");
                // Process next input character as part of the line toggle step
                line_mode = LINE_TOGGLE;
                break;

            case KEY_P:
                tio_printf("Please enter which serial line number to pulse:");
                tio_printf(" DTR (0)");
                tio_printf(" RTS (1)");
                tio_printf(" CTS (2)");
                tio_printf(" DSR (3)");
                tio_printf(" DCD (4)");
                tio_printf(" RI  (5)");
                // Process next input character as part of the line pulse step
                line_mode = LINE_PULSE;
                break;

            case KEY_B:
                tcsendbreak(fd, 0);
                break;

            case KEY_C:
                tio_printf("Configuration:");
                config_file_print();
                options_print();
                break;

            case KEY_E:
                option.local_echo = !option.local_echo;
                tio_printf("Switched local echo %s", option.local_echo ? "on" : "off");
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
                break;

            case KEY_Q:
                /* Exit upon ctrl-t q sequence */
                exit(EXIT_SUCCESS);

            case KEY_S:
                /* Show tx/rx statistics upon ctrl-t s sequence */
                tio_printf("Statistics:");
                tio_printf(" Sent %lu bytes", tx_total);
                tio_printf(" Received %lu bytes", rx_total);
                break;

            case KEY_T:
                option.timestamp += 1;
                switch (option.timestamp)
                {
                    case TIMESTAMP_NONE:
                        break;
                    case TIMESTAMP_24HOUR:
                        tio_printf("Switched to 24hour timestamp mode");
                        break;
                    case TIMESTAMP_24HOUR_START:
                        tio_printf("Switched to 24hour-start timestamp mode");
                        break;
                    case TIMESTAMP_24HOUR_DELTA:
                        tio_printf("Switched to 24hour-delta timestamp mode");
                        break;
                    case TIMESTAMP_ISO8601:
                        tio_printf("Switched to iso8601 timestamp mode");
                        break;
                    case TIMESTAMP_END:
                        option.timestamp = TIMESTAMP_NONE;
                        tio_printf("Switched timestamp off");
                        break;
                }
                break;

            case KEY_U:
                map_o_ltu = !map_o_ltu;
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

void stdin_restore(void)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &stdin_old);
}

void stdin_configure(void)
{
    int status;

    /* Save current stdin settings */
    if (tcgetattr(STDIN_FILENO, &stdin_old) < 0)
    {
        tio_error_printf("Saving current stdin settings failed");
        exit(EXIT_FAILURE);
    }

    /* Prepare new stdin settings */
    memcpy(&stdin_new, &stdin_old, sizeof(stdin_old));

    /* Reconfigure stdin (RAW configuration) */
    cfmakeraw(&stdin_new);

    /* Control characters */
    stdin_new.c_cc[VTIME] = 0; /* Inter-character timer unused */
    stdin_new.c_cc[VMIN]  = 1; /* Blocking read until 1 character received */

    /* Activate new stdin settings */
    status = tcsetattr(STDIN_FILENO, TCSANOW, &stdin_new);
    if (status == -1)
    {
        tio_error_printf("Could not apply new stdin settings (%s)", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Make sure we restore old stdin settings on exit */
    atexit(&stdin_restore);
}

void stdout_restore(void)
{
    tcsetattr(STDOUT_FILENO, TCSANOW, &stdout_old);
}

void stdout_configure(void)
{
    int status;

    /* Disable line buffering in stdout. This is necessary if we
     * want things like local echo to work correctly. */
    setvbuf(stdout, NULL, _IONBF, 0);

    /* Save current stdout settings */
    if (tcgetattr(STDOUT_FILENO, &stdout_old) < 0)
    {
        tio_error_printf("Saving current stdio settings failed");
        exit(EXIT_FAILURE);
    }

    /* Prepare new stdout settings */
    memcpy(&stdout_new, &stdout_old, sizeof(stdout_old));

    /* Reconfigure stdout (RAW configuration) */
    cfmakeraw(&stdout_new);

    /* Allow ^C / SIGINT (to allow termination when piping to tio) */
    if (!interactive_mode)
    {
        stdout_new.c_lflag |= ISIG;
    }

    /* Control characters */
    stdout_new.c_cc[VTIME] = 0; /* Inter-character timer unused */
    stdout_new.c_cc[VMIN]  = 1; /* Blocking read until 1 character received */

    /* Activate new stdout settings */
    status = tcsetattr(STDOUT_FILENO, TCSANOW, &stdout_new);
    if (status == -1)
    {
        tio_error_printf("Could not apply new stdout settings (%s)", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* At start use normal print function */
    print = print_normal;

    /* Make sure we restore old stdout settings on exit */
    atexit(&stdout_restore);
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
        /* The macro below expands into switch cases autogenerated by meson
         * configure. Each switch case verifies and configures the baud
         * rate and is of the form:
         *
         * case $baudrate: baudrate = B$baudrate; break;
         *
         * Only switch cases for baud rates detected supported by the host
         * system are inserted.
         *
         * To see which baud rates are being probed see meson.build
         */
        BAUDRATE_CASES

        default:
#if defined (HAVE_TERMIOS2) || defined (HAVE_IOSSIOSPEED)
            standard_baudrate = false;
            break;
#else
            tio_error_printf("Invalid baud rate");
            exit(EXIT_FAILURE);
#endif
    }

    if (standard_baudrate)
    {
        // Set input speed
        status = cfsetispeed(&tio, baudrate);
        if (status == -1)
        {
            tio_error_printf("Could not configure input speed (%s)", strerror(errno));
            exit(EXIT_FAILURE);
        }

        // Set output speed
        status = cfsetospeed(&tio, baudrate);
        if (status == -1)
        {
            tio_error_printf("Could not configure output speed (%s)", strerror(errno));
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
            tio_error_printf("Invalid data bits");
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
        tio_error_printf("Invalid flow control");
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
            tio_error_printf("Invalid stop bits");
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
    {
        tio.c_cflag &= ~PARENB;
    }
    else if ( strcmp("mark", option.parity) == 0)
    {
        tio.c_cflag |= PARENB;
        tio.c_cflag |= PARODD;
        tio.c_cflag |= CMSPAR;
    }
    else if ( strcmp("space", option.parity) == 0)
    {
        tio.c_cflag |= PARENB;
        tio.c_cflag &= ~PARODD;
        tio.c_cflag |= CMSPAR;
    }
    else
    {
        tio_error_printf("Invalid parity");
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
        {
            token = strtok(buffer,",");
        }
        else
        {
            token = strtok(NULL, ",");
        }

        if (token != NULL)
        {
            if (strcmp(token,"INLCR") == 0)
            {
                tio.c_iflag |= INLCR;
            }
            else if (strcmp(token,"IGNCR") == 0)
            {
                tio.c_iflag |= IGNCR;
            }
            else if (strcmp(token,"ICRNL") == 0)
            {
                tio.c_iflag |= ICRNL;
            }
            else if (strcmp(token,"OCRNL") == 0)
            {
                map_o_cr_nl = true;
            }
            else if (strcmp(token,"ODELBS") == 0)
            {
                map_o_del_bs = true;
            }
            else if (strcmp(token,"INLCRNL") == 0)
            {
                map_i_nl_crnl = true;
            }
            else if (strcmp(token, "ONLCRNL") == 0)
            {
                map_o_nl_crnl = true;
            }
            else if (strcmp(token, "OLTU") == 0)
            {
                map_o_ltu = true;
            }
            else
            {
                printf("Error: Unknown mapping flag %s\n", token);
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            token_found = false;
        }
    }
    free(buffer);
}

void tty_wait_for_device(void)
{
    fd_set rdfs;
    int    status;
    int    maxfd;
    struct timeval tv;
    static char input_char, previous_char = 0;
    static bool first = true;
    static int last_errno = 0;

    /* Loop until device pops up */
    while (true)
    {
        if (interactive_mode)
        {
            /* In interactive mode, while waiting for tty device, we need to
             * read from stdin to react on input key commands. */
            if (first)
            {
                /* Don't wait first time */
                tv.tv_sec = 0;
                tv.tv_usec = 1;
                first = false;
            }
            else
            {
                /* Wait up to 1 second for input */
                tv.tv_sec = 1;
                tv.tv_usec = 0;
            }

            FD_ZERO(&rdfs);
            FD_SET(STDIN_FILENO, &rdfs);
            maxfd = MAX(STDIN_FILENO, socket_add_fds(&rdfs, false));

            /* Block until input becomes available or timeout */
            status = select(maxfd + 1, &rdfs, NULL, NULL, &tv);
            if (status > 0)
            {
                if (FD_ISSET(STDIN_FILENO, &rdfs))
                {
                    /* Input from stdin ready */

                    /* Read one character */
                    status = read(STDIN_FILENO, &input_char, 1);
                    if (status <= 0)
                    {
                        tio_error_printf("Could not read from stdin");
                        exit(EXIT_FAILURE);
                    }

                    /* Handle commands */
                    handle_command_sequence(input_char, previous_char, NULL, NULL);

                    previous_char = input_char;
                }
                socket_handle_input(&rdfs, NULL);
            }
            else if (status == -1)
            {
                tio_error_printf("select() failed (%s)", strerror(errno));
                exit(EXIT_FAILURE);
            }
        }

        /* Test for accessible device file */
        status = access(option.tty_device, R_OK);
        if (status == 0)
        {
            last_errno = 0;
            return;
        }
        else if (last_errno != errno)
        {
            tio_warning_printf("Could not open tty device (%s)", strerror(errno));
            tio_printf("Waiting for tty device..");
            last_errno = errno;
        }

        if (!interactive_mode)
        {
            /* In non-interactive mode we do not need to handle input key
             * commands so we simply sleep 1 second between checking for
             * presence of tty device */
            sleep(1);
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
    {
        tty_disconnect();
    }
}

void forward_to_tty(int fd, char output_char)
{
    int status;

    /* Map output character */
    if ((output_char == 127) && (map_o_del_bs))
    {
        output_char = '\b';
    }
    if ((output_char == '\r') && (map_o_cr_nl))
    {
        output_char = '\n';
    }

    /* Map newline character */
    if ((output_char == '\n' || output_char == '\r') && (map_o_nl_crnl))
    {
        const char *crlf = "\r\n";

        optional_local_echo(crlf[0]);
        optional_local_echo(crlf[1]);
        status = tty_write(fd, crlf, 2);
        if (status < 0)
        {
            tio_warning_printf("Could not write to tty device");
        }

        tx_total += 2;
    }
    else
    {
        if (print_mode == HEX)
        {
            output_hex(output_char);
        }
        else
        {
            /* Send output to tty device */
            optional_local_echo(output_char);
            status = tty_write(fd, &output_char, 1);
            if (status < 0)
            {
                tio_warning_printf("Could not write to tty device");
            }

            /* Update transmit statistics */
            tx_total++;
        }
    }
}

int tty_connect(void)
{
    fd_set rdfs;           /* Read file descriptor set */
    int    maxfd;          /* Maximum file descriptor used */
    char   input_char, output_char;
    char   input_buffer[BUFSIZ];
    static char previous_char = 0;
    static bool first = true;
    int    status;
    bool   next_timestamp = false;
    char*  now = NULL;

    /* Open tty device */
    fd = open(option.tty_device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0)
    {
        tio_error_printf_silent("Could not open tty device (%s)", strerror(errno));
        goto error_open;
    }

    /* Make sure device is of tty type */
    if (!isatty(fd))
    {
        tio_error_printf("Not a tty device");
        exit(EXIT_FAILURE);;
    }

    /* Lock device file */
    status = flock(fd, LOCK_EX | LOCK_NB);
    if ((status == -1) && (errno == EWOULDBLOCK))
    {
        tio_error_printf("Device file is locked by another process");
        exit(EXIT_FAILURE);
    }

    /* Flush stale I/O data (if any) */
    tcflush(fd, TCIOFLUSH);

    /* Print connect status */
    tio_printf("Connected");
    connected = true;
    print_tainted = false;

    if (option.timestamp)
    {
        next_timestamp = true;
    }

    /* Manage print output mode */
    if (option.hex_mode)
    {
        print = print_hex;
        print_mode = HEX;
    }
    else
    {
        print = print_normal;
        print_mode = NORMAL;
    }

    /* Save current port settings */
    if (tcgetattr(fd, &tio_old) < 0)
    {
        tio_error_printf_silent("Could not get port settings (%s)", strerror(errno));
        goto error_tcgetattr;
    }

#ifdef HAVE_IOSSIOSPEED
    if (!standard_baudrate)
    {
        /* OS X wants these fields left alone before setting arbitrary baud rate */
        tio.c_ispeed = tio_old.c_ispeed;
        tio.c_ospeed = tio_old.c_ospeed;
    }
#endif

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
        tio_error_printf_silent("Could not apply port settings (%s)", strerror(errno));
        goto error_tcsetattr;
    }

    /* Set arbitrary baudrate (only works on supported platforms) */
    if (!standard_baudrate)
    {
        if (setspeed(fd, option.baudrate) != 0)
        {
            tio_error_printf_silent("Could not set baudrate speed (%s)", strerror(errno));
            goto error_setspeed;
        }
    }

    /* Input loop */
    while (true)
    {
        FD_ZERO(&rdfs);
        FD_SET(fd, &rdfs);
        FD_SET(STDIN_FILENO, &rdfs);
        maxfd = MAX(fd, STDIN_FILENO);
        maxfd = MAX(maxfd, socket_add_fds(&rdfs, true));

        /* Block until input becomes available */
        status = select(maxfd + 1, &rdfs, NULL, NULL, NULL);
        if (status > 0)
        {
            bool forward = false;
            if (FD_ISSET(fd, &rdfs))
            {
                /* Input from tty device ready */
                ssize_t bytes_read = read(fd, input_buffer, BUFSIZ);
                if (bytes_read <= 0)
                {
                    /* Error reading - device is likely unplugged */
                    tio_error_printf_silent("Could not read from tty device");
                    goto error_read;
                }

                /* Update receive statistics */
                rx_total += bytes_read;

                /* Process input byte by byte */
                for (int i=0; i<bytes_read; i++)
                {
                    input_char = input_buffer[i];

                    /* Print timestamp on new line if enabled */
                    if (next_timestamp && input_char != '\n' && input_char != '\r')
                    {
                        now = current_time();
                        if (now)
                        {
                            ansi_printf_raw("[%s] ", now);
                            if (option.log)
                            {
                                log_printf("[%s] ", now);
                            }
                            next_timestamp = false;
                        }
                    }

                    /* Map input character */
                    if ((input_char == '\n') && (map_i_nl_crnl))
                    {
                        print('\r');
                        print('\n');
                        if (option.timestamp)
                        {
                            next_timestamp = true;
                        }
                    }
                    else
                    {
                        /* Print received tty character to stdout */
                        print(input_char);
                    }

                    /* Write to log */
                    if (option.log)
                    {
                        log_putc(input_char);
                    }

                    socket_write(input_char);

                    print_tainted = true;

                    if (input_char == '\n' && option.timestamp)
                    {
                        next_timestamp = true;
                    }
                }
            }
            else if (FD_ISSET(STDIN_FILENO, &rdfs))
            {
                /* Input from stdin ready */
                ssize_t bytes_read = read(STDIN_FILENO, input_buffer, BUFSIZ);
                if (bytes_read < 0)
                {
                    tio_error_printf_silent("Could not read from stdin (%s)", strerror(errno));
                    goto error_read;
                }
                else if (bytes_read == 0)
                {
                    /* Reached EOF (when piping to stdin) */
                    tty_flush(fd);
                    exit(EXIT_SUCCESS);
                }

                /* Process input byte by byte */
                for (int i=0; i<bytes_read; i++)
                {
                    input_char = input_buffer[i];

                    /* Forward input to output */
                    output_char = input_char;
                    forward = true;

                    if (interactive_mode)
                    {
                        /* Do not forward prefix key */
                        if (input_char == option.prefix_code)
                        {
                            forward = false;
                        }

                        /* Handle commands */
                        handle_command_sequence(input_char, previous_char, &output_char, &forward);

                        /* Save previous key */
                        previous_char = input_char;

                        if (print_mode == HEX)
                        {
                            if (!is_valid_hex(input_char))
                            {
                                tio_warning_printf("Invalid hex character: '%d' (0x%02x)", input_char, input_char);
                                forward = false;
                            }
                        }
                    }

                    if (forward)
                    {
                        forward_to_tty(fd, output_char);
                    }
                }

                tty_flush(fd);
            }
            else
            {
                forward = socket_handle_input(&rdfs, &output_char);

                if (forward)
                {
                    forward_to_tty(fd, output_char);
                }

                tty_flush(fd);
            }
        }
        else if (status == -1)
        {
            tio_error_printf("select() failed (%s)", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    return TIO_SUCCESS;

error_setspeed:
error_tcsetattr:
error_tcgetattr:
error_read:
    tty_disconnect();
error_open:
    return TIO_ERROR;
}

void list_serial_devices(void)
{
    DIR *d = opendir(PATH_SERIAL_DEVICES);
    if (d)
    {
        struct dirent *dir;
        while ((dir = readdir(d)) != NULL)
        {
            if ((strcmp(dir->d_name, ".")) && (strcmp(dir->d_name, "..")))
            {
#ifdef __APPLE__
#define TTY_DEVICES_PREFIX "tty."
                if (!strncmp(dir->d_name, TTY_DEVICES_PREFIX, sizeof(TTY_DEVICES_PREFIX) - 1))
#endif
                printf("%s%s\n", PATH_SERIAL_DEVICES, dir->d_name);
            }
        }
        closedir(d);
    }
}
