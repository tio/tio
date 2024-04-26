/*
 * tio - a serial device I/O tool
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

#define __STDC_WANT_LIB_EXT2__ 1   // To access vasprintf

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include "options.h"
#include "print.h"
#include "error.h"
#include "timestamp.h"

static char error[2][1000];
static bool in_session = false;

void error_enter_session_mode(void)
{
    in_session = true;
}

void error_printf_(const char *format, ...)
{
    va_list args;
    char *line;

    va_start(args, format);
    vasprintf(&line, format, args);

    if (in_session)
    {
        if (print_tainted)
        {
            putchar('\n');
        }
        ansi_error_printf("[%s] %s", timestamp_current_time(), line);
    }
    else
    {
        fprintf(stderr, "%s\n", line);
    }

    va_end(args);

    print_tainted = false;
    free(line);
}

void tio_error_printf(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    vsnprintf(error[0], 1000, format, args);
    va_end(args);
}

void tio_error_printf_silent(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    vsnprintf(error[1], 1000, format, args);
    va_end(args);
}

void error_exit(void)
{
    if (error[0][0] != 0)
    {
        /* Print error */
        error_printf_("Error: %s", error[0]);
    }
    else if ((error[1][0] != 0) && (option.no_reconnect))
    {
        /* Print silent error */
        error_printf_("Error: %s", error[1]);
    }
}
