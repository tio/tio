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
#include <string.h>
#include <errno.h>
#include "tio/options.h"
#include "tio/print.h"
#include "tio/error.h"

char error[2][1000];

void error_exit(void)
{
    /* Always print errors but only print silent errors when in no autoconnect
     * mode */
    if (error[0][0] != 0)
        printf("\rError: %s\r\n", error[0]);
    else if ((error[1][0] != 0) && (option.no_autoconnect))
        printf("\rError: %s\r\n", error[1]);
}
