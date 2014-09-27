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
#include "gotty/options.h"
#include "gotty/tty.h"
#include "gotty/device.h"

int main(int argc, char *argv[])
{
    int status;

    /* Parse options */
    parse_options(argc, argv);

    /* Configure output terminal */
    configure_stdout();

    /* Connect to tty device */
    if (option.no_autoconnect)
        status = connect_tty();
    else
    {
        while (true)
        {
            wait_for_device();
            status = connect_tty();
        }
    }

    return status;
}
