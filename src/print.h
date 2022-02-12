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

#pragma once

#include <stdbool.h>
#include "misc.h"
#include "error.h"

extern bool print_tainted;
extern bool print_color_mode;
extern const char *print_color;

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
{ \
  if (print_color_mode) \
    fprintf (stdout, "\r%s" format ANSI_COLOR_RESET "\r\n", print_color, ## args); \
  else \
    fprintf (stdout, "\r" format "\r\n", ## args); \
  fflush(stdout); \
}

#define warning_printf(format, args...) \
{ \
  color_printf("[%s] Warning: " format, current_time(), ## args); \
  fflush(stdout); \
}

#define tio_printf(format, args...) \
{ \
  if (print_tainted) \
    putchar('\n'); \
  color_printf("[%s] " format, current_time(), ## args); \
  print_tainted = false; \
}

#define error_printf(format, args...) \
  snprintf(error[0], 1000, format, ## args);

#define error_printf_silent(format, args...) \
  snprintf(error[1], 1000, format, ## args);

#ifdef DEBUG
#define debug_printf(format, args...) \
  fprintf (stdout, "[debug] " format, ## args)
#define debug_printf_raw(format, args...) \
  fprintf (stdout, "" format, ## args)
#else
#define debug_printf(format, args...)
#define debug_printf_raw(format, args...)
#endif

void print_hex(char c);
void print_normal(char c);
void print_set_color_mode(bool mode);
