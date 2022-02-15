/*
 * tio - a simple TTY terminal I/O tool
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
#include <dirent.h>
#include "config.h"
#include "tty.h"
#include "print.h"
#include "options.h"
#include "misc.h"
#include "log.h"
#include "error.h"

#ifdef HAVE_TERMIOS2
extern int setspeed2(int fd, int baudrate);
#endif

#ifdef __APPLE__
#define PATH_SERIAL_DEVICES "/dev/"
#else
#define PATH_SERIAL_DEVICES "/dev/serial/by-id/"
#endif

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
                tio_printf(" ctrl-t d   Toggle DTR line");
                tio_printf(" ctrl-t e   Toggle local echo mode");
                tio_printf(" ctrl-t h   Toggle hexadecimal mode");
                tio_printf(" ctrl-t l   Clear screen");
                tio_printf(" ctrl-t L   Show lines state");
                tio_printf(" ctrl-t q   Quit");
                tio_printf(" ctrl-t r   Toggle RTS line");
                tio_printf(" ctrl-t s   Show statistics");
                tio_printf(" ctrl-t t   Send ctrl-t key code");
                tio_printf(" ctrl-t T   Toggle line timestamp mode");
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
                tio_printf(" Local echo: %s", option.local_echo ? "enabled" : "disabled");
                tio_printf(" Timestamps: %s", option.timestamp ? "enabled" : "disabled");
                tio_printf(" Output delay: %d", option.output_delay);
                tio_printf(" Auto connect: %s", option.no_autoconnect ? "disabled" : "enabled");
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
                tio_printf(" Sent %lu bytes", tx_total);
                tio_printf(" Received %lu bytes", rx_total);
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
         * To see which baud rates are being probed see meson.build
         */
        BAUDRATE_CASES

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
        status = access(option.tty_device, R_OK);
        if (status == 0) {
            last_errno = 0;
            return;
        }
        else if (last_errno != errno)
        {
            warning_printf("Could not open tty device (%s)", strerror(errno));
            tio_printf("Waiting for tty device..");
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

static void optional_local_echo(char c)
{
    if (!option.local_echo)
        return;
    print(c);
    fflush(stdout);
    if (option.log)
        log_write(c);
}

int tty_connect(void)
{
    fd_set rdfs;           /* Read file descriptor set */
    int    maxfd;          /* Maximum file descriptor used */
    char   input_char, output_char;
    static char previous_char = 0;
    static bool first = true;
    int    status;
    bool next_timestamp = false;
    char*  now = NULL;

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
    print_tainted = false;

    if (option.timestamp)
        next_timestamp = true;

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
                        if (now)
                        {
                            ansi_printf_raw("[%s] ", now);
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
                            next_timestamp = false;
                        }
                    }

                    /* Map input character */
                    if ((input_char == '\n') && (map_i_nl_crnl))
                    {
                        print('\r');
                        print('\n');
                        if (option.timestamp)
                            next_timestamp = true;
                    } else
                    {
                        /* Print received tty character to stdout */
                        print(input_char);
                    }
                    fflush(stdout);

                    /* Write to log */
                    if (option.log)
                        log_write(input_char);

                    print_tainted = true;

                    if (input_char == '\n' && option.timestamp)
                        next_timestamp = true;
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

                /* Forward input to output except ctrl-t key */
                output_char = input_char;
                if (input_char == KEY_CTRL_T)
                    forward = false;

                /* Handle commands */
                handle_command_sequence(input_char, previous_char, &output_char, &forward);

                if (forward)
                {
                    /* Map output character */
                    if ((output_char == 127) && (map_o_del_bs))
                        output_char = '\b';
                    if ((output_char == '\r') && (map_o_cr_nl))
                        output_char = '\n';

                    /* Map newline character */
                    if ((output_char == '\n' || output_char == '\r') && (map_o_nl_crnl)) {
                        const char *crlf = "\r\n";

                        optional_local_echo(crlf[0]);
                        optional_local_echo(crlf[1]);
                        status = write(fd, crlf, 2);
                        if (status < 0)
                            warning_printf("Could not write to tty device");

                        tx_total += 2;
                        delay(option.output_delay);
                    } else
                    {
                        /* Send output to tty device */
                        optional_local_echo(output_char);
                        status = write(fd, &output_char, 1);
                        if (status < 0)
                            warning_printf("Could not write to tty device");
                        fsync(fd);

                        /* Update transmit statistics */
                        tx_total++;

                        /* Insert output delay */
                        delay(option.output_delay);
                    }
                }

                /* Save previous key */
                previous_char = input_char;

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
