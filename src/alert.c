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
#include <string.h>
#include "error.h"
#include "print.h"
#include "options.h"

enum alert_t alert_option_parse(const char *arg)
{
    enum alert_t alert = option.alert; // Default

    if (arg != NULL)
    {
        if (strcmp(arg, "none") == 0)
        {
            return ALERT_NONE;
        }
        else if (strcmp(arg, "bell") == 0)
        {
            return ALERT_BELL;
        }
        else if (strcmp(arg, "blink") == 0)
        {
            return ALERT_BLINK;
        }
    }

    return alert;
}

void blink_background(void)
{
    // Turn on reverse video
    printf("\e[?5h");
    fflush(stdout);

    usleep(200*1000);

    // Turn on normal video
    printf("\e[?5l");
    fflush(stdout);
}

void sound_bell(void)
{
    // Audio bell
    printf("\a");
    fflush(stdout);
}

void alert_connect(void)
{
    switch (option.alert)
    {
        case ALERT_NONE:
            break;
        case ALERT_BELL:
            sound_bell();
            break;
        case ALERT_BLINK:
            blink_background();
            break;
        default:
            break;
    }
}

void alert_disconnect(void)
{
    switch (option.alert)
    {
        case ALERT_NONE:
            break;
        case ALERT_BELL:
            sound_bell();
            usleep(200*1000);
            sound_bell();
            break;
        case ALERT_BLINK:
            blink_background();
            usleep(200*1000);
            blink_background();
            break;
        default:
            break;
    }
}
