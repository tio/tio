/*
 * tio - a simple TTY terminal I/O application
 *
 * Copyright (c) 2014-2017  Martin Lund
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
#include <time.h>
#include <sys/time.h>
#include "tio/error.h"
#include "tio/print.h"

#define TIME_STRING_SIZE 20

char * current_time(void)
{
    static char time_string[TIME_STRING_SIZE];
    struct tm *tmp;

    struct timeval tv;
    gettimeofday(&tv,NULL);

    tmp = localtime(&tv.tv_sec);
    if (tmp == NULL)
    {
        error_printf("Retrieving local time failed");
        exit(EXIT_FAILURE);
    }

    size_t len = strftime(time_string, sizeof(time_string), "%H:%M:%S", tmp);
    if (len) {
        len = snprintf(time_string + len, TIME_STRING_SIZE - len, ".%03ld", tv.tv_usec / 1000);
    }

    return (len >= 0) && (len < TIME_STRING_SIZE) ? time_string : NULL;
}

void delay(long ms)
{
    struct timespec ts;

    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;

    nanosleep(&ts, NULL);
}
