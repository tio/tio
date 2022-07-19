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

#pragma once

#include <stdbool.h>
#include <limits.h>
#include <termios.h>
#include <sys/param.h>

enum timestamp_t
{
    TIMESTAMP_NONE,
    TIMESTAMP_24HOUR,
    TIMESTAMP_24HOUR_START,
    TIMESTAMP_24HOUR_DELTA,
    TIMESTAMP_ISO8601,
    TIMESTAMP_END,
};

enum timestamp_t timestamp_option_parse(const char *arg);

/* Options */
struct option_t
{
    const char *tty_device;
    unsigned int baudrate;
    int databits;
    char *flow;
    int stopbits;
    char *parity;
    int output_delay;
    int output_line_delay;
    unsigned int dtr_pulse_duration;
    unsigned int rts_pulse_duration;
    unsigned int cts_pulse_duration;
    unsigned int dsr_pulse_duration;
    unsigned int dcd_pulse_duration;
    unsigned int ri_pulse_duration;
    bool no_autoconnect;
    bool log;
    bool log_strip;
    bool local_echo;
    enum timestamp_t timestamp;
    const char *log_filename;
    const char *map;
    const char *socket;
    int color;
    bool hex_mode;
    unsigned char prefix_code;
    unsigned char prefix_key;
};

extern struct option_t option;

void options_print();
void options_parse(int argc, char *argv[]);
void options_parse_final(int argc, char *argv[]);

void line_pulse_duration_option_parse(const char *arg);
