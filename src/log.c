/*
 * tio - a simple TTY terminal I/O application
 *
 * Copyright (c) 2014-2017  Martin Lund
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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "tio/options.h"
#include "tio/print.h"
#include "tio/error.h"

static FILE *fp;
static bool log_error = false;

void log_open(const char *filename)
{
    fp = fopen(filename, "w+");

    if (fp == NULL)
    {
        log_error = true;
        exit(EXIT_FAILURE);
    }
    setvbuf(fp, NULL, _IOLBF, 0);
}

void log_write(char c)
{
    if (fp != NULL)
        fputc(c, fp);
}

#define IS_ESC_INTERMEDIATE_CHAR(c)    ((c <= 0x2F) && (c >= 0x20))
#define IS_ESC_END_CHAR(c)              ((c <= 0x7E) && (c >= 0x30))
#define IS_CSI_END_CHAR(c)              ((c <= 0x7E) && (c >= 0x40))

static void scan_ctrl_char(char *buf, int length)
{
    int i, j;
    size_t len = strnlen(buf, length);
    char c;

    i = j = 0;
    while (i < len) {
        c = buf[i++];
        if (iscntrl(c)) {
            switch (c)
                {
                case 8: /* backspace */
                    j--;
                    break;

                case 13: /* \r */
                    break;

                case 27: /* ESC */
                    c = buf[i++];

                    if (c == 0x5B) { /* CSI */
                        c = buf[i++];
                        while (!IS_CSI_END_CHAR(c)) {
                            c = buf[i++];
                        }
                    } else {
                        while (!IS_ESC_END_CHAR(c)) {
                            c = buf[i++];
                        }
                    }
                    break;
                default:
                    buf[j++] = c;
                    break;

                }
        } else {
            buf[j++] = c;
        }
    }
    buf[j] = '\0';
}

void log_writeline(char *l, int max_line_size, bool esc_strip)
{
    char *buf = NULL;

    if (fp == NULL)
        return;

    if (esc_strip) {
        buf = strdup(l);
        scan_ctrl_char(buf, max_line_size);
        fputs(buf, fp);
        free(buf);
    } else {
        buf = l;
        fputs(buf, fp);
    }

	fflush(fp);
}

void log_close(void)
{
    if (fp != NULL)
        fclose(fp);
}

void log_exit(void)
{
    if (option.log)
        log_close();

    if (log_error)
        error_printf("Could not open log file %s (%s)", option.log_filename, strerror(errno));
}
