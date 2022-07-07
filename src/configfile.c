/*
 * tio - a simple serial terminal I/O tool
 *
 * Copyright (c) 2020-2022  Liam Beguin
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

#define _GNU_SOURCE

#include "config.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <getopt.h>
#include <termios.h>
#include <limits.h>
#include <unistd.h>
#include <regex.h>
#include <ini.h>
#include "options.h"
#include "configfile.h"
#include "misc.h"
#include "options.h"
#include "error.h"
#include "print.h"

static struct config_t *c;

static int get_match(const char *input, const char *pattern, char **match)
{
    int ret;
    int len = 0;
    regex_t re;
    regmatch_t m[2];
    char err[128];

    /* compile a regex with the pattern */
    ret = regcomp(&re, pattern, REG_EXTENDED);
    if (ret)
    {
        regerror(ret, &re, err, sizeof(err));
        fprintf(stderr, "regex error: %s", err);
        return ret;
    }

    /* try to match on input */
    ret = regexec(&re, input, 2, m, 0);
    if (!ret)
    {
        len = m[1].rm_eo - m[1].rm_so;
    }

    regfree(&re);

    if (len)
    {
        asprintf(match, "%s", &input[m[1].rm_so]);
    }

    return len;
}

/**
 * data_handler() - walk config file to load parameters matching user input
 *
 * INIH handler used to get all parameters from a given section
 */
static int data_handler(void *user, const char *section, const char *name,
                        const char *value)
{
    UNUSED(user);

    // If section matches current section being parsed
    if (!strcmp(section, c->section_name))
    {
        // Set configuration parameter if found
        if (!strcmp(name, "tty"))
        {
            asprintf(&c->tty, value, c->match);
            option.tty_device = c->tty;
        }
        else if (!strcmp(name, "baudrate"))
        {
            option.baudrate = string_to_long((char *)value);
        }
        else if (!strcmp(name, "databits"))
        {
            option.databits = atoi(value);
        }
        else if (!strcmp(name, "flow"))
        {
            asprintf(&c->flow, "%s", value);
            option.flow = c->flow;
        }
        else if (!strcmp(name, "stopbits"))
        {
            option.stopbits = atoi(value);
        }
        else if (!strcmp(name, "parity"))
        {
            asprintf(&c->parity, "%s", value);
            option.parity = c->parity;
        }
        else if (!strcmp(name, "output-delay"))
        {
            option.output_delay = atoi(value);
        }
        else if (!strcmp(name, "dtr-pulse-duration"))
        {
            option.dtr_pulse_duration = atoi(value);
        }
        else if (!strcmp(name, "no-autoconnect"))
        {
            if (!strcmp(value, "enable"))
            {
                option.no_autoconnect = true;
            }
            else if (!strcmp(value, "disable"))
            {
                option.no_autoconnect = false;
            }
        }
        else if (!strcmp(name, "log"))
        {
            if (!strcmp(value, "enable"))
            {
                option.log = true;
            }
            else if (!strcmp(value, "disable"))
            {
                option.log = false;
            }
        }
        else if (!strcmp(name, "log-file"))
        {
            asprintf(&c->log_filename, "%s", value);
            option.log_filename = c->log_filename;
        }
        else if (!strcmp(name, "log-strip"))
        {
            if (!strcmp(value, "enable"))
            {
                option.log_strip = true;
            }
            else if (!strcmp(value, "disable"))
            {
                option.log_strip = false;
            }
        }
        else if (!strcmp(name, "local-echo"))
        {
            if (!strcmp(value, "enable"))
            {
                option.local_echo = true;
            }
            else if (!strcmp(value, "disable"))
            {
                option.local_echo = false;
            }
        }
        else if (!strcmp(name, "hexadecimal"))
        {
            if (!strcmp(value, "enable"))
            {
                option.hex_mode = true;
            }
            else if (!strcmp(value, "disable"))
            {
                option.hex_mode = false;
            }
        }
        else if (!strcmp(name, "timestamp"))
        {
            if (!strcmp(value, "enable"))
            {
                option.timestamp = TIMESTAMP_24HOUR;
            }
            else if (!strcmp(value, "disable"))
            {
                option.timestamp = TIMESTAMP_NONE;
            }
        }
        else if (!strcmp(name, "timestamp-format"))
        {
            option.timestamp = timestamp_option_parse(value);
        }
        else if (!strcmp(name, "map"))
        {
            asprintf(&c->map, "%s", value);
            option.map = c->map;
        }
        else if (!strcmp(name, "color"))
        {
            if (!strcmp(value, "list"))
            {
                // Ignore
                return 0;
            }

            if (!strcmp(value, "none"))
            {
                option.color = -1; // No color
                return 0;
            }

            option.color = atoi(value);
            if ((option.color < 0) || (option.color > 255))
            {
                option.color = -1; // No color
            }
        }
        else if (!strcmp(name, "socket"))
        {
            asprintf(&c->socket, "%s", value);
            option.socket = c->socket;
        }
    }
    return 0;
}

/**
 * section_pattern_search_handler() - walk config file to find section matching user input
 *
 * INIH handler used to resolve the section matching the user's input.
 * This will look for the pattern element of each section and try to match it
 * with the user input.
 */
static int section_pattern_search_handler(void *user, const char *section, const char *varname,
                                          const char *varval)
{
    UNUSED(user);

    if (strcmp(varname, "pattern"))
        return 0;

    if (!strcmp(varval, c->user))
    {
        /* pattern matches as plain text */
        asprintf(&c->section_name, "%s", section);
    }
    else if (get_match(c->user, varval, &c->match) > 0)
    {
        /* pattern matches as regex */
        asprintf(&c->section_name, "%s", section);
    }

    return 0;
}

/**
 * section_pattern_search_handler() - walk config file to find section matching user input
 *
 * INIH handler used to resolve the section matching the user's input.
 * This will try to match the user input against a section with the name of the user input.
 */
static int section_name_search_handler(void *user, const char *section, const char *varname,
                                       const char *varval)
{
    UNUSED(user);
    UNUSED(varname);
    UNUSED(varval);

    if (!strcmp(section, c->user))
    {
        /* section name matches as plain text */
        asprintf(&c->section_name, "%s", section);
    }

    return 0;
}

static int resolve_config_file(void)
{
    asprintf(&c->path, "%s/tio/tiorc", getenv("XDG_CONFIG_HOME"));
    if (!access(c->path, F_OK))
    {
        return 0;
    }

    free(c->path);

    asprintf(&c->path, "%s/.config/tio/tiorc", getenv("HOME"));
    if (!access(c->path, F_OK))
    {
        return 0;
    }

    free(c->path);

    asprintf(&c->path, "%s/.tiorc", getenv("HOME"));
    if (!access(c->path, F_OK))
    {
        return 0;
    }

    free(c->path);

    c->path = NULL;

    return -EINVAL;
}

void config_file_parse(void)
{
    int ret;

    c = malloc(sizeof(struct config_t));
    if (!c)
    {
        fprintf(stderr, "Error: Insufficient memory allocation");
        exit(EXIT_FAILURE);
    } 
    memset(c, 0, sizeof(struct config_t));

    // Find config file
    if (resolve_config_file() != 0)
    {
        // None found - stop parsing
        return;
    }

    // Set user input which may be tty device or sub config
    c->user = option.tty_device;

    if (!c->user)
    {
        return;
    }

    // Parse default (unnamed) settings
    asprintf(&c->section_name, "%s", "");
    ret = ini_parse(c->path, data_handler, NULL);
    if (ret < 0)
    {
        fprintf(stderr, "Error: Unable to parse configuration file (%d)", ret);
        exit(EXIT_FAILURE);
    }
    free(c->section_name);
    c->section_name = NULL;

    // Find matching section
    ret = ini_parse(c->path, section_pattern_search_handler, NULL);
    if (!c->section_name)
    {
        ret = ini_parse(c->path, section_name_search_handler, NULL);
        if (!c->section_name)
        {
            debug_printf("Unable to match user input to configuration section (%d)", ret);
            return;
        }
    }

    // Parse settings of found section (sub config)
    ret = ini_parse(c->path, data_handler, NULL);
    if (ret < 0)
    {
        fprintf(stderr, "Error: Unable to parse configuration file (%d)", ret);
        exit(EXIT_FAILURE);
    }

    atexit(&config_exit);
}

void config_exit(void)
{
    free(c->tty);
    free(c->flow);
    free(c->parity);
    free(c->log_filename);
    free(c->map);

    free(c->match);
    free(c->section_name);
    free(c->path);

    free(c);
}

void config_file_print(void)
{
    if (c->path != NULL)
    {
        tio_printf(" Path: %s", c->path);
        if (c->section_name != NULL)
        {
            tio_printf(" Active sub-configuration: %s", c->section_name);
        }
    }
}
