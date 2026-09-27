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

#include "readline.h"
#include "print.h"
#include "misc.h"
#include <assert.h>

#define RL_LINE_LENGTH_MAX PATH_MAX

typedef struct readline_s
{
    char line[RL_LINE_LENGTH_MAX];
    char *history[RL_HISTORY_MAX];
    char history_filename[PATH_MAX];
    char prompt[RL_PROMPT_LENGTH_MAX];
    int prompt_length;
    int history_count;
    int history_index;
    int line_length;
    int cursor_pos;
    int escape;
} readline_t;

void print_prompt(readline_t *rl)
{
    clear_line();
    printf("%s", rl->prompt);
    printf("\r"); // Move the cursor back to the beginning
    for (int i = 0; i < rl->prompt_length; ++i)
    {
        printf("\x1b[C"); // Move the cursor right
    }
}

void print_line(readline_t *rl)
{
    clear_line();
    printf("%s%s", rl->prompt, rl->line);
    printf("\r"); // Move the cursor back to the beginning
    for (int i = 0; i < rl->prompt_length + rl->cursor_pos; ++i)
    {
        printf("\x1b[C"); // Move the cursor right
    }
}

readline_t *readline_create(const char *history_filename)
{
    readline_t *rl = malloc(sizeof(readline_t));
    if (rl == NULL)
        return NULL;

    readline_reinit(rl, history_filename);
    return rl;
}

void readline_reinit(readline_t *rl, const char *history_filename)
{
    assert(rl != NULL);

    if (history_filename && history_filename[0] != '\0')
    {
        strncpy(rl->history_filename, history_filename, PATH_MAX - 1);
        rl->history_filename[PATH_MAX - 1] = '\0';
    }
    else
    {
        rl->history_filename[0] = '\0';
    }

    rl->prompt[0] = '\0';
    rl->prompt_length = 0;

    rl->history_count = 0;
    rl->history_index = 0;

    for (int i = 0; i < RL_HISTORY_MAX; ++i)
    {
        rl->history[i] = NULL;
    }

    rl->line[0] = 0;
    rl->line_length = 0;
    rl->cursor_pos = 0;
    rl->escape = 0;
}

void readline_load(readline_t *rl)
{
    if (rl->history_filename[0] == '\0')
        return;

    FILE *fp = fopen(rl->history_filename, "r");
    if (!fp)
        return;

    char buf[RL_LINE_LENGTH_MAX];

    while (fgets(buf, sizeof(buf), fp))
    {
        size_t len = strlen(buf);

        // strip newline
        if (len > 0 && buf[len - 1] == '\n')
            buf[--len] = '\0';

        if (len == 0)
            continue;

        if (rl->history_count < RL_HISTORY_FILE_MAX)
        {
            rl->history[rl->history_count++] = strndup(buf, len);
        }
        else
        {
            // trim top and append line to tail
            free(rl->history[0]);
            memmove(&rl->history[0], &rl->history[1],
                    (RL_HISTORY_FILE_MAX - 1) * sizeof(char *));
            rl->history[RL_HISTORY_FILE_MAX - 1] = strndup(buf, len);
        }
    }

    fclose(fp);

    rl->history_index = rl->history_count;
}

void readline_save(readline_t *rl)
{
    if (rl->history_count == 0)
        return;

    FILE *fp = fopen(rl->history_filename, "w");
    if (!fp)
    {
        tio_warning_printf("Could not save history %s", rl->history_filename);
        return;
    }

    int start = 0;
    if (rl->history_count > RL_HISTORY_FILE_MAX)
    {
        start = rl->history_count - RL_HISTORY_FILE_MAX;
    }

    for (int i = start; i < rl->history_count; i++)
    {
        fprintf(fp, "%s\n", rl->history[i]);
    }

    fclose(fp);
}

void readline_set_prompt(readline_t *rl, const char *prompt)
{
    strncpy(rl->prompt, prompt, RL_PROMPT_LENGTH_MAX - 1);
    rl->prompt[RL_PROMPT_LENGTH_MAX - 1] = '\0';
    rl->prompt_length = strlen(rl->prompt);
}

char * readline_get(readline_t *rl)
{
    assert(rl != NULL);
    return rl->line;
}

void readline_prompt_for_input(readline_t *rl)
{
    assert(rl != NULL);

    rl->line[0] = 0;
    rl->line_length = 0;
    rl->cursor_pos = 0;
    rl->escape = 0;
    print_line(rl);
}

static void readline_input_char(readline_t *rl, char input_char)
{
    assert(rl != NULL);

    if (rl->line_length < RL_LINE_LENGTH_MAX - 1)
    {
        memmove(&rl->line[rl->cursor_pos + 1], &rl->line[rl->cursor_pos], rl->line_length - rl->cursor_pos);
        rl->line[rl->cursor_pos] = input_char;
        rl->line_length++;
        rl->cursor_pos++;
        rl->line[rl->line_length] = '\0';
        print_line(rl);
    }
    rl->escape = 0;
}

static void readline_input_cr(readline_t *rl)
{
    rl->line[rl->line_length] = '\0';

    if (rl->line_length > 0)
    {
        // Different line only
        if (rl->history_count == 0 ||
            (rl->history_count > 0 &&
             ! (rl->history[rl->history_count - 1][rl->line_length] == '\0' &&
                strncmp(rl->history[rl->history_count - 1], rl->line, rl->line_length) == 0)) )
        {
            // Save to history
            if (rl->history_count < RL_HISTORY_MAX)
            {
                rl->history[rl->history_count] = strndup(rl->line, rl->line_length);
                rl->history_count++;
            }
            else
            {
                free(rl->history[0]);
                memmove(&rl->history[0], &rl->history[1], (RL_HISTORY_MAX - 1) * sizeof(char*));
                rl->history[RL_HISTORY_MAX - 1] = strndup(rl->line, rl->line_length);
            }
        }
    }

    if (option.local_echo == false)
    {
        clear_line();
    }
    else
    {
        print("\r\n");
    }

    rl->line_length = 0;
    rl->cursor_pos = 0;
    rl->history_index = rl->history_count;
    rl->escape = 0;
}

static void readline_input_bs(readline_t *rl)
{
    if (rl->cursor_pos > 0)
    {
        memmove(&rl->line[rl->cursor_pos - 1], &rl->line[rl->cursor_pos], rl->line_length - rl->cursor_pos);
        rl->line_length--;
        rl->cursor_pos--;
        rl->line[rl->line_length] = '\0';
        print_line(rl);
    }
    rl->escape = 0;
}

static void readline_input_escape(readline_t *rl)
{
    rl->escape = 1;
}

static void readline_input_left_bracket(readline_t *rl)
{
    if (rl->escape == 1)
    {
        rl->escape = 2;
    }
    else
    {
        readline_input_char(rl, '[');
        rl->escape = 0;
    }
}

static void readline_input_A(readline_t *rl)
{
    if (rl->escape == 2)
    {
        // Up arrow
        if (rl->history_index > 0)
        {
            rl->history_index--;
            strncpy(rl->line, rl->history[rl->history_index], RL_LINE_LENGTH_MAX-1);
            rl->line_length = strlen(rl->line);
            rl->cursor_pos = rl->line_length;
            print_line(rl);
        }
    }
    else
    {
        readline_input_char(rl, 'A');
    }

    rl->escape = 0;
}

static void readline_input_B(readline_t *rl)
{
    if (rl->escape == 2)
    {
        // Down arrow
        if (rl->history_index < rl->history_count - 1)
        {
            rl->history_index++;
            strncpy(rl->line, rl->history[rl->history_index], RL_LINE_LENGTH_MAX-1);
            rl->line_length = strlen(rl->line);
            rl->cursor_pos = rl->line_length;
            print_line(rl);
        }
        else if (rl->history_index == rl->history_count - 1)
        {
            rl->history_index++;
            rl->line_length = 0;
            rl->cursor_pos = 0;
            rl->line[rl->line_length] = '\0';
            print_line(rl);
        }
    }
    else
    {
        readline_input_char(rl, 'B');
    }

    rl->escape = 0;
}

static void readline_input_C(readline_t *rl)
{
    if (rl->escape == 2)
    {
        // Right arrow
        if (rl->cursor_pos < rl->line_length)
        {
            rl->cursor_pos++;
            print("\x1b[C");
        }
    }
    else
    {
        readline_input_char(rl, 'C');
    }

    rl->escape = 0;
}

static void readline_input_D(readline_t *rl)
{
    if (rl->escape == 2)
    {
        // Left arrow
        if (rl->cursor_pos > 0)
        {
            rl->cursor_pos--;
            print("\b");
        }
    }
    else
    {
        readline_input_char(rl, 'D');
    }

    rl->escape = 0;
}

void readline_input(readline_t *rl, char input_char)
{
    switch (input_char)
    {
        case '\r': // Carriage return
            readline_input_cr(rl);
            break;

        case 127: // Backspace
            readline_input_bs(rl);
            break;

        case 27: // Escape
            readline_input_escape(rl);
            break;

        case '[':
            readline_input_left_bracket(rl);
            break;

        case 'A':
            readline_input_A(rl);
            break;

        case 'B':
            readline_input_B(rl);
            break;

        case 'C':
            readline_input_C(rl);
            break;

        case 'D':
            readline_input_D(rl);
            break;

        default:
            readline_input_char(rl, input_char);
            break;
    }
}
