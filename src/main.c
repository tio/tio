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
#include "options.h"
#include "configfile.h"
#include "tty.h"
#include "log.h"
#include "error.h"
#include "print.h"
#include "signals.h"
#include "socket.h"

int main(int argc, char *argv[])
{
    int status = 0;

    /* Handle received signals */
    signal_handlers_install();

    /* Add error exit handler */
    atexit(&error_exit);

    /* Parse command-line options (1st pass) */
    options_parse(argc, argv);

    /* Parse configuration file */
    config_file_parse();

    /* Parse command-line options (2nd pass) */
    options_parse_final(argc, argv);

    /* Configure tty device */
    tty_configure();

    /* Configure input terminal */
    if (isatty(fileno(stdin)))
    {
       stdin_configure();
    }

    /* Configure output terminal */
    if (isatty(fileno(stdout)))
    {
        stdout_configure();
    }
    else
    {
        // No color when piping
        option.color = -1;
    }

    /* Add log exit handler */
    atexit(&log_exit);

    /* Create log file */
    if (option.log)
    {
        log_open(option.log_filename);
    }

    /* Initialize ANSI text formatting (colors etc.) */
    print_init_ansi_formatting();

    /* Print launch hints */
    tio_printf("tio v%s", VERSION);
    tio_printf("Press ctrl-t q to quit");

    /* Open socket */
    if (option.socket)
    {
        socket_configure();
    }

    /* Connect to tty device */
    if (option.no_autoconnect)
    {
        status = tty_connect();
    }
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
