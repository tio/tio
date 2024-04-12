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

#include <stdbool.h>

#define LINE_HIGH true
#define LINE_LOW false

extern bool interactive_mode;
extern bool map_i_nl_cr;
extern bool map_i_cr_nl;
extern bool map_ign_cr;

void stdout_configure(void);
void stdin_configure(void);
void tty_configure(void);
int tty_connect(void);
void tty_wait_for_device(void);
void list_serial_devices(void);
void tty_input_thread_create(void);
void tty_input_thread_wait_ready(void);
void tty_line_set(int fd, int mask, bool value);
void tty_line_toggle(int fd, int mask);
void tty_line_config(int mask, bool value);
void tty_line_config_apply(void);
