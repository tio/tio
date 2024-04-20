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

#pragma once

typedef enum
{
    TIMESTAMP_NONE,
    TIMESTAMP_24HOUR,
    TIMESTAMP_24HOUR_START,
    TIMESTAMP_24HOUR_DELTA,
    TIMESTAMP_ISO8601,
    TIMESTAMP_END,
} timestamp_t;

char *timestamp_current_time(void);
const char* timestamp_state_to_string(timestamp_t timestamp);
timestamp_t timestamp_option_parse(const char *arg);
