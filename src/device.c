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
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "gotty/options.h"
#include "gotty/print.h"
#include "gotty/tty.h"

void wait_for_device(void)
{
    int ready, n;
    struct stat status;
    fd_set rdfs;
    struct timeval tv;
    char c, c0=0, c1=0;

    /* Loop until device pops up */
    while (true)
    {
        /* Wait up to 1 seconds */
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
            n = read(STDIN_FILENO, &c, 1);

            /* Exit upon ctrl-g ctrl-q sequence */
            c1 = c0;
            c0 = c;
            if ((c0 == CTRLQ) && (c1 == CTRLG))
                exit(EXIT_SUCCESS);
        } else
        {
            /* Timeout */

            /* Test for device file */
            if (stat(option.device, &status) == 0)
                return;
        }
    }
}
