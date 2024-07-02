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

#include "print.h"
#include "misc.h"

#define RL_LINE_LENGTH_MAX PATH_MAX
#define RL_HISTORY_MAX 1000

static char rl_line[RL_LINE_LENGTH_MAX] = {};
static char *rl_history[RL_HISTORY_MAX];
static int rl_history_count = 0;
static int rl_history_index = 0;
static int rl_line_length = 0;
static int rl_cursor_pos = 0;
static int rl_escape = 0;

static void print_line(const char *string, int cursor_pos)
{
    clear_line();
    print("%s", string);
    print("\r"); // Move the cursor back to the beginning
    for (int i = 0; i < cursor_pos; ++i)
    {
        print("\x1b[C"); // Move the cursor right
    }
}

void readline_init(void)
{
    rl_history_count = 0;
    rl_history_index = 0;

    for (int i = 0; i < RL_HISTORY_MAX; ++i)
    {
        rl_history[i] = NULL;
    }

    rl_line[0] = 0;
    rl_line_length = 0;
    rl_cursor_pos = 0;
    rl_escape = 0;
}

char * readline_get(void)
{
    return rl_line;
}

static void readline_input_char(char input_char)
{
    if (rl_line_length < RL_LINE_LENGTH_MAX - 1)
    {
        memmove(&rl_line[rl_cursor_pos + 1], &rl_line[rl_cursor_pos], rl_line_length - rl_cursor_pos);
        rl_line[rl_cursor_pos] = input_char;
        rl_line_length++;
        rl_cursor_pos++;
        rl_line[rl_line_length] = '\0';
        print_line(rl_line, rl_cursor_pos);
    }
    rl_escape = 0;
}

static void readline_input_cr(void)
{
    if (rl_line_length > 0)
    {
        // Save to history
        if (rl_history_count < RL_HISTORY_MAX)
        {
            rl_history[rl_history_count] = strndup(rl_line, rl_line_length);
            rl_history_count++;
        }
        else
    {
            free(rl_history[0]);
            memmove(&rl_history[0], &rl_history[1], (RL_HISTORY_MAX - 1) * sizeof(char*));
            rl_history[RL_HISTORY_MAX - 1] = strndup(rl_line, rl_line_length);
        }
    }

    rl_line[rl_line_length] = '\0';
    if (option.local_echo == false)
    {
        clear_line();
    }
    else
    {
        print("\r\n");
    }

    rl_line_length = 0;
    rl_cursor_pos = 0;
    rl_history_index = rl_history_count;
    rl_escape = 0;
}

static void readline_input_bs(void)
{
    if (rl_cursor_pos > 0)
    {
        memmove(&rl_line[rl_cursor_pos - 1], &rl_line[rl_cursor_pos], rl_line_length - rl_cursor_pos);
        rl_line_length--;
        rl_cursor_pos--;
        rl_line[rl_line_length] = '\0';
        print_line(rl_line, rl_cursor_pos);
    }
    rl_escape = 0;
}

static void readline_input_escape(void)
{
    rl_escape = 1;
}

static void readline_input_left_bracket(void)
{
    if (rl_escape == 1)
    {
        rl_escape = 2;
    }
    else
    {
        rl_escape = 0;
    }
}

static void readline_input_A(void)
{
    if (rl_escape == 2)
    {
        // Up arrow
        if (rl_history_index > 0)
        {
            rl_history_index--;
            strncpy(rl_line, rl_history[rl_history_index], RL_LINE_LENGTH_MAX-1);
            rl_line_length = strlen(rl_line);
            rl_cursor_pos = rl_line_length;
            print_line(rl_line, rl_cursor_pos);
        }
    }
    else
    {
        readline_input_char('A');
    }

    rl_escape = 0;
}

static void readline_input_B(void)
{
    if (rl_escape == 2)
    {
        // Down arrow
        if (rl_history_index < rl_history_count - 1)
        {
            rl_history_index++;
            strncpy(rl_line, rl_history[rl_history_index], RL_LINE_LENGTH_MAX-1);
            rl_line_length = strlen(rl_line);
            rl_cursor_pos = rl_line_length;
            print_line(rl_line, rl_cursor_pos);
        }
        else if (rl_history_index == rl_history_count - 1)
        {
            rl_history_index++;
            rl_line_length = 0;
            rl_cursor_pos = 0;
            rl_line[rl_line_length] = '\0';
            print_line(rl_line, rl_cursor_pos);
        }
    }
    else
    {
        readline_input_char('B');
    }

    rl_escape = 0;
}

static void readline_input_C(void)
{
    if (rl_escape == 2)
    {
        // Right arrow
        if (rl_cursor_pos < rl_line_length)
        {
            rl_cursor_pos++;
            print("\x1b[C");
        }
    }
    else
    {
        readline_input_char('C');
    }

    rl_escape = 0;
}

static void readline_input_D(void)
{
    if (rl_escape == 2)
    {
        // Left arrow
        if (rl_cursor_pos > 0)
        {
            rl_cursor_pos--;
            print("\b");
        }
    }
    else
    {
        readline_input_char('D');
    }

    rl_escape = 0;
}

void readline_input(char input_char)
{
    switch (input_char)
    {
        case '\r': // Carriage return
            readline_input_cr();
            break;

        case 127: // Backspace
            readline_input_bs();
            break;

        case 27: // Escape
            readline_input_escape();
            break;

        case '[':
            readline_input_left_bracket();
            break;

        case 'A':
            readline_input_A();
            break;

        case 'B':
            readline_input_B();
            break;

        case 'C':
            readline_input_C();
            break;

        case 'D':
            readline_input_D();
            break;

        default:
            readline_input_char(input_char);
            break;
    }
}
