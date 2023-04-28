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

#define __STDC_WANT_LIB_EXT2__ 1   // To access vasprintf

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <libgen.h>
#include "options.h"
#include "print.h"
#include "error.h"

#define IS_ESC_CSI_INTERMEDIATE_CHAR(c) ((c >= 0x20) && (c <= 0x3F))
#define IS_ESC_END_CHAR(c)              ((c >= 0x30) && (c <= 0x7E))
#define IS_CTRL_CHAR(c)                 ((c >= 0x00) && (c <= 0x1F))

static FILE *fp = NULL;
static char file_buffer[BUFSIZ];
static const char *log_filename = NULL;

static char *date_time(void)
{
    static char date_time_string[50];
    struct tm *tm;
    struct timeval tv;

    gettimeofday(&tv, NULL);

    tm = localtime(&tv.tv_sec);
    strftime(date_time_string, sizeof(date_time_string), "%Y-%m-%dT%H:%M:%S", tm);

    return date_time_string;
}

int log_open(const char *filename)
{
    static char automatic_filename[400];

    if (filename == NULL)
    {
        // Generate filename if none provided ("tio_DEVICE_YYYY-MM-DDTHH:MM:SS.log")
        sprintf(automatic_filename, "tio_%s_%s.log", basename((char *)option.tty_device), date_time());
        filename = automatic_filename;
    }

    log_filename = filename;

    // Open log file
    if (option.log_append)
    {
        // Appends to existing log file
        fp = fopen(filename, "a+");
    }
    else
    {
        // Truncates existing log file
        fp = fopen(filename, "w+");
    }
    if (fp == NULL)
    {
        tio_warning_printf("Could not open log file %s (%s)", filename, strerror(errno));
        return -1;
    }

    // Enable line buffering
    setvbuf(fp, file_buffer, _IOLBF, BUFSIZ);

    return 0;
}

bool log_strip(char c)
{
    static char previous_char = 0;
    static bool esc_sequence = false;
    bool strip = false;

    /* Detect if character should be stripped or not */
    switch (c)
    {
        case 0xa:
            /* Line feed / new line */
            /* Reset ESC sequence just in case something went wrong with the
             * escape sequence parsing. */
            esc_sequence = false;
            break;

        case 0x1b:
            /* Escape */
            strip = true;
            break;

        case 0x5b:
            /* Left bracket */
            if (previous_char == 0x1b)
            {
                // Start of ESC sequence
                esc_sequence = true;
                strip = true;
            }
            break;

        default:
            if (IS_CTRL_CHAR(c))
            {
                /* Strip ASCII control characters */
                strip = true;
                break;
            }
            else
            if ((esc_sequence) && (IS_ESC_CSI_INTERMEDIATE_CHAR(c)))
            {
                strip = true;
                break;
            }
            else
            if ((esc_sequence) && (IS_ESC_END_CHAR(c)))
            {
                esc_sequence = false;
                strip = true;
                break;
            }
            break;
    }

    previous_char = c;

    return strip;
}

void log_printf(const char *format, ...)
{
    if (fp == NULL)
    {
        return;
    }

    char *line;

    va_list(args);
    va_start(args, format);
    vasprintf(&line, format, args);
    va_end(args);

    fwrite(line, strlen(line), 1, fp);

    free(line);
}

void log_putc(char c)
{
    if (fp == NULL)
    {
        return;
    }

    if (option.log_strip)
    {
        if (!log_strip(c))
        {
            fputc(c, fp);
        }
    }
    else
    {
        fputc(c, fp);
    }
}

void log_close(void)
{
    if (fp != NULL)
    {
        fclose(fp);
        fp = NULL;
        log_filename = NULL;
    }
}

void log_exit(void)
{
    if (option.log)
    {
        tio_printf("Saved log to file %s", log_filename);
        log_close();
    }
}

const char *log_get_filename(void)
{
    return log_filename;
}
