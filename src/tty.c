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
#include <fcntl.h>
#include <termios.h>
#include <stdbool.h>
#include <errno.h>
#include "gotty/tty.h"
#include "gotty/print.h"
#include "gotty/options.h"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))

static int connected = false;
struct termios stdio, old_stdio, old_tio;
static int fd;

void configure_stdout(void)
{
    /* Save current stdout settings */
    if (tcgetattr(STDOUT_FILENO, &old_stdio) < 0)
    {
        printf("Error: Saving current stdio settings failed\n");
        exit(EXIT_FAILURE);
    }

    /* Prepare stdio settings */
    bzero(&stdio, sizeof(stdio));

    /* Control, input, output, local modes for stdio */
    stdio.c_cflag = 0;
    stdio.c_iflag = 0;
    stdio.c_oflag = 0;
    stdio.c_lflag = 0;

    /* Control characters */
    stdio.c_cc[VTIME] = 0; /* Inter-character timer unused */
    stdio.c_cc[VMIN]  = 1; /* Blocking read until 1 character received */

    /* Activate stdio settings */
    tcsetattr(STDOUT_FILENO, TCSANOW, &stdio);
    tcsetattr(STDOUT_FILENO, TCSAFLUSH, &stdio);
}

void restore_stdout(void)
{
    tcflush(STDOUT_FILENO, TCIOFLUSH);
    tcsetattr(STDOUT_FILENO, TCSANOW, &old_stdio);
    tcsetattr(STDOUT_FILENO, TCSAFLUSH, &old_stdio);
}

void restore_tty(void)
{
    tcsetattr(fd, TCSANOW, &old_tio);
    tcsetattr(fd, TCSAFLUSH, &old_tio);

    if (connected)
        gotty_printf("Disconnected\n");
}

int connect_tty(void)
{
    fd_set rdfs;           /* Read file descriptor set */
    int    maxfd;          /* Maximum file desciptor used */
    char   c, c0 = 0, c1 = 0;
    static bool first = true;
    int    status;

    /* Open tty device */
    fd = open(option.device, O_RDWR | O_NOCTTY ); 
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

    gotty_printf("Connected");
    connected = true;

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
            if (read(fd, &c, 1) > 0)
            {
                /* Print received tty character to stdout */
                putchar(c);
                fflush(stdout);
            } else
            {
                /* Error reading - device is likely unplugged */
                if (!option.no_autoconnect)
                    gotty_printf("Disconnected");
                close(fd);
                connected = false;
                return EXIT_FAILURE;
            }
        }
        if (FD_ISSET(STDIN_FILENO, &rdfs))
        {
            /* Input from stdin ready */
            status = read(STDIN_FILENO, &c, 1);

            /* Exit upon ctrl-g ctrl-q sequence */
            c1 = c0;
            c0 = c;
            if ((c0 == CTRLQ) && (c1 == CTRLG))
                {
                    close(fd);
                    exit(EXIT_SUCCESS);
                }

            /* Forward input to tty device */
            status = write(fd, &c, 1);
        }
    }

    return EXIT_SUCCESS;
}
