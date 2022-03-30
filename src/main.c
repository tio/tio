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

int main(int argc, char *argv[])
{
    int status = 0;

    /* Handle received signals */
    signal_handlers_install();

    /* Add error exit handler */
    atexit(&error_exit);

    /* Parse configuration file */
    config_file_parse(argc, argv);
    atexit(&config_exit);

    /* Parse command-line options */
    options_parse(argc, argv);

    /* List available serial devices */
    if (option.list_devices)
    {
        list_serial_devices();
        return status;
    }

    /* Configure tty device */
    tty_configure();

    /* Configure input terminal */
    stdin_configure();

    /* Configure output terminal */
    stdout_configure();

    /* Add log exit handler */
    atexit(&log_exit);

    /* Create log file */
    if (option.log)
        log_open(option.log_filename);

    /* Enable ANSI text formatting (colors etc.) */
    print_enable_ansi_formatting();

    /* Print launch hints */
    tio_printf("tio v%s", VERSION);
    tio_printf("Press ctrl-t q to quit");
    if (option.debug)
    {
        config_file_print();
        options_print();
    }

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
