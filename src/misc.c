/*
 * tio - a simple TTY terminal I/O tool
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
#include <time.h>
#include <sys/time.h>
#include "error.h"
#include "print.h"
#include "options.h"

#define TIME_STRING_SIZE_MAX 24

char *current_time(void)
{
    static char time_string[TIME_STRING_SIZE_MAX];
    static struct timeval tv_start;
    static bool first = true;
    struct tm *tm;
    struct timeval tv;
    size_t len;

    gettimeofday(&tv, NULL);

    if (first)
    {
        tv_start = tv;
        first = false;
    }

    // Add formatted timestap
    switch (option.timestamp)
    {
        case TIMESTAMP_NONE:
        case TIMESTAMP_24HOUR:
            // "hh:mm:ss.sss" (24 hour format)
            tm = localtime(&tv.tv_sec);
            len = strftime(time_string, sizeof(time_string), "%H:%M:%S", tm);
            break;
        case TIMESTAMP_24HOUR_START:
            // "hh:mm:ss.sss" (24 hour format relative to start time)
            timersub(&tv, &tv_start, &tv);
            tv.tv_sec -= 3600; // Why is this needed??
            tm = localtime(&tv.tv_sec);
            len = strftime(time_string, sizeof(time_string), "%H:%M:%S", tm);
            break;
        case TIMESTAMP_ISO8601:
            // "YYYY-MM-DDThh:mm:ss.sss" (ISO-8601)
            tm = localtime(&tv.tv_sec);
            len = strftime(time_string, sizeof(time_string), "%Y-%m-%dT%H:%M:%S", tm);
            break;
        default:
            return NULL;
    }

    // Append milliseconds to all timestamps
    if (len)
    {
        len = snprintf(time_string + len, TIME_STRING_SIZE_MAX - len, ".%03ld", (long)tv.tv_usec / 1000);
    }

    return (len < TIME_STRING_SIZE_MAX) ? time_string : NULL;
}

void delay(long ms)
{
    struct timespec ts;

    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;

    nanosleep(&ts, NULL);
}
