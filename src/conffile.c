// vim: set expandtab tabstop=4 softtabstop=4 shiftwidth=4:
/*
 * tio - a simple TTY terminal I/O application
 *
 * Copyright (c) 2020-2022  Liam Beguin
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
#include "conffile.h"
#include "misc.h"
#include "options.h"
#include "error.h"
#include "print.h"

static struct conf_data *d;

static int get_match(const char *input, const char *pattern, char **match)
{
    int ret;
    int len = 0;
    regex_t re;
    regmatch_t m[2];
    char err[128];

    /* compile a regex with the pattern */
    ret = regcomp(&re, pattern, REG_EXTENDED);
    if (ret) {
        regerror(ret, &re, err, sizeof(err));
        printf("reg error: %s\n", err);
        return ret;
    }

    /* try to match on input */
    ret = regexec(&re, input, 2, m, 0);
    if (!ret)
        len = m[1].rm_eo - m[1].rm_so;

    regfree(&re);

    if (len)
        asprintf(match, "%s", &input[m[1].rm_so]);

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
    _unused(user);

    if (strcmp(section, d->section_name))
        return 0;

    if (!strcmp(name, "tty")) {
        asprintf(&d->tty, value, d->match);
        option.tty_device = d->tty;
    } else if (!strcmp(name, "baudrate")) {
        option.baudrate = string_to_long((char *)value);
    } else if (!strcmp(name, "databits")) {
        option.databits = atoi(value);
    } else if (!strcmp(name, "flow")) {
        asprintf(&d->flow, "%s", value);
        option.flow = d->flow;
    } else if (!strcmp(name, "stopbits")) {
        option.stopbits = atoi(value);
    } else if (!strcmp(name, "parity")) {
        asprintf(&d->parity, "%s", value);
        option.parity = d->parity;
    } else if (!strcmp(name, "output-delay")) {
        option.output_delay = atoi(value);
    } else if (!strcmp(name, "no-autoconnect")) {
        option.no_autoconnect = atoi(value);
    } else if (!strcmp(name, "log")) {
        option.log = atoi(value);
    } else if (!strcmp(name, "local-echo")) {
        option.local_echo = atoi(value);
    } else if (!strcmp(name, "timestamp")) {
        option.timestamp = atoi(value);
    } else if (!strcmp(name, "log-filename")) {
        asprintf(&d->log_filename, "%s", value);
        option.log_filename = d->log_filename;
    } else if (!strcmp(name, "map")) {
        asprintf(&d->map, "%s", value);
        option.map = d->map;
    }

    return 0;
}

/**
 * section_search_handler() - walk config file to find section matching user input
 *
 * INIH handler used to resolve the section matching the user's input.
 * This will look for the pattern element of each section and try to match it
 * with the user input.
 */
static int section_search_handler(void *user, const char *section, const char
                                  *varname, const char *varval)
{
    _unused(user);

    if (strcmp(varname, "pattern"))
        return 0;

    if (!strcmp(varval, d->user)) {
        /* pattern matches as plain text */
        asprintf(&d->section_name, "%s", section);
    } else if (get_match(d->user, varval, &d->match) > 0) {
        /* pattern matches as regex */
        asprintf(&d->section_name, "%s", section);
    }

    return 0;
}

int resolve_conf_file(void)
{
    asprintf(&d->path, "%s/tio/tiorc", getenv("XDG_CONFIG_HOME"));
    if (!access(d->path, F_OK))
        return 0;

    asprintf(&d->path, "%s/.config/tio/tiorc", getenv("HOME"));
    if (!access(d->path, F_OK))
        return 0;

    asprintf(&d->path, "%s/.tiorc", getenv("HOME"));
    if (!access(d->path, F_OK))
        return 0;

    return -EINVAL;
}

void conf_parse_file(const int argc, char *argv[])
{
    int ret;
    int i;

    d = malloc(sizeof(struct conf_data));
    memset(d, 0, sizeof(struct conf_data));

    resolve_conf_file();

    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            d->user = argv[i];
            break;
        }
    }

    if (!d->user)
            return;

    ret = ini_parse(d->path, section_search_handler, NULL);
    if (!d->section_name) {
        debug_printf("unable to match user input to configuration section (%d)\n", ret);
        return;
    }

    ret = ini_parse(d->path, data_handler, NULL);
    if (ret < 0) {
        fprintf(stderr, "Error: unable to parse configuration file (%d)\n", ret);
        exit(EXIT_FAILURE);
    }
}

void conf_exit(void)
{
    free(d->tty);
    free(d->flow);
    free(d->parity);
    free(d->log_filename);
    free(d->map);

    free(d->match);
    free(d->section_name);
    free(d->path);

    free(d);
}
