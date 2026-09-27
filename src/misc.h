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
#include <stdio.h>

#define POLL_NOWAIT (0)
#define POLL_FOREVER (-1)

#define TOSTRING_(x) #x
#define TOSTR(x) TOSTRING_(x)

#define UNUSED(expr) do { (void)(expr); } while (0)

void delay(long ms);
int ctrl_key_code(unsigned char key);
int ctrl_key_char(int key_code);
bool regex_match(const char *string, const char *pattern);
unsigned long djb2_hash(const unsigned char *str);
void *base62_encode(unsigned long num, char *output);
int read_poll(int fd, void *data, size_t len, int timeout);
ssize_t write_poll(int fd, const void *data, size_t len, int timeout);
double get_current_time(void);
bool match_patterns(const char *string, const char *patterns);
int execute_shell_command(int fd, const char *command);
void clear_line();

#if defined(__linux__)
void terminate_shell_command(void);
#endif
