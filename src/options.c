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

#include <assert.h>
#include <regex.h>
#include <getopt.h>
#include <errno.h>
#include "config.h"
#include "misc.h"
#include "print.h"
#include "rs485.h"
#include "log.h"
#include "configfile.h"

#define HEX_N_VALUE_MAX 4096

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
    OPT_COMPLETE_PROFILES,
    OPT_MUTE,
    OPT_SCRIPT,
    OPT_SCRIPT_FILE,
    OPT_SCRIPT_RUN,
    OPT_INPUT_MODE,
    OPT_OUTPUT_MODE,
    OPT_EXCLUDE_DEVICES,
    OPT_EXCLUDE_DRIVERS,
    OPT_EXCLUDE_TIDS,
    OPT_EXEC,
};

/* Default options */
struct option_t option =
{
    .target = "",
    .baudrate = 115200,
    .databits = 8,
    .flow = FLOW_NONE,
    .stopbits = 1,
    .parity = PARITY_NONE,
    .output_delay = 0,
    .output_line_delay = 0,
    .dtr_pulse_duration = 100,
    .rts_pulse_duration = 100,
    .cts_pulse_duration = 100,
    .dsr_pulse_duration = 100,
    .dcd_pulse_duration = 100,
    .ri_pulse_duration = 100,
    .no_reconnect = false,
    .auto_connect = AUTO_CONNECT_DIRECT,
    .log = false,
    .log_append = false,
    .log_filename = NULL,
    .log_directory = NULL,
    .log_strip = false,
    .local_echo = false,
    .timestamp = TIMESTAMP_NONE,
    .socket = NULL,
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
    .complete_profiles = false,
    .script = NULL,
    .script_filename = NULL,
    .script_run = SCRIPT_RUN_ALWAYS,
    .timestamp_timeout = 200,
    .exclude_devices = NULL,
    .exclude_drivers = NULL,
    .exclude_tids = NULL,
    .hex_n_value = 0,
    .vt100 = false,
    .exec = NULL,
    .map_i_nl_cr = false,
    .map_i_cr_nl = false,
    .map_ign_cr = false,
    .map_i_ff_escc = false,
    .map_i_nl_crnl = false,
    .map_o_cr_nl = false,
    .map_o_nl_crnl = false,
    .map_o_del_bs = false,
    .map_o_ltu = false,
    .map_o_nulbrk = false,
    .map_i_msb2lsb = false,
    .map_o_ign_cr = false,
};

void option_print_help(char *argv[])
{
    UNUSED(argv);

    printf("Usage: tio [<options>] <tty-device|profile|tid>\n");
    printf("\n");
    printf("Connect to TTY device directly or via configuration profile or topology ID.\n");
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
    printf("  -a, --auto-connect new|latest|direct   Automatic connect strategy (default: direct)\n");
    printf("      --exclude-devices <pattern>        Exclude devices by pattern\n");
    printf("      --exclude-drivers <pattern>        Exclude drivers by pattern\n");
    printf("      --exclude-tids <pattern>           Exclude topology IDs by pattern\n");
    printf("  -n, --no-reconnect                     Do not reconnect\n");
    printf("  -e, --local-echo                       Enable local echo\n");
    printf("      --input-mode normal|hex|line       Select input mode (default: normal)\n");
    printf("      --output-mode normal|hex|hexN      Select output mode (default: normal)\n");
    printf("  -t, --timestamp                        Enable line timestamp\n");
    printf("      --timestamp-format <format>        Set timestamp format (default: 24hour)\n");
    printf("      --timestamp-timeout <ms>           Set timestamp timeout (default: 200)\n");
    printf("  -l, --list                             List available serial devices, TIDs, and profiles\n");
    printf("  -L, --log                              Enable log to file\n");
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
    printf("      --mute                             Mute tio messages\n");
    printf("      --script <string>                  Run script from string\n");
    printf("      --script-file <filename>           Run script from file\n");
    printf("      --script-run once|always|never     Run script on connect (default: always)\n");
    printf("      --exec <command>                   Execute shell command with I/O redirected to device\n");
    printf("  -v, --version                          Display version\n");
    printf("  -h, --help                             Display help\n");
    printf("\n");
    printf("Options and profiles may be set via configuration file.\n");
    printf("\n");
    printf("In session you can press ctrl-%c ? to list available key commands.\n", option.prefix_key);
    printf("\n");
    printf("See the man page for more details.\n");
}

int option_string_to_integer(const char *string, int *value, const char *desc, int min, int max)
{
    int val;
    char *end_token;

    errno = 0;
    val = strtol(string, &end_token, 10);
    if ((errno != 0) || (*end_token != 0))
    {
        if (desc != NULL)
        {
            tio_error_print("Invalid %s '%s'", desc, string);
            exit(EXIT_FAILURE);
        }
        else
        {
            tio_error_print("Invalid digit '%s'", string);
            exit(EXIT_FAILURE);
        }
        return -1;
    }
    else
    {
        if ((val < min) || (val > max))
        {
            if (desc != NULL)
            {
                tio_error_print("Invalid %s '%s'", desc, string);
                exit(EXIT_FAILURE);
            }
            else
            {
                tio_error_print("Invalid digit '%s'", string);
                exit(EXIT_FAILURE);
            }
            return -1;
        }
        else
        {
            *value = val;
        }
    }

    return 0;
}

void option_parse_flow(const char *arg, flow_t *flow)
{
    assert(arg != NULL);

    /* Parse flow control */
    if (strcmp("hard", arg) == 0)
    {
        *flow = FLOW_HARD;
    }
    else if (strcmp("soft", arg) == 0)
    {
        *flow = FLOW_SOFT;
    }
    else if (strcmp("none", arg) == 0)
    {
        *flow = FLOW_NONE;
    }
    else
    {
        tio_error_print("Invalid flow control '%s'", arg);
        exit(EXIT_FAILURE);
    }
}

const char *option_flow_to_string(flow_t flow)
{
    switch (flow)
    {
        case FLOW_NONE:
            return "none";
        case FLOW_HARD:
            return "hard";
        case FLOW_SOFT:
            return "soft";
        default:
            return "unknown";
    }
}

void option_parse_parity(const char *arg, parity_t *parity)
{
    assert(arg != NULL);

    if (strcmp("none", arg) == 0)
    {
        *parity = PARITY_NONE;
    }
    else if (strcmp("odd", arg) == 0)
    {
        *parity = PARITY_ODD;
    }
    else if (strcmp("even", arg) == 0)
    {
        *parity = PARITY_EVEN;
    }
    else if (strcmp("mark", arg) == 0)
    {
        *parity = PARITY_MARK;
    }
    else if (strcmp("space", arg) == 0)
    {
        *parity = PARITY_SPACE;
    }
    else
    {
        tio_error_print("Invalid parity '%s'", arg);
        exit(EXIT_FAILURE);
    }
}

const char *option_parity_to_string(parity_t parity)
{
    switch (parity)
    {
        case PARITY_NONE:
            return "none";
        case PARITY_ODD:
            return "odd";
        case PARITY_EVEN:
            return "even";
        case PARITY_MARK:
            return "mark";
        case PARITY_SPACE:
            return "space";
        default:
            return "unknown";
    }
}

void option_parse_color(const char *arg, int *color)
{
    int value;

    assert(arg != NULL);

    if (strcmp(optarg, "list") == 0)
    {
        // Print available color codes
        printf("Available color codes:\n");
        for (int i=0; i<=255; i++)
        {
            printf(" \e[1;38;5;%dmThis is color code %d\e[0m\n", i, i);
        }
        exit(EXIT_SUCCESS);
    }
    else if (strcmp(arg, "none") == 0)
    {
        *color = -1; // No color
    }
    else if (strcmp(arg, "bold") == 0)
    {
        *color = 256; // Bold
    }
    else
    {
        if (option_string_to_integer(arg, &value, "color code", 0, 255) == 0)
        {
            *color = value;
        }
    }
}

void option_parse_alert(const char *arg, alert_t *alert)
{
    assert(arg != NULL);

    if (strcmp(arg, "none") == 0)
    {
        *alert = ALERT_NONE;
    }
    else if (strcmp(arg, "bell") == 0)
    {
        *alert = ALERT_BELL;
    }
    else if (strcmp(arg, "blink") == 0)
    {
        *alert = ALERT_BLINK;
    }
    else
    {
        tio_error_print("Invalid alert '%s'", arg);
        exit(EXIT_FAILURE);
    }
}

const char* option_timestamp_format_to_string(timestamp_t timestamp)
{
    switch (timestamp)
    {
        case TIMESTAMP_NONE:
            return "none";
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

        case TIMESTAMP_EPOCH:
            return "epoch";
            break;

        default:
            return "unknown";
            break;
    }
}

void option_parse_timestamp(const char *arg, timestamp_t *timestamp)
{
    assert(arg != NULL);

    if (strcmp(arg, "24hour") == 0)
    {
        *timestamp = TIMESTAMP_24HOUR;
    }
    else if (strcmp(arg, "24hour-start") == 0)
    {
        *timestamp = TIMESTAMP_24HOUR_START;
    }
    else if (strcmp(arg, "24hour-delta") == 0)
    {
        *timestamp = TIMESTAMP_24HOUR_DELTA;
    }
    else if (strcmp(arg, "iso8601") == 0)
    {
        *timestamp = TIMESTAMP_ISO8601;
    }
    else if (strcmp(arg, "epoch") == 0)
    {
        *timestamp = TIMESTAMP_EPOCH;
    }
    else
    {
        tio_error_print("Invalid timestamp '%s'", arg);
        exit(EXIT_FAILURE);
    }
}

const char *option_alert_state_to_string(alert_t state)
{
    switch (state)
    {
        case ALERT_NONE:
            return "none";
        case ALERT_BELL:
            return "bell";
        case ALERT_BLINK:
            return "blink";
        default:
            return "unknown";
    }
}

const char *option_auto_connect_state_to_string(auto_connect_t strategy)
{
    switch (strategy)
    {
        case AUTO_CONNECT_DIRECT:
            return "direct";
        case AUTO_CONNECT_NEW:
            return "new";
        case AUTO_CONNECT_LATEST:
            return "latest";
        default:
            return "unknown";
    }
}

void option_parse_auto_connect(const char *arg, auto_connect_t *auto_connect)
{
    assert(arg != NULL);

    if (arg != NULL)
    {
        if (strcmp(arg, "direct") == 0)
        {
            *auto_connect = AUTO_CONNECT_DIRECT;
        }
        else if (strcmp(arg, "new") == 0)
        {
            *auto_connect = AUTO_CONNECT_NEW;
        }
        else if (strcmp(arg, "latest") == 0)
        {
            *auto_connect = AUTO_CONNECT_LATEST;
        }
        else
        {
            tio_error_print("Invalid auto-connect strategy '%s'", arg);
            exit(EXIT_FAILURE);
        }
    }
}

void option_parse_line_pulse_duration(const char *arg)
{
    bool token_found = true;
    char *token = NULL;
    char *buffer = strdup(arg);

    assert(arg != NULL);

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
                else
                {
                    tio_error_print("Invalid line '%s'", keyname);
                    exit(EXIT_FAILURE);
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

// Function to parse the input string
int option_parse_hexN_string(const char *input_string)
{
    regmatch_t match[2]; // One for entire match, one for the optional N
    int n_value = 0;
    regex_t regex;
    int ret;

    // Compile the regular expression to match "hex" and optionally capture N
    ret = regcomp(&regex, "^hex([0-9]+)?$", REG_EXTENDED);
    if (ret)
    {
        tio_error_print("Could not compile regex");
        exit(EXIT_FAILURE);
    }

    // Execute the regular expression
    ret = regexec(&regex, input_string, 2, match, 0);
    if (!ret)
    {
        // If there is a match, extract the N value if present
        if (match[1].rm_so != -1)
        {
            char n_value_str[32]; // Assume max 32 digits for the numerical value
            strncpy(n_value_str, input_string + match[1].rm_so, match[1].rm_eo - match[1].rm_so);
            n_value_str[match[1].rm_eo - match[1].rm_so] = '\0'; // Null-terminate the string
            n_value = atoi(n_value_str);

            if ((n_value > HEX_N_VALUE_MAX) || (n_value == 0))
            {
                n_value = -1;
            }
        }
        else
        {
            n_value = 0;
        }
    }
    else if (ret == REG_NOMATCH)
    {
        n_value = -1;
    }
    else
    {
        char msgbuf[100];
        regerror(ret, &regex, msgbuf, sizeof(msgbuf));
        tio_error_print("Regex match failed: %s", msgbuf);
        exit(EXIT_FAILURE);
    }

    regfree(&regex);

    return n_value;
}

void option_parse_input_mode(const char *arg, input_mode_t *mode)
{
    assert(arg != NULL);

    if (strcmp("normal", arg) == 0)
    {
        *mode = INPUT_MODE_NORMAL;
    }
    else if (strcmp("hex", arg) == 0)
    {
        *mode = INPUT_MODE_HEX;
    }
    else if (strcmp("line", arg) == 0)
    {
        *mode = INPUT_MODE_LINE;
    }
    else
    {
        tio_error_print("Invalid input mode '%s'", arg);
        exit(EXIT_FAILURE);
    }
}

void option_parse_output_mode(const char *arg, output_mode_t *mode)
{
    int n = 0;

    assert(arg != NULL);

    if (strcmp("normal", arg) == 0)
    {
        *mode = OUTPUT_MODE_NORMAL;
    }
    else if ((n = option_parse_hexN_string(arg)) != -1)
    {
        option.hex_n_value = n;
        *mode = OUTPUT_MODE_HEX;
    }
    else
    {
        tio_error_print("Invalid output mode '%s'", arg);
        exit(EXIT_FAILURE);
    }
}

const char *option_input_mode_to_string(input_mode_t mode)
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

const char *option_output_mode_to_string(output_mode_t mode)
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

void option_parse_script_run(const char *arg, script_run_t *script_run)
{
    assert(arg != NULL);

    if (strcmp("once", arg) == 0)
    {
        *script_run = SCRIPT_RUN_ONCE;
    }
    else if (strcmp("always", arg) == 0)
    {
        *script_run = SCRIPT_RUN_ALWAYS;
    }
    else if (strcmp("never", arg) == 0)
    {
        *script_run = SCRIPT_RUN_NEVER;
    }
    else
    {
        tio_error_print("Invalid script run option '%s'", arg);
        exit(EXIT_FAILURE);
    }
}

void option_parse_mappings(const char *map)
{
    bool token_found = true;
    char *token = NULL;
    char *buffer;

    if (map == NULL)
    {
        return;
    }

    /* Parse any specified input or output mappings */
    buffer = strdup(map);
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
            if (strcmp(token,"INLCR") == 0)
            {
                option.map_i_nl_cr = true;
            }
            else if (strcmp(token,"IGNCR") == 0)
            {
                option.map_ign_cr = true;
            }
            else if (strcmp(token,"ICRNL") == 0)
            {
                option.map_i_cr_nl = true;
            }
            else if (strcmp(token,"OCRNL") == 0)
            {
                option.map_o_cr_nl = true;
            }
            else if (strcmp(token,"ODELBS") == 0)
            {
                option.map_o_del_bs = true;
            }
            else if (strcmp(token,"IFFESCC") == 0)
            {
                option.map_i_ff_escc = true;
            }
            else if (strcmp(token,"INLCRNL") == 0)
            {
                option.map_i_nl_crnl = true;
            }
            else if (strcmp(token, "ONLCRNL") == 0)
            {
                option.map_o_nl_crnl = true;
            }
            else if (strcmp(token, "OLTU") == 0)
            {
                option.map_o_ltu = true;
            }
            else if (strcmp(token, "ONULBRK") == 0)
            {
                option.map_o_nulbrk = true;
            }
            else if (strcmp(token, "OIGNCR") == 0)
            {
                option.map_o_ign_cr = true;
            }
            else if (strcmp(token, "IMSB2LSB") == 0)
            {
                option.map_i_msb2lsb = true;
            }
            else
            {
                printf("Error: Unknown mapping flag %s\n", token);
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            token_found = false;
        }
    }
    free(buffer);
}

void options_print()
{
    tio_printf(" Device: %s", device_name);
    tio_printf(" Baudrate: %u", option.baudrate);
    tio_printf(" Databits: %d", option.databits);
    tio_printf(" Flow: %s", option_flow_to_string(option.flow));
    tio_printf(" Stopbits: %d", option.stopbits);
    tio_printf(" Parity: %s", option_parity_to_string(option.parity));
    tio_printf(" Local echo: %s", option.local_echo ? "true" : "false");
    tio_printf(" Timestamp: %s", option_timestamp_format_to_string(option.timestamp));
    tio_printf(" Timestamp timeout: %u", option.timestamp_timeout);
    tio_printf(" Output delay: %d", option.output_delay);
    tio_printf(" Output line delay: %d", option.output_line_delay);
    tio_printf(" Automatic connect strategy: %s", option_auto_connect_state_to_string(option.auto_connect));
    tio_printf(" Automatic reconnect: %s", option.no_reconnect ? "true" : "false");
    tio_printf(" Pulse duration: DTR=%d RTS=%d CTS=%d DSR=%d DCD=%d RI=%d", option.dtr_pulse_duration,
                                                                            option.rts_pulse_duration,
                                                                            option.cts_pulse_duration,
                                                                            option.dsr_pulse_duration,
                                                                            option.dcd_pulse_duration,
                                                                            option.ri_pulse_duration);
    tio_printf(" Input mode: %s", option_input_mode_to_string(option.input_mode));
    tio_printf(" Output mode: %s", option_output_mode_to_string(option.output_mode));
    tio_printf(" Alert: %s", option_alert_state_to_string(option.alert));
    if (option.log)
    {
        tio_printf(" Log file: %s", log_get_filename());
        if (option.log_directory != NULL)
        {
            tio_printf(" Log file directory: %s", option.log_directory);
        }
        tio_printf(" Log append: %s", option.log_append ? "true" : "false");
        tio_printf(" Log strip: %s", option.log_strip ? "true" : "false");
    }
    if (option.socket)
    {
        tio_printf(" Socket: %s", option.socket);
    }
    if (option.script_filename != NULL)
    {
        tio_printf(" Script file: %s", option.script_filename);
        tio_printf(" Script run: %s", script_run_state_to_string(option.script_run));
    }
}

void options_parse(int argc, char *argv[])
{
    int c;

    if (argc == 1)
    {
        option_print_help(argv);
        exit(EXIT_SUCCESS);
    }

    // Support no-color.org informal spec
    char *no_color = getenv("NO_COLOR");
    if (no_color != NULL && no_color[0] != '\0')
    {
        option.color = -1;
    }

    // Check for vt100 terminal
    char *term = getenv("TERM");
    if ((term != NULL) && (!strcmp(term, "vt100")))
    {
        option.vt100 = true;
    }

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
            {"auto-connect",         required_argument, 0, 'a'                     },
            {"exclude-devices",      required_argument, 0, OPT_EXCLUDE_DEVICES     },
            {"exclude-drivers",      required_argument, 0, OPT_EXCLUDE_DRIVERS     },
            {"exclude-tids",         required_argument, 0, OPT_EXCLUDE_TIDS        },
            {"no-reconnect",         no_argument,       0, 'n'                     },
            {"local-echo",           no_argument,       0, 'e'                     },
            {"timestamp",            no_argument,       0, 't'                     },
            {"timestamp-format",     required_argument, 0, OPT_TIMESTAMP_FORMAT    },
            {"timestamp-timeout",    required_argument, 0, OPT_TIMESTAMP_TIMEOUT   },
            {"list",                 no_argument,       0, 'l'                     },
            {"log",                  no_argument,       0, 'L'                     },
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
            {"exec",                 required_argument, 0, OPT_EXEC                },
            {"version",              no_argument,       0, 'v'                     },
            {"help",                 no_argument,       0, 'h'                     },
            {"complete-profiles",    no_argument,       0, OPT_COMPLETE_PROFILES   },
            {0,                      0,                 0,  0                      }
        };

        /* getopt_long stores the option index here */
        int option_index = 0;

        /* Parse argument using getopt_long */
        c = getopt_long(argc, argv, "b:d:f:s:p:o:O:a:netLlS:m:c:xrvh", long_options, &option_index);

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
                option_string_to_integer(optarg, &option.baudrate, "baudrate", 0, INT_MAX);
                break;

            case 'd':
                option_string_to_integer(optarg, &option.databits, "databits", 5, 8);
                break;

            case 'f':
                option_parse_flow(optarg, &option.flow);
                break;

            case 's':
                option_string_to_integer(optarg, &option.stopbits, "stopbits", 1, 2);
                break;

            case 'p':
                option_parse_parity(optarg, &option.parity);
                break;

            case 'o':
                option_string_to_integer(optarg, &option.output_delay, "output delay", 0, INT_MAX);
                break;

            case 'O':
                option_string_to_integer(optarg, &option.output_line_delay, "output line delay", 0, INT_MAX);
                break;

            case OPT_LINE_PULSE_DURATION:
                option_parse_line_pulse_duration(optarg);
                break;

            case 'a':
                option_parse_auto_connect(optarg, &option.auto_connect);
                break;

            case OPT_EXCLUDE_DEVICES:
                option.exclude_devices = optarg;
                break;

            case OPT_EXCLUDE_DRIVERS:
                option.exclude_drivers = optarg;
                break;

            case OPT_EXCLUDE_TIDS:
                option.exclude_tids = optarg;
                break;

            case 'n':
                option.no_reconnect = true;
                break;

            case 'e':
                option.local_echo = true;
                break;

            case 't':
                option.timestamp = TIMESTAMP_24HOUR;
                break;

            case OPT_TIMESTAMP_FORMAT:
                option_parse_timestamp(optarg, &option.timestamp);
                break;

            case OPT_TIMESTAMP_TIMEOUT:
                option_string_to_integer(optarg, &option.timestamp_timeout, "timestamp timeout", 0, INT_MAX);
                break;

            case 'L':
                option.log = true;
                break;

            case 'l':
                list_serial_devices();
                config_list_targets();
                exit(EXIT_SUCCESS);
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
                option_parse_mappings(optarg);
                break;

            case 'c':
                option_parse_color(optarg, &option.color);
                break;

            case OPT_INPUT_MODE:
                option_parse_input_mode(optarg, &option.input_mode);
                break;

            case OPT_OUTPUT_MODE:
                option_parse_output_mode(optarg, &option.output_mode);
                break;

            case OPT_RS485:
                option.rs485 = true;
                break;

            case OPT_RS485_CONFIG:
                rs485_parse_config(optarg);
                break;

            case OPT_ALERT:
                option_parse_alert(optarg, &option.alert);
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
                option_parse_script_run(optarg, &option.script_run);
                break;

            case OPT_EXEC:
                option.exec = optarg;
                break;

            case 'v':
                printf("tio v%s\n", VERSION);
                exit(EXIT_SUCCESS);
                break;

            case 'h':
                option_print_help(argv);
                exit(EXIT_SUCCESS);
                break;

            case OPT_COMPLETE_PROFILES:
                option.complete_profiles = true;
                break;

            case '?':
                /* getopt_long already printed an error message */
                exit(EXIT_FAILURE);
                break;

            default:
                exit(EXIT_FAILURE);
        }
    }

    /* Assume first non-option is the target (tty device, profile, tid) */
    if (strcmp(option.target, ""))
    {
            optind++;
    }
    else if (optind < argc)
    {
        option.target = argv[optind++];
    }

    if (option.complete_profiles)
    {
        return;
    }

    if (option.auto_connect != AUTO_CONNECT_DIRECT)
    {
        return;
    }

    if (strlen(option.target) == 0)
    {
        tio_error_print("Missing tty device, profile or topology ID");
        exit(EXIT_FAILURE);
    }

    /* Print any remaining command line arguments as unknown */
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
    /* Do 2nd pass to override settings set by configuration file */
    optind = 1; // Reset option index to restart scanning of argv
    options_parse(argc, argv);

#ifdef __CYGWIN__
    unsigned char portnum;
    char *tty_win;
    if ( ((strncmp("COM", option.target, 3) == 0)
        || (strncmp("com", option.target, 3) == 0) )
        && (sscanf(option.target + 3, "%hhu", &portnum) == 1)
        && (portnum > 0) )
    {
        asprintf(&tty_win, "/dev/ttyS%hhu", portnum - 1);
        option.target = tty_win;
    }
#endif
}
