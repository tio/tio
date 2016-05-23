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
#include <stdlib.h>
#include <unistd.h>
#include "tio/options.h"
#include "tio/tty.h"
#include "tio/log.h"
#include "tio/error.h"
#include "tio/print.h"

int main(int argc, char *argv[])
{
    int status;

    /* Install error exit handler */
    atexit(&error_exit);

    /* Parse options */
    parse_options(argc, argv);

    /* Configure tty device */
    tty_configure();

    /* Configure output terminal */
    stdout_configure();

    /* Install log exit handler */
    atexit(&log_exit);

    /* Create log file */
    if (option.log)
        log_open(option.log_filename);

    /* Connect to tty device */
    if (option.no_autoconnect)
        status = tty_connect();
    else
    {
        /* Enter connect loop */
        while (true)
        {
            tty_wait_for_device();
            status = tty_connect();
        }
    }

    return status;
}
