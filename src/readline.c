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

#define MAX_LINE_LEN PATH_MAX
#define MAX_HISTORY 1000

static char line[MAX_LINE_LEN] = {};
static int history_count = 0;
static int history_index = 0;
static int line_length = 0;
static int cursor_pos = 0;
static int escape = 0;

static char *history[MAX_HISTORY];

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
    history_count = 0;
    history_index = 0;

    for (int i = 0; i < MAX_HISTORY; ++i)
    {
        history[i] = NULL;
    }

    line[0] = 0;
    line_length = 0;
    cursor_pos = 0;
    escape = 0;
}

char * readline_get(void)
{
    return line;
}

static void readline_input_char(char input_char)
{
    if (line_length < MAX_LINE_LEN - 1)
    {
        memmove(&line[cursor_pos + 1], &line[cursor_pos], line_length - cursor_pos);
        line[cursor_pos] = input_char;
        line_length++;
        cursor_pos++;
        line[line_length] = '\0';
        print_line(line, cursor_pos);
    }
    escape = 0;
}

static void readline_input_cr(void)
{
    if (line_length > 0)
    {
        // Save to history
        if (history_count < MAX_HISTORY)
        {
            history[history_count] = strndup(line, line_length);
            history_count++;
        }
        else
    {
            free(history[0]);
            memmove(&history[0], &history[1], (MAX_HISTORY - 1) * sizeof(char*));
            history[MAX_HISTORY - 1] = strndup(line, line_length);
        }
    }

    line[line_length] = '\0';
    if (option.local_echo == false)
    {
        clear_line();
    }
    else
    {
        print("\r\n");
    }

    line_length = 0;
    cursor_pos = 0;
    history_index = history_count;
    escape = 0;
}

static void readline_input_bs(void)
{
    if (cursor_pos > 0)
    {
        memmove(&line[cursor_pos - 1], &line[cursor_pos], line_length - cursor_pos);
        line_length--;
        cursor_pos--;
        line[line_length] = '\0';
        print_line(line, cursor_pos);
    }
    escape = 0;
}

static void readline_input_escape(void)
{
    escape = 1;
}

static void readline_input_left_bracket(void)
{
    if (escape == 1)
    {
        escape = 2;
    }
    else
    {
        escape = 0;
    }
}

static void readline_input_A(void)
{
    if (escape == 2)
    {
        // Up arrow
        if (history_index > 0)
        {
            history_index--;
            strncpy(line, history[history_index], MAX_LINE_LEN-1);
            line_length = strlen(line);
            cursor_pos = line_length;
            print_line(line, cursor_pos);
        }
    }
    else
    {
        readline_input_char('A');
    }

    escape = 0;
}

static void readline_input_B(void)
{
    if (escape == 2)
    {
        // Down arrow
        if (history_index < history_count - 1)
        {
            history_index++;
            strncpy(line, history[history_index], MAX_LINE_LEN-1);
            line_length = strlen(line);
            cursor_pos = line_length;
            print_line(line, cursor_pos);
        }
        else if (history_index == history_count - 1)
        {
            history_index++;
            line_length = 0;
            cursor_pos = 0;
            line[line_length] = '\0';
            print_line(line, cursor_pos);
        }
    }
    else
    {
        readline_input_char('B');
    }

    escape = 0;
}

static void readline_input_C(void)
{
    if (escape == 2)
    {
        // Right arrow
        if (cursor_pos < line_length)
        {
            cursor_pos++;
            print("\x1b[C");
        }
    }
    else
    {
        readline_input_char('C');
    }

    escape = 0;
}

static void readline_input_D(void)
{
    if (escape == 2)
    {
        // Left arrow
        if (cursor_pos > 0)
        {
            cursor_pos--;
            print("\b");
        }
    }
    else
    {
        readline_input_char('D');
    }

    escape = 0;
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
