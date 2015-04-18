/*
 * Go TTY - The Really Simple Terminal Application
 *
 * Copyright (c) 2014  Martin Lund
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
#include <fcntl.h>
#include <termios.h>
#include <stdbool.h>
#include <errno.h>
#include "gotty/tty.h"
#include "gotty/print.h"
#include "gotty/options.h"
#include "gotty/time.h"

static int connected = false;
struct termios new_stdout, old_stdout, old_tio;
static int fd;
static bool tainted = false;

void wait_for_tty_device(void)
{
    int ready, n;
    struct stat status;
    fd_set rdfs;
    struct timeval tv;
    char c_stdin[3];

    /* Loop until device pops up */
    while (true)
    {
        /* Test for device file */
        if (stat(option.tty_device, &status) == 0)
            return;

        /* Wait up to 1 second */
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        FD_ZERO(&rdfs);
        FD_SET(STDIN_FILENO, &rdfs);

        /* Block until input becomes available or timeout */
        ready = select(STDIN_FILENO + 1, &rdfs, NULL, NULL, &tv);
        if (ready)
        {
            /* Input from stdin ready */

            /* Read one character */
            n = read(STDIN_FILENO, &c_stdin[0], 1);

            /* Exit upon ctrl-g + q sequence */
            c_stdin[2] = c_stdin[1];
            c_stdin[1] = c_stdin[0];
            if ((c_stdin[1] == KEY_Q) && (c_stdin[2] == KEY_CTRL_G))
                exit(EXIT_SUCCESS);
        }
    }
}

void configure_stdout(void)
{
    /* Save current stdout settings */
    if (tcgetattr(STDOUT_FILENO, &old_stdout) < 0)
    {
        printf("Error: Saving current stdio settings failed\n");
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
}

void restore_stdout(void)
{
    tcflush(STDOUT_FILENO, TCIOFLUSH);
    tcsetattr(STDOUT_FILENO, TCSANOW, &old_stdout);
    tcsetattr(STDOUT_FILENO, TCSAFLUSH, &old_stdout);
}

void disconnect_tty(void)
{
    if (tainted)
        putchar('\n');
    color_printf("[gotty %s] Disconnected", current_time());
    close(fd);
    connected = false;
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
    int    maxfd;          /* Maximum file desciptor used */
    static bool first = true;
    int    status;
    char   c_tty;
    char   c_stdin[3];

    /* Open tty device */
    fd = open(option.tty_device, O_RDWR | O_NOCTTY );
    if (fd <0)
    {
        printf("\033[300DError: %s\n\033[300D", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Make sure device is of tty type */
    if (!isatty(fd))
    {
        printf("\033[300DError: Not a tty device\n\033[300D");
        exit(EXIT_FAILURE);
    }

    /* Flush stale I/O data (if any) */
    tcflush(fd, TCIOFLUSH);

    /* Print connect status */
    color_printf("[gotty %s] Connected", current_time());
    connected = true;
    tainted = false;
    bzero(&c_stdin[0], 3);
    c_tty = 0;

    /* Save current port settings */
    if (tcgetattr(fd, &old_tio) < 0)
        return EXIT_FAILURE;

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
        select(maxfd, &rdfs, NULL, NULL, NULL);
        if (FD_ISSET(fd, &rdfs))
        {
            /* Input from tty device ready */
            if (read(fd, &c_tty, 1) > 0)
            {
                /* Print received tty character to stdout */
                putchar(c_tty);
                fflush(stdout);
                if (c_tty != 0x7) // Small trick to avoid ctrl-g echo
                    tainted = true;
            } else
            {
                /* Error reading - device is likely unplugged */
                disconnect_tty();
                return EXIT_FAILURE;
            }
        }
        if (FD_ISSET(STDIN_FILENO, &rdfs))
        {
            /* Input from stdin ready */
            status = read(STDIN_FILENO, &c_stdin[0], 1);
            if ((c_stdin[0] != KEY_Q) && (c_stdin[0] != KEY_CTRL_G))
                tainted = true;

            /* Exit upon ctrl-g + q sequence */
            c_stdin[2] = c_stdin[1];
            c_stdin[1] = c_stdin[0];
            if ((c_stdin[1] == KEY_Q) && (c_stdin[2] == KEY_CTRL_G))
                exit(EXIT_SUCCESS);

            /* Forward input to tty device */
            status = write(fd, &c_stdin[0], 1);

            /* Insert output delay */
            if (option.output_delay)
                usleep(option.output_delay * 1000);
        }
    }

    return EXIT_SUCCESS;
}
