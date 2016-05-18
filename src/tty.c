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
#include "tio/tty.h"
#include "tio/print.h"
#include "tio/options.h"
#include "tio/time.h"
#include "tio/log.h"
#include "tio/error.h"

static struct termios new_stdout, old_stdout, old_tio;
static long rx_total = 0, tx_total = 0;
static bool connected = false;
static bool tainted = false;
static int fd;

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
            case KEY_I:
                if (tainted)
                    putchar('\n');
                color_printf("[tio %s] TTY device: %s", current_time(), option.tty_device);
                color_printf("[tio %s] Baudrate: %d", current_time(), option.baudrate);
                color_printf("[tio %s] Databits: %d", current_time(), option.databits);
                color_printf("[tio %s] Flow: %s", current_time(), option.flow);
                color_printf("[tio %s] Stopbits: %d", current_time(), option.stopbits);
                color_printf("[tio %s] Parity: %s", current_time(), option.parity);
                color_printf("[tio %s] Output delay: %d", current_time(), option.output_delay);
                if (option.log)
                    color_printf("[tio %s] Log file: %s", current_time(), option.log_filename);
                tainted = false;
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
                if (tainted)
                    putchar('\n');
                color_printf("[tio %s] Sent %ld bytes, received %ld bytes", current_time(), tx_total, rx_total);
                tainted = false;
                *forward = false;
                break;
            default:
                /* Ignore unknown ctrl-t escaped keys */
                *forward = false;
                break;
        }
    }
}

void wait_for_tty_device(void)
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

void configure_stdout(void)
{
    /* Save current stdout settings */
    if (tcgetattr(STDOUT_FILENO, &old_stdout) < 0)
    {
        error_printf("Saving current stdio settings failed");
        exit(EXIT_FAILURE);
    }

    /* Prepare new stdout settings */
    bzero(&new_stdout, sizeof(new_stdout));

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

    /* Make sure we restore old stdout settings on exit */
    atexit(&restore_stdout);
}

void restore_stdout(void)
{
    tcsetattr(STDOUT_FILENO, TCSANOW, &old_stdout);
    tcsetattr(STDOUT_FILENO, TCSAFLUSH, &old_stdout);
}

void disconnect_tty(void)
{
    if (tainted)
        putchar('\n');

    if (connected)
    {
        color_printf("[tio %s] Disconnected", current_time());
        flock(fd, LOCK_UN);
        close(fd);
        connected = false;
    }
}

void restore_tty(void)
{
    tcsetattr(fd, TCSANOW, &old_tio);
    tcsetattr(fd, TCSAFLUSH, &old_tio);

    if (connected)
        disconnect_tty();
}

int connect_tty(void)
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
        printf("Error: Device file is locked by another process\r\n");
        exit(EXIT_FAILURE);
    }

    /* Flush stale I/O data (if any) */
    tcflush(fd, TCIOFLUSH);

    /* Print connect status */
    color_printf("[tio %s] Connected", current_time());
    connected = true;
    tainted = false;

    /* Save current port settings */
    if (tcgetattr(fd, &old_tio) < 0)
        goto error_tcgetattr;

    /* Make sure we restore tty settings on exit */
    if (first)
    {
        atexit(&restore_tty);
        first = false;
    }

    /* Control, input, output, local modes for tty device */
    option.tio.c_cflag |= CLOCAL | CREAD;
    option.tio.c_oflag = 0;
    option.tio.c_lflag = 0;

    /* Control characters */
    option.tio.c_cc[VTIME] = 0; /* Inter-character timer unused */
    option.tio.c_cc[VMIN]  = 1; /* Blocking read until 1 character received */

    /* Activate new port settings */
    tcsetattr(fd, TCSANOW, &option.tio);
    tcsetattr(fd, TCSAFLUSH, &option.tio);

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

                    /* Write to log */
                    if (option.log)
                        log_write(output_char);

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
    disconnect_tty();
error_open:
    return TIO_ERROR;
}
