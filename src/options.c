/*
 * tio - a simple TTY terminal I/O application
 *
 * Copyright (c) 2014-2016  Martin Lund
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

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <getopt.h>
#include <termios.h>
#include <limits.h>
#include "config.h"
#include "tio/options.h"
#include "tio/error.h"

struct option_t option =
{
    "",       // Device name
    false,    // No log
    "",       // Log filename
    false,    // No autoconnect
    0,        // No output delay
    {},
    115200,   // Baudrate
    8,        // Databits
    "none",   // Flow
    1,        // Stopbits
    "none"    // Parity
};

void print_options_help(char *argv[])
{
    printf("Usage: %s [<options>] <tty device>\n", argv[0]);
    printf("\n");
    printf("Options:\n");
    printf("  -b, --baudrate <bps>        Baud rate (default: 115200)\n");
    printf("  -d, --databits 5|6|7|8      Data bits (default: 8)\n");
    printf("  -f, --flow hard|soft|none   Flow control (default: none)\n");
    printf("  -s, --stopbits 1|2          Stop bits (default: 1)\n");
    printf("  -p, --parity odd|even|none  Parity (default: none)\n");
    printf("  -o, --output-delay <ms>     Output delay (default: 0)\n");
    printf("  -n, --no-autoconnect        Disable automatic connect\n");
    printf("  -l, --log <filename>        Log to file\n");
    printf("  -v, --version               Display version\n");
    printf("  -h, --help                  Display help\n");
    printf("\n");
    printf("In session, press ctrl-t + q to quit.\n");
    printf("\n");
}

void parse_options(int argc, char *argv[])
{
    int c;
    int baudrate;

    if (argc == 1)
    {
        print_options_help(argv);
        exit(EXIT_SUCCESS);
    }

    /* Set default termios settings:
     * (115200 baud, 8 data bits, no flow control, 1 stop bit, no parity) */
    bzero(&option.tio, sizeof(option.tio));
    option.tio.c_cflag = B115200 | CS8;

    /* Set default output delay */
    option.output_delay = 0;

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
            {"log",            required_argument, 0, 'l'},
            {"version",        no_argument,       0, 'v'},
            {"help",           no_argument,       0, 'h'},
            {0,                0,                 0,  0 }
        };

        /* getopt_long stores the option index here */
        int option_index = 0;

        /* Parse argument using getopt_long */
        c = getopt_long(argc, argv, "b:d:f:s:p:o:nl:vh", long_options, &option_index);

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
                option.baudrate = baudrate = atoi(optarg);
                switch (baudrate)
                {
                    case 0:
                        baudrate = B0;
                        break;
                    case 50:
                        baudrate = B50;
                        break;
                    case 75:
                        baudrate = B75;
                        break;
                    case 110:
                        baudrate = B110;
                        break;
                    case 134:
                        baudrate = B134;
                        break;
                    case 150:
                        baudrate = B150;
                        break;
                    case 300:
                        baudrate = B300;
                        break;
                    case 600:
                        baudrate = B600;
                        break;
                    case 1200:
                        baudrate = B1200;
                        break;
                    case 2400:
                        baudrate = B2400;
                        break;
                    case 4800:
                        baudrate = B4800;
                        break;
                    case 9600:
                        baudrate = B9600;
                        break;
                    case 19200:
                        baudrate = B19200;
                        break;
                    case 38400:
                        baudrate = B38400;
                        break;
                    case 57600:
                        baudrate = B57600;
                        break;
                    case 115200:
                        baudrate = B115200;
                        break;
                    case 230400:
                        baudrate = B230400;
                        break;
                    case 460800:
                        baudrate = B460800;
                        break;
                    case 500000:
                        baudrate = B500000;
                        break;
                    case 576000:
                        baudrate = B576000;
                        break;
                    case 921600:
                        baudrate = B921600;
                        break;
                    case 1000000:
                        baudrate = B1000000;
                        break;
                    case 1152000:
                        baudrate = B1152000;
                        break;
                    case 1500000:
                        baudrate = B1500000;
                        break;
                    case 2000000:
                        baudrate = B2000000;
                        break;
                    case 2500000:
                        baudrate = B2500000;
                        break;
                    case 3000000:
                        baudrate = B3000000;
                        break;
                    case 3500000:
                        baudrate = B3500000;
                        break;
                    case 4000000:
                        baudrate = B4000000;
                        break;
                    default:
                        error_printf("Invalid baud rate");
                        exit(EXIT_FAILURE);
                }

                cfsetispeed(&option.tio, baudrate);
                cfsetospeed(&option.tio, baudrate);

                break;

            case 'd':
                option.databits = atoi(optarg);
                option.tio.c_cflag &= ~CSIZE;
                switch (option.databits)
                {
                    case 5:
                        option.tio.c_cflag |= CS5;
                        break;
                    case 6:
                        option.tio.c_cflag |= CS6;
                        break;
                    case 7:
                        option.tio.c_cflag |= CS7;
                        break;
                    case 8:
                        option.tio.c_cflag |= CS8;
                        break;
                    default:
                        error_printf("Invalid data bits");
                        exit(EXIT_FAILURE);
                }
                break;

            case 'f':
                option.flow = optarg;

                if (strcmp("hard", optarg) == 0)
                {
                    option.tio.c_cflag |= CRTSCTS;
                    option.tio.c_iflag &= ~(IXON | IXOFF | IXANY);
                }
                else if (strcmp("soft", optarg) == 0)
                {
                    option.tio.c_cflag &= ~CRTSCTS;
                    option.tio.c_iflag |= IXON | IXOFF;
                }
                else if (strcmp("none", optarg) == 0)
                {
                    option.tio.c_cflag &= ~CRTSCTS;
                    option.tio.c_iflag &= ~(IXON | IXOFF | IXANY);
                }
                else
                {
                    error_printf("Invalid flow control");
                    exit(EXIT_FAILURE);
                }
                break;

            case 's':
                option.stopbits = atoi(optarg);
                switch (option.stopbits)
                {
                    case 1:
                        option.tio.c_cflag &= ~CSTOPB;
                        break;
                    case 2:
                        option.tio.c_cflag |= CSTOPB;
                        break;
                    default:
                        error_printf("Invalid stop bits");
                        exit(EXIT_FAILURE);
                }
                break;

            case 'p':
                option.parity = optarg;

                if (strcmp("odd", optarg) == 0)
                {
                    option.tio.c_cflag |= PARENB;
                    option.tio.c_cflag |= PARODD;
                }
                else if (strcmp("even", optarg) == 0)
                {
                    option.tio.c_cflag |= PARENB;
                    option.tio.c_cflag &= ~PARODD;
                }
                else if (strcmp("none", optarg) == 0)
                    option.tio.c_cflag &= ~PARENB;
                else
                {
                    error_printf("Invalid parity");
                    exit(EXIT_FAILURE);
                }
                break;

            case 'o':
                option.output_delay = atoi(optarg);
                break;

            case 'n':
                option.no_autoconnect = true;
                break;

            case 'l':
                option.log = true;
                option.log_filename = optarg;
                break;

            case 'v':
                printf("tio v%s\n", VERSION);
                printf("Copyright (c) 2014-2016 Martin Lund\n");
                printf("\n");
                printf("License GPLv2: GNU GPL version 2 or later <http://gnu.org/licenses/gpl-2.0.html>.\n");
                printf("This is free software: you are free to change and redistribute it.\n");
                printf("There is NO WARRANTY, to the extent permitted by law.\n");
                exit(EXIT_SUCCESS);
                break;

            case 'h':
                print_options_help(argv);
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
    if (optind < argc)
        option.tty_device = argv[optind++];

    if (strlen(option.tty_device) == 0)
    {
        error_printf("Missing device name");
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
