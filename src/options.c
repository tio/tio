/*
 * tio - a serial device I/O tool
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
#include "rs485.h"
#include "timestamp.h"
#include "alert.h"
#include "log.h"
#include "script.h"

enum opt_t
{
    OPT_NONE,
    OPT_TIMESTAMP_FORMAT,
    OPT_TIMESTAMP_TIMEOUT,
    OPT_LOG_FILE,
    OPT_LOG_DIRECTORY,
    OPT_LOG_STRIP,
    OPT_LOG_APPEND,
    OPT_LINE_PULSE_DURATION,
    OPT_RS485,
    OPT_RS485_CONFIG,
    OPT_ALERT,
    OPT_COMPLETE_SUB_CONFIGS,
    OPT_MUTE,
    OPT_SCRIPT,
    OPT_SCRIPT_FILE,
    OPT_SCRIPT_RUN,
    OPT_INPUT_MODE,
    OPT_OUTPUT_MODE,
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
    .output_line_delay = 0,
    .dtr_pulse_duration = 100,
    .rts_pulse_duration = 100,
    .cts_pulse_duration = 100,
    .dsr_pulse_duration = 100,
    .dcd_pulse_duration = 100,
    .ri_pulse_duration = 100,
    .no_autoconnect = false,
    .log = false,
    .log_append = false,
    .log_filename = NULL,
    .log_directory = NULL,
    .log_strip = false,
    .local_echo = false,
    .timestamp = TIMESTAMP_NONE,
    .socket = NULL,
    .map = "",
    .color = 256, // Bold
    .input_mode = INPUT_MODE_NORMAL,
    .output_mode = OUTPUT_MODE_NORMAL,
    .prefix_code = 20, // ctrl-t
    .prefix_key = 't',
    .prefix_enabled = true,
    .mute = false,
    .rs485 = false,
    .rs485_config_flags = 0,
    .rs485_delay_rts_before_send = -1,
    .rs485_delay_rts_after_send = -1,
    .alert = ALERT_NONE,
    .complete_sub_configs = false,
    .script = NULL,
    .script_filename = NULL,
    .script_run = SCRIPT_RUN_ALWAYS,
    .timestamp_timeout = 200,
};

void print_help(char *argv[])
{
    UNUSED(argv);

    printf("Usage: tio [<options>] <tty-device|sub-config>\n");
    printf("\n");
    printf("Connect to TTY device directly or via sub-configuration.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -b, --baudrate <bps>                   Baud rate (default: 115200)\n");
    printf("  -d, --databits 5|6|7|8                 Data bits (default: 8)\n");
    printf("  -f, --flow hard|soft|none              Flow control (default: none)\n");
    printf("  -s, --stopbits 1|2                     Stop bits (default: 1)\n");
    printf("  -p, --parity odd|even|none|mark|space  Parity (default: none)\n");
    printf("  -o, --output-delay <ms>                Output character delay (default: 0)\n");
    printf("  -O, --output-line-delay <ms>           Output line delay (default: 0)\n");
    printf("      --line-pulse-duration <duration>   Set line pulse duration\n");
    printf("  -n, --no-autoconnect                   Disable automatic connect\n");
    printf("  -e, --local-echo                       Enable local echo\n");
    printf("      --input-mode normal|hex|line       Select input mode (default: normal)\n");
    printf("      --output-mode normal|hex           Select output mode (default: normal)\n");
    printf("  -t, --timestamp                        Enable line timestamp\n");
    printf("      --timestamp-format <format>        Set timestamp format (default: 24hour)\n");
    printf("      --timestamp-timeout <ms>           Set timestamp timeout (default: 200)\n");
    printf("  -L, --list-devices                     List available serial devices by ID\n");
    printf("  -l, --log                              Enable log to file\n");
    printf("      --log-file <filename>              Set log filename\n");
    printf("      --log-directory <path>             Set log directory path for automatic named logs\n");
    printf("      --log-append                       Append to log file\n");
    printf("      --log-strip                        Strip control characters and escape sequences\n");
    printf("  -m, --map <flags>                      Map characters\n");
    printf("  -c, --color 0..255|bold|none|list      Colorize tio text (default: bold)\n");
    printf("  -S, --socket <socket>                  Redirect I/O to socket\n");
    printf("      --rs-485                           Enable RS-485 mode\n");
    printf("      --rs-485-config <config>           Set RS-485 configuration\n");
    printf("      --alert bell|blink|none            Alert on connect/disconnect (default: none)\n");
    printf("      --mute                             Mute tio\n");
    printf("      --script <string>                  Run script from string\n");
    printf("      --script-file <filename>           Run script from file\n");
    printf("      --script-run once|always|never     Run script on connect (default: always)\n");
    printf("  -v, --version                          Display version\n");
    printf("  -h, --help                             Display help\n");
    printf("\n");
    printf("Options and sub-configurations may be set via configuration file.\n");
    printf("\n");
    printf("See the man page for more details.\n");
}

void line_pulse_duration_option_parse(const char *arg)
{
    bool token_found = true;
    char *token = NULL;
    char *buffer = strdup(arg);

    while (token_found == true)
    {
        if (token == NULL)
        {
            token = strtok(buffer,",");
        }
        else
        {
            token = strtok(NULL, ",");
        }

        if (token != NULL)
        {
            char keyname[11];
            unsigned int value;

            if (sscanf(token, "%10[^=]=%d", keyname, &value) == 2)
            {
                if (!strcmp(keyname, "DTR"))
                {
                    option.dtr_pulse_duration = value;
                }
                else if (!strcmp(keyname, "RTS"))
                {
                    option.rts_pulse_duration = value;
                }
                else if (!strcmp(keyname, "CTS"))
                {
                    option.cts_pulse_duration = value;
                }
                else if (!strcmp(keyname, "DSR"))
                {
                    option.dsr_pulse_duration = value;
                }
                else if (!strcmp(keyname, "DCD"))
                {
                    option.dcd_pulse_duration = value;
                }
                else if (!strcmp(keyname, "RI"))
                {
                    option.ri_pulse_duration = value;
                }
            }
            else
            {
                token_found = false;
            }
        }
        else
        {
            token_found = false;
        }
    }
    free(buffer);
}

input_mode_t input_mode_option_parse(const char *arg)
{
    if (strcmp("normal", arg) == 0)
    {
        return INPUT_MODE_NORMAL;
    }
    else if (strcmp("hex", arg) == 0)
    {
        return INPUT_MODE_HEX;
    }
    else if (strcmp("line", arg) == 0)
    {
        return INPUT_MODE_LINE;
    }
    else
    {
        tio_error_printf("Invalid input mode option");
        exit(EXIT_FAILURE);
    }
}

output_mode_t output_mode_option_parse(const char *arg)
{
    if (strcmp("normal", arg) == 0)
    {
        return OUTPUT_MODE_NORMAL;
    }
    else if (strcmp("hex", arg) == 0)
    {
        return OUTPUT_MODE_HEX;
    }
    else
    {
        tio_error_printf("Invalid output mode option");
        exit(EXIT_FAILURE);
    }
}

const char *input_mode_by_string(input_mode_t mode)
{
    switch (mode)
    {
        case INPUT_MODE_NORMAL:
            return "normal";
        case INPUT_MODE_HEX:
            return "hex";
        case INPUT_MODE_LINE:
            return "line";
        case INPUT_MODE_END:
            break;
    }

    return NULL;
}

const char *output_mode_by_string(output_mode_t mode)
{
    switch (mode)
    {
        case OUTPUT_MODE_NORMAL:
            return "normal";
        case OUTPUT_MODE_HEX:
            return "hex";
        case OUTPUT_MODE_END:
            break;
    }

    return NULL;
}

enum script_run_t script_run_option_parse(const char *arg)
{
    if (strcmp("once", arg) == 0)
    {
        return SCRIPT_RUN_ONCE;
    }
    else if (strcmp("always", arg) == 0)
    {
        return SCRIPT_RUN_ALWAYS;
    }
    else if (strcmp("never", arg) == 0)
    {
        return SCRIPT_RUN_NEVER;
    }
    else
    {
        tio_error_printf("Invalid script run option");
        exit(EXIT_FAILURE);
    }
}

void options_print()
{
    tio_printf(" Device: %s", option.tty_device);
    tio_printf(" Baudrate: %u", option.baudrate);
    tio_printf(" Databits: %d", option.databits);
    tio_printf(" Flow: %s", option.flow);
    tio_printf(" Stopbits: %d", option.stopbits);
    tio_printf(" Parity: %s", option.parity);
    tio_printf(" Local echo: %s", option.local_echo ? "enabled" : "disabled");
    tio_printf(" Timestamp: %s", timestamp_state_to_string(option.timestamp));
    tio_printf(" Output delay: %d", option.output_delay);
    tio_printf(" Output line delay: %d", option.output_line_delay);
    tio_printf(" Auto connect: %s", option.no_autoconnect ? "disabled" : "enabled");
    tio_printf(" Pulse duration: DTR=%d RTS=%d CTS=%d DSR=%d DCD=%d RI=%d", option.dtr_pulse_duration,
                                                                            option.rts_pulse_duration,
                                                                            option.cts_pulse_duration,
                                                                            option.dsr_pulse_duration,
                                                                            option.dcd_pulse_duration,
                                                                            option.ri_pulse_duration);
    tio_printf(" Input mode: %s", input_mode_by_string(option.input_mode));
    tio_printf(" Output mode: %s", output_mode_by_string(option.output_mode));
    if (option.map[0] != 0)
        tio_printf(" Map flags: %s", option.map);
    if (option.log)
        tio_printf(" Log file: %s", log_get_filename());
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

    // Support no-color.org informal spec
    char *no_color = getenv("NO_COLOR");
    if(no_color != NULL && no_color[0] != '\0')
	    option.color = -1;

    while (1)
    {
        static struct option long_options[] =
        {
            {"baudrate",             required_argument, 0, 'b'                     },
            {"databits",             required_argument, 0, 'd'                     },
            {"flow",                 required_argument, 0, 'f'                     },
            {"stopbits",             required_argument, 0, 's'                     },
            {"parity",               required_argument, 0, 'p'                     },
            {"output-delay",         required_argument, 0, 'o'                     },
            {"output-line-delay" ,   required_argument, 0, 'O'                     },
            {"line-pulse-duration",  required_argument, 0, OPT_LINE_PULSE_DURATION },
            {"no-autoconnect",       no_argument,       0, 'n'                     },
            {"local-echo",           no_argument,       0, 'e'                     },
            {"timestamp",            no_argument,       0, 't'                     },
            {"timestamp-format",     required_argument, 0, OPT_TIMESTAMP_FORMAT    },
            {"timestamp-timeout",    required_argument, 0, OPT_TIMESTAMP_TIMEOUT   },
            {"list-devices",         no_argument,       0, 'L'                     },
            {"log",                  no_argument,       0, 'l'                     },
            {"log-file",             required_argument, 0, OPT_LOG_FILE            },
            {"log-directory",        required_argument, 0, OPT_LOG_DIRECTORY       },
            {"log-append",           no_argument,       0, OPT_LOG_APPEND          },
            {"log-strip",            no_argument,       0, OPT_LOG_STRIP           },
            {"socket",               required_argument, 0, 'S'                     },
            {"map",                  required_argument, 0, 'm'                     },
            {"color",                required_argument, 0, 'c'                     },
            {"input-mode",           required_argument, 0, OPT_INPUT_MODE          },
            {"output-mode",          required_argument, 0, OPT_OUTPUT_MODE         },
            {"rs-485",               no_argument,       0, OPT_RS485               },
            {"rs-485-config",        required_argument, 0, OPT_RS485_CONFIG        },
            {"alert",                required_argument, 0, OPT_ALERT               },
            {"mute",                 no_argument,       0, OPT_MUTE                },
            {"script",               required_argument, 0, OPT_SCRIPT              },
            {"script-file",          required_argument, 0, OPT_SCRIPT_FILE         },
            {"script-run",           required_argument, 0, OPT_SCRIPT_RUN          },
            {"version",              no_argument,       0, 'v'                     },
            {"help",                 no_argument,       0, 'h'                     },
            {"complete-sub-configs", no_argument,       0, OPT_COMPLETE_SUB_CONFIGS},
            {0,                      0,                 0,  0                      }
        };

        /* getopt_long stores the option index here */
        int option_index = 0;

        /* Parse argument using getopt_long */
        c = getopt_long(argc, argv, "b:d:f:s:p:o:O:netLlS:m:c:xrvh", long_options, &option_index);

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

            case 'O':
                option.output_line_delay = string_to_long(optarg);
                break;

            case OPT_LINE_PULSE_DURATION:
                line_pulse_duration_option_parse(optarg);
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

            case OPT_TIMESTAMP_TIMEOUT:
                option.timestamp_timeout = string_to_long(optarg);
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

            case OPT_LOG_DIRECTORY:
                option.log_directory = optarg;
                break;

            case OPT_LOG_STRIP:
                option.log_strip = true;
                break;

            case OPT_LOG_APPEND:
                option.log_append = true;
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
                else if (!strcmp(optarg, "none"))
                {
                    option.color = -1; // No color
                    break;
                }
                else if (!strcmp(optarg, "bold"))
                {
                    option.color = 256; // Bold
                    break;
                }

                option.color = string_to_long(optarg);
                if ((option.color < 0) || (option.color > 255))
                {
                    tio_error_printf("Invalid color code");
                    exit(EXIT_FAILURE);
                }
                break;

            case OPT_INPUT_MODE:
                option.input_mode = input_mode_option_parse(optarg);
                break;

            case OPT_OUTPUT_MODE:
                option.output_mode = output_mode_option_parse(optarg);
                break;

            case OPT_RS485:
                option.rs485 = true;
                break;

            case OPT_RS485_CONFIG:
                rs485_parse_config(optarg);
                break;

            case OPT_ALERT:
                option.alert = alert_option_parse(optarg);
                break;

            case OPT_MUTE:
                option.mute = true;
                break;

            case OPT_SCRIPT:
                option.script = optarg;
                break;

            case OPT_SCRIPT_FILE:
                option.script_filename = optarg;
                break;

            case OPT_SCRIPT_RUN:
                option.script_run = script_run_option_parse(optarg);
                break;

            case 'v':
                printf("tio v%s\n", VERSION);
                exit(EXIT_SUCCESS);
                break;

            case 'h':
                print_help(argv);
                exit(EXIT_SUCCESS);
                break;

            case OPT_COMPLETE_SUB_CONFIGS:
                option.complete_sub_configs = true;
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

    if (option.complete_sub_configs)
    {
        return;
    }

    if (strlen(option.tty_device) == 0)
    {
        tio_error_printf("Missing tty device or sub-configuration name");
        exit(EXIT_FAILURE);
    }

    /* Print any remaining command line arguments (unknown options) */
    if (optind < argc)
    {
        fprintf(stderr, "Error: Unknown argument ");
        while (optind < argc)
        {
            fprintf(stderr, "%s ", argv[optind++]);
        }
        fprintf(stderr, "\n");
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

#ifdef __CYGWIN__
    unsigned char portnum;
    char *tty_win;
    if ( ((strncmp("COM", tty_device, 3) == 0)
        || (strncmp("com", tty_device, 3) == 0) )
        && (sscanf(tty_device + 3, "%hhu", &portnum) == 1)
        && (portnum > 0) ) 
    {
        asprintf(&tty_win, "/dev/ttyS%hhu", portnum - 1);
        tty_device = tty_win;
    }
#endif

    /* Restore tty device */
    option.tty_device = tty_device;
}
