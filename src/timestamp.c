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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "error.h"
#include "print.h"
#include "options.h"
#include "timestamp.h"

#define TIME_STRING_SIZE_MAX 24

char *timestamp_current_time(void)
{
    static char time_string[TIME_STRING_SIZE_MAX];
    static struct timeval tv, tv_now, tv_start, tv_previous;
    static bool first = true;
    struct tm *tm;
    size_t len;

    // Get current time value
    gettimeofday(&tv_now, NULL);

    if (first)
    {
        tv_start = tv_now;
        first = false;
    }

    // Add formatted timestamp
    switch (option.timestamp)
    {
        case TIMESTAMP_NONE:
        case TIMESTAMP_24HOUR:
            // "hh:mm:ss.sss" (24 hour format)
            tv = tv_now;
            tm = localtime(&tv.tv_sec);
            len = strftime(time_string, sizeof(time_string), "%H:%M:%S", tm);
            break;
        case TIMESTAMP_24HOUR_START:
            // "hh:mm:ss.sss" (24 hour format relative to start time)
            timersub(&tv_now, &tv_start, &tv);
            tm = gmtime(&tv.tv_sec);
            len = strftime(time_string, sizeof(time_string), "%H:%M:%S", tm);
            break;
        case TIMESTAMP_24HOUR_DELTA:
            // "hh:mm:ss.sss" (24 hour format relative to previous time stamp)
            timersub(&tv_now, &tv_previous, &tv);
            tm = gmtime(&tv.tv_sec);
            len = strftime(time_string, sizeof(time_string), "%H:%M:%S", tm);
            break;
        case TIMESTAMP_ISO8601:
            // "YYYY-MM-DDThh:mm:ss.sss" (ISO-8601)
            tv = tv_now;
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

    // Save previous time value for next run
    tv_previous = tv_now;

    return (len < TIME_STRING_SIZE_MAX) ? time_string : NULL;
}

const char* timestamp_state_to_string(timestamp_t timestamp)
{
    switch (timestamp)
    {
        case TIMESTAMP_NONE:
            return "disabled";
            break;

        case TIMESTAMP_24HOUR:
            return "24hour";
            break;

        case TIMESTAMP_24HOUR_START:
            return "24hour-start";
            break;

        case TIMESTAMP_24HOUR_DELTA:
            return "24hour-delta";
            break;

        case TIMESTAMP_ISO8601:
            return "iso8601";
            break;

        default:
            return "unknown";
            break;
    }
}

timestamp_t timestamp_option_parse(const char *arg)
{
    timestamp_t timestamp = TIMESTAMP_24HOUR; // Default

    if (arg != NULL)
    {
        if (strcmp(arg, "24hour") == 0)
        {
            return TIMESTAMP_24HOUR;
        }
        else if (strcmp(arg, "24hour-start") == 0)
        {
            return TIMESTAMP_24HOUR_START;
        }
        else if (strcmp(arg, "24hour-delta") == 0)
        {
            return TIMESTAMP_24HOUR_DELTA;
        }
        else if (strcmp(arg, "iso8601") == 0)
        {
            return TIMESTAMP_ISO8601;
        }
    }

    return timestamp;
}
