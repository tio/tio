/*
 * tio - a simple TTY terminal I/O application
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

#include <stdio.h>
#include <stdbool.h>
#include "print.h"

bool print_tainted = false;
bool print_color_mode = false;
const char *print_color = ANSI_COLOR_YELLOW;

void print_hex(char c)
{
  if ((c == '\n') || (c == '\r'))
  {
    printf("%c", c);
  }
  else
  {
    printf("%02x ", (unsigned char) c);
  }

  fflush(stdout);
}

void print_normal(char c)
{
  putchar(c);
  fflush(stdout);
}

void print_set_color_mode(bool mode)
{
  print_color_mode = mode;
}
