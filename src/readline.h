/*
 * tio - a serial device I/O tool
 *
 * Copyright (c) 2014-2024  Martin Lund
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

#define RL_HISTORY_MAX 500
#define RL_HISTORY_FILE_MAX 500
#define RL_PROMPT_LENGTH_MAX 16

typedef struct readline_s readline_t;

readline_t *readline_create(const char *history_filename);
void readline_reinit(readline_t *rl, const char *history_filename);
void readline_set_prompt(readline_t *rl, const char *prompt);
void readline_load(readline_t *rl);
void readline_save(readline_t *rl);
void readline_prompt_for_input(readline_t *rl);
void readline_input(readline_t *rl, char input_char);
char *readline_get(readline_t *rl);
void print_prompt(readline_t *rl);
