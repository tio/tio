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
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <libgen.h>
#include "options.h"
#include "print.h"
#include "error.h"

static FILE *fp;
static bool log_error = false;

static char *date_time(void)
{
    static char date_time_string[50];
    struct tm *tm;
    struct timeval tv;

    gettimeofday(&tv, NULL);

    tm = localtime(&tv.tv_sec);
    strftime(date_time_string, sizeof(date_time_string), "%Y-%m-%dT%H:%M:%S", tm);

    return date_time_string;
}

void log_open(const char *filename)
{
    static char automatic_filename[400];

    if (filename == NULL)
    {
        // Generate filename if none provided ("tio_DEVICE_YYYY-MM-DDTHH:MM:SS.log")
        sprintf(automatic_filename, "tio_%s_%s.log", basename((char *)option.tty_device), date_time());
        filename = automatic_filename;
        option.log_filename = automatic_filename;
    }

    fp = fopen(filename, "w+");

    if (fp == NULL)
    {
        log_error = true;
        exit(EXIT_FAILURE);
    }
    setvbuf(fp, NULL, _IONBF, 0);
}

void log_write(char c)
{
    if (fp != NULL)
    {
        fputc(c, fp);
    }
}

void log_close(void)
{
    if (fp != NULL)
    {
        fclose(fp);
    }
}

void log_exit(void)
{
    if (option.log)
    {
        log_close();
    }

    if (log_error)
    {
        error_printf("Could not open log file %s (%s)", option.log_filename, strerror(errno));
    }
    else if (option.log)
    {
        tio_printf("Saved log to file %s", option.log_filename);
    }
}
