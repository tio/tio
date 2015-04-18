/*
 * Go TTY - The Really Simple Terminal Application
 *
 * Copyright (c) 2014  Martin Lund
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
    char tty_device[MAXPATHLEN];
    bool no_autoconnect;
    struct termios tio;
    int output_delay;
};

extern struct option_t option;

void parse_options(int argc, char *argv[]);

#endif
