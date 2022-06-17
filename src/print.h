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
#include "misc.h"
#include "error.h"
#include "options.h"

extern bool print_tainted;
extern char ansi_format[];

#define ANSI_RESET "\e[0m"

#define ansi_printf(format, args...) \
{ \
  if (option.color < 0) \
    fprintf (stdout, "\r" format "\r\n", ## args); \
  else \
    fprintf (stdout, "\r%s" format ANSI_RESET "\r\n", ansi_format, ## args); \
  fflush(stdout); \
}

#define ansi_error_printf(format, args...) \
{ \
  if (option.color < 0) \
    fprintf (stdout, "\r" format "\r\n", ## args); \
  else \
    fprintf (stderr, "\r%s" format ANSI_RESET "\r\n", ansi_format, ## args); \
  fflush(stderr); \
}

#define ansi_printf_raw(format, args...) \
{ \
  if (option.color < 0) \
    fprintf (stdout, format, ## args); \
  else \
    fprintf (stdout, "%s" format ANSI_RESET, ansi_format, ## args); \
  fflush(stdout); \
}

#define warning_printf(format, args...) \
{ \
  if (option.color < 0) \
    fprintf (stdout, "\r[%s] Warning: " format "\r\n", current_time(), ## args); \
  else \
    ansi_printf("[%s] Warning: " format, current_time(), ## args); \
  fflush(stdout); \
}

#define tio_printf(format, args...) \
{ \
  if (print_tainted) \
    putchar('\n'); \
  ansi_printf("[%s] " format, current_time(), ## args); \
  print_tainted = false; \
}

#define tio_error_printf(format, args...) \
{ \
  if (print_tainted) \
    putchar('\n'); \
  ansi_error_printf("[%s] " format, current_time(), ## args); \
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
void print_init_ansi_formatting(void);
