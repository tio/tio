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
#include "rs485.h"
#include "timestamp.h"
#include "alert.h"

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
    char *socket;
    char *map;
};

static struct config_t c;

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
        tio_error_printf("Regex failure: %s", err);
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

static bool read_boolean(const char *value, const char *name)
{
    const char *true_values[] = { "true", "enable", "on", "yes", "1", NULL };
    const char *false_values[] = { "false", "disable", "off", "no", "0", NULL };

    for (int i = 0; true_values[i] != NULL; i++)
        if (strcmp(value, true_values[i]) == 0)
            return true;

    for (int i = 0; false_values[i] != NULL; i++)
        if (strcmp(value, false_values[i]) == 0)
            return false;

    tio_error_printf("Invalid value '%s' for option '%s' in configuration file",
            value, name);
    exit(EXIT_FAILURE);
}

static long read_integer(const char *value, const char *name, long min_value, long max_value)
{
    errno = 0;
    char *endptr;
    long result = strtol(value, &endptr, 10);

    if (errno || endptr == value || *endptr != '\0' || result < min_value || result > max_value)
    {
        tio_error_printf("Invalid value '%s' for option '%s' in configuration file",
                value, name);
        exit(EXIT_FAILURE);
    }

    return result;
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
    if (!strcmp(section, c.section_name))
    {
        // Set configuration parameter if found
        if (!strcmp(name, "device") || !strcmp(name, "tty"))
        {
            asprintf(&c.tty, value, c.match);
            option.tty_device = c.tty;
        }
        else if (!strcmp(name, "baudrate"))
        {
            option.baudrate = read_integer(value, name, 0, LONG_MAX);
        }
        else if (!strcmp(name, "databits"))
        {
            option.databits = read_integer(value, name, 5, 8);
        }
        else if (!strcmp(name, "flow"))
        {
            asprintf(&c.flow, "%s", value);
            option.flow = c.flow;
        }
        else if (!strcmp(name, "stopbits"))
        {
            option.stopbits = read_integer(value, name, 1, 2);
        }
        else if (!strcmp(name, "parity"))
        {
            asprintf(&c.parity, "%s", value);
            option.parity = c.parity;
        }
        else if (!strcmp(name, "output-delay"))
        {
            option.output_delay = read_integer(value, name, 0, LONG_MAX);
        }
        else if (!strcmp(name, "output-line-delay"))
        {
            option.output_line_delay = read_integer(value, name, 0, LONG_MAX);
        }
        else if (!strcmp(name, "line-pulse-duration"))
        {
            line_pulse_duration_option_parse(value);
        }
        else if (!strcmp(name, "no-autoconnect"))
        {
            option.no_autoconnect = read_boolean(value, name);
        }
        else if (!strcmp(name, "log"))
        {
            option.log = read_boolean(value, name);
        }
        else if (!strcmp(name, "log-file"))
        {
            asprintf(&c.log_filename, "%s", value);
            option.log_filename = c.log_filename;
        }
        else if (!strcmp(name, "log-append"))
        {
            option.log_append = read_boolean(value, name);
        }
        else if (!strcmp(name, "log-strip"))
        {
            option.log_strip = read_boolean(value, name);
        }
        else if (!strcmp(name, "local-echo"))
        {
            option.local_echo = read_boolean(value, name);
        }
        else if (!strcmp(name, "hexadecimal"))
        {
            option.hex_mode = read_boolean(value, name);
        }
        else if (!strcmp(name, "timestamp"))
        {
            option.timestamp = read_boolean(value, name) ?
                TIMESTAMP_24HOUR : TIMESTAMP_NONE;
        }
        else if (!strcmp(name, "timestamp-format"))
        {
            option.timestamp = timestamp_option_parse(value);
        }
        else if (!strcmp(name, "map"))
        {
            asprintf(&c.map, "%s", value);
            option.map = c.map;
        }
        else if (!strcmp(name, "color"))
        {
            if (!strcmp(value, "list"))
            {
                // Ignore
                return 0;
            }
            else if (!strcmp(value, "none"))
            {
                option.color = -1; // No color
                return 0;
            }
            else if (!strcmp(value, "bold"))
            {
                option.color = 256; // Bold
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
            asprintf(&c.socket, "%s", value);
            option.socket = c.socket;
        }
        else if (!strcmp(name, "prefix-ctrl-key"))
        {
            if (!strcmp(value, "none"))
            {
                option.prefix_enabled = false;
            }
            else if (ctrl_key_code(value[0]) > 0)
            {
                option.prefix_code = ctrl_key_code(value[0]);
                option.prefix_key = value[0];
            }
        }
        else if (!strcmp(name, "response-wait"))
        {
            option.response_wait = read_boolean(value, name);
        }
        else if (!strcmp(name, "response-timeout"))
        {
            option.response_timeout = read_integer(value, name, 0, LONG_MAX);
        }
        else if (!strcmp(name, "rs-485"))
        {
            option.rs485 = read_boolean(value, name);
        }
        else if (!strcmp(name, "rs-485-config"))
        {
            rs485_parse_config(value);
        }
        else if (!strcmp(name, "alert"))
        {
            option.alert = alert_option_parse(value);
        }
        else if (!strcmp(name, "mute"))
        {
            option.mute = read_boolean(value, name);
        }
        else if (!strcmp(name, "pattern"))
        {
            // Do nothing
        }
        else
        {
            tio_warning_printf("Unknown option '%s' in configuration file, ignored", name);
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

    if (!strcmp(varval, c.user))
    {
        /* pattern matches as plain text */
        asprintf(&c.section_name, "%s", section);
    }
    else if (get_match(c.user, varval, &c.match) > 0)
    {
        /* pattern matches as regex */
        asprintf(&c.section_name, "%s", section);
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

    if (!strcmp(section, c.user))
    {
        /* section name matches as plain text */
        asprintf(&c.section_name, "%s", section);
    }

    return 0;
}

static int section_name_print_handler(void *user, const char *section, const char *varname,
                                       const char *varval)
{
    UNUSED(user);
    UNUSED(varname);
    UNUSED(varval);

    static char *section_previous = "";

    if (strcmp(section, section_previous) != 0)
    {
        printf("%s ", section);
        section_previous = strdup(section);
    }

    return 0;
}

static int resolve_config_file(void)
{
    char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg)
    {
        asprintf(&c.path, "%s/tio/config", xdg);
        if (access(c.path, F_OK) == 0)
        {
            return 0;
        }
        free(c.path);
    }

    char *home = getenv("HOME");
    if (home)
    {
        asprintf(&c.path, "%s/.config/tio/config", home);
        if (access(c.path, F_OK) == 0)
        {
            return 0;
        }
        free(c.path);

        asprintf(&c.path, "%s/.tioconfig", home);
        if (access(c.path, F_OK) == 0)
        {
            return 0;
        }
        free(c.path);
    }

    c.path = NULL;
    return -EINVAL;
}

void config_file_show_sub_configurations(void)
{
    memset(&c, 0, sizeof(struct config_t));

    // Find config file
    if (resolve_config_file() != 0)
    {
        // None found - stop parsing
        return;
    }

    ini_parse(c.path, section_name_print_handler, NULL);
}

void config_file_parse(void)
{
    int ret;

    memset(&c, 0, sizeof(struct config_t));

    // Find config file
    if (resolve_config_file() != 0)
    {
        // None found - stop parsing
        return;
    }

    // Set user input which may be tty device or sub config
    c.user = option.tty_device;

    if (!c.user)
    {
        return;
    }

    // Parse default (unnamed) settings
    asprintf(&c.section_name, "%s", "");
    ret = ini_parse(c.path, data_handler, NULL);
    if (ret < 0)
    {
        tio_error_printf("Unable to parse configuration file (%d)", ret);
        exit(EXIT_FAILURE);
    }
    free(c.section_name);
    c.section_name = NULL;

    // Find matching section
    ret = ini_parse(c.path, section_pattern_search_handler, NULL);
    if (!c.section_name)
    {
        ret = ini_parse(c.path, section_name_search_handler, NULL);
        if (!c.section_name)
        {
            tio_debug_printf("Unable to match user input to configuration section (%d)", ret);
            return;
        }
    }

    // Parse settings of found section (sub config)
    ret = ini_parse(c.path, data_handler, NULL);
    if (ret < 0)
    {
        tio_error_printf("Unable to parse configuration file (%d)", ret);
        exit(EXIT_FAILURE);
    }

    atexit(&config_exit);
}

void config_exit(void)
{
    free(c.tty);
    free(c.flow);
    free(c.parity);
    free(c.log_filename);
    free(c.map);

    free(c.match);
    free(c.section_name);
    free(c.path);
}

void config_file_print(void)
{
    if (c.path != NULL)
    {
        tio_printf(" Active configuration file: %s", c.path);
        if (c.section_name != NULL)
        {
            tio_printf(" Active sub-configuration: %s", c.section_name);
        }
    }
}
