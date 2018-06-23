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

#ifndef OPTIONS_H
#define OPTIONS_H

#include <stdbool.h>
#include <limits.h>
#include <termios.h>
#include <sys/param.h>

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
    bool no_autoconnect;
    bool log;
    bool local_echo;
    const char *log_filename;
    const char *map;
};

extern struct option_t option;

void parse_options(int argc, char *argv[]);

#endif
