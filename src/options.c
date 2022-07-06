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
#include "misc.h"
#include "print.h"
#include "tty.h"

enum opt_t
{
    OPT_NONE,
    OPT_TIMESTAMP_FORMAT,
    OPT_LOG_FILE,
    OPT_LOG_STRIP,
};

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
    .log_filename = NULL,
    .log_strip = false,
    .local_echo = false,
    .timestamp = TIMESTAMP_NONE,
    .socket = NULL,
    .map = "",
    .color = 15,
    .hex_mode = false,
};

void print_help(char *argv[])
{
    printf("Usage: %s [<options>] <tty-device|sub-config>\n", argv[0]);
    printf("\n");
    printf("Connect to tty-device directly or via sub-configuration.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -b, --baudrate <bps>             Baud rate (default: 115200)\n");
    printf("  -d, --databits 5|6|7|8           Data bits (default: 8)\n");
    printf("  -f, --flow hard|soft|none        Flow control (default: none)\n");
    printf("  -s, --stopbits 1|2               Stop bits (default: 1)\n");
    printf("  -p, --parity odd|even|none       Parity (default: none)\n");
    printf("  -o, --output-delay <ms>          Output delay (default: 0)\n");
    printf("  -n, --no-autoconnect             Disable automatic connect\n");
    printf("  -e, --local-echo                 Enable local echo\n");
    printf("  -t, --timestamp                  Enable line timestamp\n");
    printf("      --timestamp-format <format>  Set timestamp format (default: 24hour)\n");
    printf("  -L, --list-devices               List available serial devices\n");
    printf("  -l, --log                        Enable log to file\n");
    printf("      --log-file <filename>        Set log filename\n");
    printf("      --log-strip                  Strip control characters and escape sequences\n");
    printf("  -m, --map <flags>                Map special characters\n");
    printf("  -c, --color 0..255|none|list     Colorize tio text (default: 15)\n");
    printf("  -S, --socket <socket>            Redirect I/O to socket\n");
    printf("  -x, --hexadecimal                Enable hexadecimal mode\n");
    printf("  -v, --version                    Display version\n");
    printf("  -h, --help                       Display help\n");
    printf("\n");
    printf("Options and sub-configurations may be set via configuration file.\n");
    printf("\n");
    printf("In session, press ctrl-t q to quit.\n");
    printf("\n");
    printf("See the man page for more details.\n");
}

const char* timestamp_state_to_string(enum timestamp_t timestamp)
{
    switch (timestamp)
    {
        case TIMESTAMP_NONE:
            return "disabled";
            break;

        case TIMESTAMP_24HOUR:
            return "24hour";
            break;

        case TIMESTAMP_24HOUR_START:
            return "24hour-start";
            break;

        case TIMESTAMP_24HOUR_DELTA:
            return "24hour-delta";
            break;

        case TIMESTAMP_ISO8601:
            return "iso8601";
            break;

        default:
            return "unknown";
            break;
    }
}

enum timestamp_t timestamp_option_parse(const char *arg)
{
    enum timestamp_t timestamp = TIMESTAMP_24HOUR; // Default
    
    if (arg != NULL)
    {
        if (strcmp(arg, "24hour") == 0)
        {
            return TIMESTAMP_24HOUR;
        }
        else if (strcmp(arg, "24hour-start") == 0)
        {
            return TIMESTAMP_24HOUR_START;
        }
        else if (strcmp(arg, "24hour-delta") == 0)
        {
            return TIMESTAMP_24HOUR_DELTA;
        }
        else if (strcmp(arg, "iso8601") == 0)
        {
            return TIMESTAMP_ISO8601;
        }
    }

    return timestamp;
}

void options_print()
{
    tio_printf(" TTY device: %s", option.tty_device);
    tio_printf(" Baudrate: %u", option.baudrate);
    tio_printf(" Databits: %d", option.databits);
    tio_printf(" Flow: %s", option.flow);
    tio_printf(" Stopbits: %d", option.stopbits);
    tio_printf(" Parity: %s", option.parity);
    tio_printf(" Local echo: %s", option.local_echo ? "enabled" : "disabled");
    tio_printf(" Timestamp: %s", timestamp_state_to_string(option.timestamp));
    tio_printf(" Output delay: %d", option.output_delay);
    tio_printf(" Auto connect: %s", option.no_autoconnect ? "disabled" : "enabled");
    if (option.map[0] != 0)
        tio_printf(" Map flags: %s", option.map);
    if (option.log)
        tio_printf(" Log file: %s", option.log_filename);
    if (option.socket)
        tio_printf(" Socket: %s", option.socket);
}

void options_parse(int argc, char *argv[])
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
            {"baudrate",         required_argument, 0, 'b'                  },
            {"databits",         required_argument, 0, 'd'                  },
            {"flow",             required_argument, 0, 'f'                  },
            {"stopbits",         required_argument, 0, 's'                  },
            {"parity",           required_argument, 0, 'p'                  },
            {"output-delay",     required_argument, 0, 'o'                  },
            {"no-autoconnect",   no_argument,       0, 'n'                  },
            {"local-echo",       no_argument,       0, 'e'                  },
            {"timestamp",        no_argument,       0, 't'                  },
            {"timestamp-format", required_argument, 0, OPT_TIMESTAMP_FORMAT },
            {"list-devices",     no_argument,       0, 'L'                  },
            {"log",              no_argument,       0, 'l'                  },
            {"log-file",         required_argument, 0, OPT_LOG_FILE         },
            {"log-strip",        no_argument,       0, OPT_LOG_STRIP        },
            {"socket",           required_argument, 0, 'S'                  },
            {"map",              required_argument, 0, 'm'                  },
            {"color",            required_argument, 0, 'c'                  },
            {"hexadecimal",      no_argument,       0, 'x'                  },
            {"version",          no_argument,       0, 'v'                  },
            {"help",             no_argument,       0, 'h'                  },
            {0,                  0,                 0,  0                   }
        };

        /* getopt_long stores the option index here */
        int option_index = 0;

        /* Parse argument using getopt_long */
        c = getopt_long(argc, argv, "b:d:f:s:p:o:netLlS:m:c:xvh", long_options, &option_index);

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
                option.timestamp = TIMESTAMP_24HOUR;
                break;

            case OPT_TIMESTAMP_FORMAT:
                option.timestamp = timestamp_option_parse(optarg);
                break;

            case 'L':
                list_serial_devices();
                exit(EXIT_SUCCESS);
                break;

            case 'l':
                option.log = true;
                break;

            case OPT_LOG_FILE:
                option.log_filename = optarg;
                break;

            case OPT_LOG_STRIP:
                option.log_strip = true;
                break;

            case 'S':
                option.socket = optarg;
                break;

            case 'm':
                option.map = optarg;
                break;

            case 'c':
                if (!strcmp(optarg, "list"))
                {
                    // Print available color codes
                    printf("Available color codes:\n");
                    for (int i=0; i<=255; i++)
                    {
                        printf(" \e[1;38;5;%dmThis is color code %d\e[0m\n", i, i);
                    }
                    exit(EXIT_SUCCESS);
                }

                if (!strcmp(optarg, "none"))
                {
                    option.color = -1;
                    break;
                }

                option.color = string_to_long(optarg);
                if ((option.color < 0) || (option.color > 255))
                {
                    printf("Error: Invalid color code\n");
                    exit(EXIT_FAILURE);
                }
                break;

            case 'x':
                option.hex_mode = true;
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

    /* Assume first non-option is the tty device name */
    if (strcmp(option.tty_device, ""))
	    optind++;
    else if (optind < argc)
        option.tty_device = argv[optind++];

    if (strlen(option.tty_device) == 0)
    {
        printf("Error: Missing tty device or sub-configuration name\n");
        exit(EXIT_FAILURE);
    }

    /* Print any remaining command line arguments (unknown options) */
    if (optind < argc)
    {
        printf("Error: Unknown argument ");
        while (optind < argc)
            printf("%s ", argv[optind++]);
        printf("\n");
        exit(EXIT_FAILURE);
    }
}

void options_parse_final(int argc, char *argv[])
{
    /* Preserve tty device which may have been set by configuration file */
    const char *tty_device = option.tty_device;

    /* Do 2nd pass to override settings set by configuration file */
    optind = 1; // Reset option index to restart scanning of argv
    options_parse(argc, argv);

    /* Restore tty device */
    option.tty_device = tty_device;
}
