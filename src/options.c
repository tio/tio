/*
 * tio - a simple TTY terminal I/O tool
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
#include "options.h"
#include "error.h"

/* Default options */
struct option_t option =
{
    .tty_device = "",
    .baudrate = 115200,
    .databits = 8,
    .flow = "none",
    .stopbits = 1,
    .parity = "none",
    .output_delay = 0,
    .no_autoconnect = false,
    .log = false,
    .local_echo = false,
    .timestamp = TIMESTAMP_NONE,
    .list_devices = false,
    .log_filename = "",
    .map = "",
    .color = -1,
};

void print_help(char *argv[])
{
    printf("Usage: %s [<options>] <tty-device>\n", argv[0]);
    printf("\n");
    printf("Options:\n");
    printf("  -b, --baudrate <bps>        Baud rate (default: 115200)\n");
    printf("  -d, --databits 5|6|7|8      Data bits (default: 8)\n");
    printf("  -f, --flow hard|soft|none   Flow control (default: none)\n");
    printf("  -s, --stopbits 1|2          Stop bits (default: 1)\n");
    printf("  -p, --parity odd|even|none  Parity (default: none)\n");
    printf("  -o, --output-delay <ms>     Output delay (default: 0)\n");
    printf("  -n, --no-autoconnect        Disable automatic connect\n");
    printf("  -e, --local-echo            Enable local echo\n");
    printf("  -t, --timestamp[=<format>]  Enable timestamp (default: 24hour)\n");
    printf("  -L, --list-devices          List available serial devices\n");
    printf("  -l, --log[=<filename>]      Log to file\n");
    printf("  -m, --map <flags>           Map special characters\n");
    printf("  -c, --color <code>          Colorize tio text\n");
    printf("  -v, --version               Display version\n");
    printf("  -h, --help                  Display help\n");
    printf("\n");
    printf("See the man page for more details.\n");
    printf("\n");
    printf("In session, press ctrl-t q to quit.\n");
    printf("\n");
}

long string_to_long(char *string)
{
    long result;
    char *end_token;

    errno = 0;
    result = strtol(string, &end_token, 10);
    if ((errno != 0) || (*end_token != 0))
    {
        printf("Error: Invalid digit\n");
        exit(EXIT_FAILURE);
    }

    return result;
}

void parse_options(int argc, char *argv[])
{
    int c;

    if (argc == 1)
    {
        print_help(argv);
        exit(EXIT_SUCCESS);
    }

    while (1)
    {
        static struct option long_options[] =
        {
            {"baudrate",       required_argument, 0, 'b'},
            {"databits",       required_argument, 0, 'd'},
            {"flow",           required_argument, 0, 'f'},
            {"stopbits",       required_argument, 0, 's'},
            {"parity",         required_argument, 0, 'p'},
            {"output-delay",   required_argument, 0, 'o'},
            {"no-autoconnect", no_argument,       0, 'n'},
            {"local-echo",     no_argument,       0, 'e'},
            {"timestamp",      optional_argument, 0, 't'},
            {"list-devices",   no_argument,       0, 'L'},
            {"log",            optional_argument, 0, 'l'},
            {"map",            required_argument, 0, 'm'},
            {"color",          required_argument, 0, 'c'},
            {"version",        no_argument,       0, 'v'},
            {"help",           no_argument,       0, 'h'},
            {0,                0,                 0,  0 }
        };

        /* getopt_long stores the option index here */
        int option_index = 0;

        /* Parse argument using getopt_long */
        c = getopt_long(argc, argv, "b:d:f:s:p:o:net::Ll::m:c:vh", long_options, &option_index);

        /* Detect the end of the options */
        if (c == -1)
            break;

        switch (c)
        {
            case 0:
                /* If this option sets a flag, do nothing else now */
                if (long_options[option_index].flag != 0)
                    break;
                printf("option %s", long_options[option_index].name);
                if (optarg)
                    printf(" with arg %s", optarg);
                printf("\n");
                break;

            case 'b':
                option.baudrate = string_to_long(optarg);
                break;

            case 'd':
                option.databits = string_to_long(optarg);
                break;

            case 'f':
                option.flow = optarg;
                break;

            case 's':
                option.stopbits = string_to_long(optarg);
                break;

            case 'p':
                option.parity = optarg;
                break;

            case 'o':
                option.output_delay = string_to_long(optarg);
                break;

            case 'n':
                option.no_autoconnect = true;
                break;

            case 'e':
                option.local_echo = true;
                break;

            case 't':
                option.timestamp = TIMESTAMP_24HOUR; // Default
                if (optarg != NULL)
                {
                    if (strcmp(optarg, "24hour") == 0)
                    {
                        option.timestamp = TIMESTAMP_24HOUR;
                    }
                    else if (strcmp(optarg, "24hour-start") == 0)
                    {
                        option.timestamp = TIMESTAMP_24HOUR_START;
                    }
                    else if (strcmp(optarg, "iso8601") == 0)
                    {
                        option.timestamp = TIMESTAMP_ISO8601;
                    }
                    else
                    {
                        printf("Error: Unknown timestamp type\n");
                        exit(EXIT_FAILURE);
                    }
                }
                break;

            case 'L':
                option.list_devices = true;
                break;

            case 'l':
                option.log = true;
                option.log_filename = optarg;
                break;

            case 'm':
                option.map = optarg;
                break;

            case 'c':
                option.color = string_to_long(optarg);
                if (option.color > 255)
                {
                    printf("Error: Invalid color code\n");
                    exit(EXIT_FAILURE);
                }
                if (option.color < 0)
                {
                    // Print available color codes
                    printf("Available color codes:\n");
                    for (int i=0; i<=255; i++)
                    {
                        printf(" \e[1;38;5;%dmThis is color code %d\e[0m\n", i, i);
                    }
                    exit(EXIT_SUCCESS);
                }
                break;

            case 'v':
                printf("tio v%s\n", VERSION);
                printf("Copyright (c) 2014-2022 Martin Lund\n");
                printf("\n");
                printf("License GPLv2+: GNU GPL version 2 or later <http://gnu.org/licenses/gpl-2.0.html>.\n");
                printf("This is free software: you are free to change and redistribute it.\n");
                printf("There is NO WARRANTY, to the extent permitted by law.\n");
                exit(EXIT_SUCCESS);
                break;

            case 'h':
                print_help(argv);
                exit(EXIT_SUCCESS);
                break;

            case '?':
                /* getopt_long already printed an error message */
                exit(EXIT_FAILURE);
                break;

            default:
                exit(EXIT_FAILURE);
        }
    }

    if (option.list_devices)
    {
        return;
    }

    /* Assume first non-option is the tty device name */
    if (strcmp(option.tty_device, ""))
	    optind++;
    else if (optind < argc)
        option.tty_device = argv[optind++];

    if (strlen(option.tty_device) == 0)
    {
        printf("Error: Missing device name\n");
        exit(EXIT_FAILURE);
    }

    /* Print any remaining command line arguments (unknown options) */
    if (optind < argc)
    {
        printf("Error: unknown arguments: ");
        while (optind < argc)
            printf("%s ", argv[optind++]);
        printf("\n");
        exit(EXIT_FAILURE);
    }
}
