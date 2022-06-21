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

static FILE *fp;
static bool log_error = false;
static char file_buffer[BUFSIZ];

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

void log_open(const char *filename)
{
    static char automatic_filename[400];

    if (filename == NULL)
    {
        // Generate filename if none provided ("tio_DEVICE_YYYY-MM-DDTHH:MM:SS.log")
        sprintf(automatic_filename, "tio_%s_%s.log", basename((char *)option.tty_device), date_time());
        filename = automatic_filename;
        option.log_filename = automatic_filename;
    }

    // Open log file in append write mode
    fp = fopen(filename, "a+");
    if (fp == NULL)
    {
        log_error = true;
        exit(EXIT_FAILURE);
    }

    // Enable full buffering
    setvbuf(fp, file_buffer, _IOFBF, BUFSIZ);
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
    if (fp != NULL)
    {
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
}

void log_close(void)
{
    if (fp != NULL)
    {
        fflush(fp);
        fclose(fp);
    }
}

void log_exit(void)
{
    if (option.log)
    {
        log_close();
    }

    if (log_error)
    {
        error_printf("Could not open log file %s (%s)", option.log_filename, strerror(errno));
    }
    else if (option.log)
    {
        tio_printf("Saved log to file %s", option.log_filename);
    }
}
