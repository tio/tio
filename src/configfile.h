/*
 * tio - a simple serial terminal I/O tool
 *
 * Copyright (c) 2020  Liam Beguin
 * Copyright (c) 2022  Martin Lund
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

struct config_t
{
	const char *user;

	char *path;
	char *section_name;
	char *match;

	char *tty;
	char *flow;
	char *parity;
	char *log_filename;
	char *map;
};

void config_file_print();
void config_file_parse(const int argc, char *argv[]);
void config_exit(void);
