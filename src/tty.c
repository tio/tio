/*
 * tio - a simple TTY terminal I/O application
 *
 * Copyright (c) 2014-2016  Martin Lund
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
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/file.h>
#include <fcntl.h>
#include <termios.h>
#include <stdbool.h>
#include <errno.h>
#include "config.h"
#include "tio/tty.h"
#include "tio/print.h"
#include "tio/options.h"
#include "tio/time.h"
#include "tio/log.h"
#include "tio/error.h"

static struct termios tio, new_stdout, old_stdout, old_tio;
static unsigned long rx_total = 0, tx_total = 0;
static bool connected = false;
static bool tainted = false;
static int fd;

#define tio_printf(format, args...) \
{ \
    if (tainted) putchar('\n'); \
    color_printf("[tio %s] " format, current_time(), ## args); \
    tainted = false; \
}

void handle_command_sequence(char input_char, char previous_char, char *output_char, bool *forward)
{
    char unused_char;
    bool unused_bool;

    /* Ignore unused arguments */
    if (output_char == NULL)
        output_char = &unused_char;

    if (forward == NULL)
        forward = &unused_bool;

    /* Handle escape key commands */
    if (previous_char == KEY_CTRL_T)
    {
        switch (input_char)
        {
            case KEY_QUESTION:
                tio_printf("Key commands:");
                tio_printf(" ctrl-t ?   List available key commands");
                tio_printf(" ctrl-t i   Show settings information");
                tio_printf(" ctrl-t q   Quit");
                tio_printf(" ctrl-t s   Show statistics");
                tio_printf(" ctrl-t t   Send ctrl-t key code");
                *forward = false;
                break;
            case KEY_I:
                tio_printf("Settings information:");
                tio_printf(" TTY device: %s", option.tty_device);
                tio_printf(" Baudrate: %u", option.baudrate);
                tio_printf(" Databits: %d", option.databits);
                tio_printf(" Flow: %s", option.flow);
                tio_printf(" Stopbits: %d", option.stopbits);
                tio_printf(" Parity: %s", option.parity);
                tio_printf(" Output delay: %d", option.output_delay);
                if (option.log)
                    tio_printf(" Log file: %s", option.log_filename);
                *forward = false;
                break;
            case KEY_Q:
                /* Exit upon ctrl-t q sequence */
                exit(EXIT_SUCCESS);
            case KEY_T:
                /* Send ctrl-t key code upon ctrl-t t sequence */
                *output_char = KEY_CTRL_T;
                break;
            case KEY_S:
                /* Show tx/rx statistics upon ctrl-t s sequence */
                tio_printf("Statistics:");
                tio_printf(" Sent %lu bytes, received %lu bytes", tx_total, rx_total);
                *forward = false;
                break;
            default:
                /* Ignore unknown ctrl-t escaped keys */
                *forward = false;
                break;
        }
    }
}

void stdout_configure(void)
{
    /* Save current stdout settings */
    if (tcgetattr(STDOUT_FILENO, &old_stdout) < 0)
    {
        error_printf("Saving current stdio settings failed");
        exit(EXIT_FAILURE);
    }

    /* Prepare new stdout settings */
    memset(&new_stdout, 0, sizeof(new_stdout));

    /* Control, input, output, local modes for stdout */
    new_stdout.c_cflag = 0;
    new_stdout.c_iflag = 0;
    new_stdout.c_oflag = 0;
    new_stdout.c_lflag = 0;

    /* Control characters */
    new_stdout.c_cc[VTIME] = 0; /* Inter-character timer unused */
    new_stdout.c_cc[VMIN]  = 1; /* Blocking read until 1 character received */

    /* Activate new stdout settings */
    tcsetattr(STDOUT_FILENO, TCSANOW, &new_stdout);
    tcsetattr(STDOUT_FILENO, TCSAFLUSH, &new_stdout);

    /* Print launch hints */
    tio_printf("tio v%s", VERSION);
    tio_printf("Press ctrl-t q to quit");

    /* Make sure we restore old stdout settings on exit */
    atexit(&stdout_restore);
}

void stdout_restore(void)
{
    tcsetattr(STDOUT_FILENO, TCSANOW, &old_stdout);
    tcsetattr(STDOUT_FILENO, TCSAFLUSH, &old_stdout);
}

void tty_configure(void)
{
    speed_t baudrate;

    memset(&tio, 0, sizeof(tio));

    /* Set speed */
    switch (option.baudrate)
    {
        case 0:
            baudrate = B0;
            break;
        case 50:
            baudrate = B50;
            break;
        case 75:
            baudrate = B75;
            break;
        case 110:
            baudrate = B110;
            break;
        case 134:
            baudrate = B134;
            break;
        case 150:
            baudrate = B150;
            break;
        case 200:
            baudrate = B200;
            break;
        case 300:
            baudrate = B300;
            break;
        case 600:
            baudrate = B600;
            break;
        case 1200:
            baudrate = B1200;
            break;
        case 1800:
            baudrate = B1800;
            break;
        case 2400:
            baudrate = B2400;
            break;
        case 4800:
            baudrate = B4800;
            break;
        case 9600:
            baudrate = B9600;
            break;
        case 19200:
            baudrate = B19200;
            break;
        case 38400:
            baudrate = B38400;
            break;
        case 57600:
            baudrate = B57600;
            break;
        case 115200:
            baudrate = B115200;
            break;
        case 230400:
            baudrate = B230400;
            break;
        case 460800:
            baudrate = B460800;
            break;
        case 500000:
            baudrate = B500000;
            break;
        case 576000:
            baudrate = B576000;
            break;
        case 921600:
            baudrate = B921600;
            break;
        case 1000000:
            baudrate = B1000000;
            break;
        case 1152000:
            baudrate = B1152000;
            break;
        case 1500000:
            baudrate = B1500000;
            break;
        case 2000000:
            baudrate = B2000000;
            break;
        case 2500000:
            baudrate = B2500000;
            break;
        case 3000000:
            baudrate = B3000000;
            break;
        case 3500000:
            baudrate = B3500000;
            break;
        case 4000000:
            baudrate = B4000000;
            break;
        default:
            error_printf("Invalid baud rate");
            exit(EXIT_FAILURE);
    }

    cfsetispeed(&tio, baudrate);
    cfsetospeed(&tio, baudrate);

    /* Set databits */
    tio.c_cflag &= ~CSIZE; // ?
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
}

void tty_wait_for_device(void)
{
    fd_set rdfs;
    int    status;
    struct timeval tv;
    static char input_char, previous_char = 0;
    static bool first = true;

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
        if (access(option.tty_device, R_OK) == 0)
            return;
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
    tcsetattr(fd, TCSANOW, &old_tio);
    tcsetattr(fd, TCSAFLUSH, &old_tio);

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

    /* Open tty device */
    fd = open(option.tty_device, O_RDWR | O_NOCTTY );
    if (fd < 0)
    {
        error_printf_silent("Could not open tty device (%s)", strerror(errno));
        goto error_open;
    }

    /* Make sure device is of tty type */
    if (!isatty(fd))
    {
        error_printf_silent("Not a tty device");
        goto error_isatty;
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

    /* Save current port settings */
    if (tcgetattr(fd, &old_tio) < 0)
        goto error_tcgetattr;

    /* Make sure we restore tty settings on exit */
    if (first)
    {
        atexit(&tty_restore);
        first = false;
    }

    /* Control, input, output, local modes for tty device */
    tio.c_cflag |= CLOCAL | CREAD;
    tio.c_oflag = 0;
    tio.c_lflag = 0;

    /* Control characters */
    tio.c_cc[VTIME] = 0; /* Inter-character timer unused */
    tio.c_cc[VMIN]  = 1; /* Blocking read until 1 character received */

    /* Activate new port settings */
    tcsetattr(fd, TCSANOW, &tio);
    tcsetattr(fd, TCSAFLUSH, &tio);

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

                    /* Print received tty character to stdout */
                    putchar(input_char);
                    fflush(stdout);

                    /* Write to log */
                    if (option.log)
                        log_write(input_char);

                    tainted = true;

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
                    /* Send output to tty device */
                    status = write(fd, &output_char, 1);
                    if (status < 0)
                        warning_printf("Could not write to tty device");

                    /* Update transmit statistics */
                    tx_total++;

                    /* Insert output delay */
                    delay(option.output_delay);
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

error_tcgetattr:
error_isatty:
error_read:
    tty_disconnect();
error_open:
    return TIO_ERROR;
}
