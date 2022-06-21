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

#include <stdio.h>
#include <stdbool.h>
#include "options.h"
#include "print.h"

bool print_tainted = false;
char ansi_format[30];

void print_hex(char c)
{
  printf("%02x ", (unsigned char) c);
}

void print_normal(char c)
{
  putchar(c);
}

void print_init_ansi_formatting()
{
  // Set bold text with user defined ANSI color
  sprintf(ansi_format, "\e[1;38;5;%dm", option.color);
}
