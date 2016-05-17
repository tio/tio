/*
 * tio - a simple TTY terminal I/O application
 *
 * Copyright (c) 2014-2016  Martin Lund
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

#ifndef PRINT_H
#define PRINT_H

#define ANSI_COLOR_GRAY      "\x1b[1;30m"
#define ANSI_COLOR_RED       "\x1b[1;31m"
#define ANSI_COLOR_GREEN     "\x1b[1;32m"
#define ANSI_COLOR_YELLOW    "\x1b[1;33m"
#define ANSI_COLOR_BLUE      "\x1b[1;34m"
#define ANSI_COLOR_PINK      "\x1b[1;35m"
#define ANSI_COLOR_CYAN      "\x1b[1;36m"
#define ANSI_COLOR_WHITE     "\x1b[1;37m"
#define ANSI_COLOR_RESET     "\x1b[0m"

#define color_printf(format, args...) \
   fprintf (stdout, "\r" ANSI_COLOR_YELLOW format ANSI_COLOR_RESET "\r\n", ## args); \
   fflush(stdout);

#define warning_printf(format, args...) \
   fprintf (stdout, "\rWarning: " format "\r\n", ## args); \
   fflush(stdout);

#ifdef DEBUG
#define debug_printf(format, args...) \
   fprintf (stdout, "[debug] " format, ## args)
#define debug_printf_raw(format, args...) \
   fprintf (stdout, "" format, ## args)
#else
#define debug_printf(format, args...)
#define debug_printf_raw(format, args...)
#endif

#endif
