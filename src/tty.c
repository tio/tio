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

#if defined(__linux__)
#include <linux/serial.h>
#endif
#include "config.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <termios.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>
#include <dirent.h>
#include <pthread.h>
#include <glib.h>
#include "config.h"
#include "configfile.h"
#include "tty.h"
#include "print.h"
#include "options.h"
#include "misc.h"
#include "log.h"
#include "error.h"
#include "socket.h"
#include "setspeed.h"
#include "rs485.h"
#include "alert.h"
#include "timestamp.h"
#include "misc.h"
#include "script.h"
#include "xymodem.h"
#include "fs.h"

/* tty device listing configuration */

#if defined(__linux__)
#define PATH_SERIAL_DEVICES "/dev"
#define PATH_SERIAL_DEVICES_BY_ID "/dev/serial/by-id"
#define PATH_SERIAL_DEVICES_BY_PATH "/dev/serial/by-path"
#elif defined(__FreeBSD__)
#define PATH_SERIAL_DEVICES "/dev"
#elif defined(__APPLE__)
#define PATH_SERIAL_DEVICES "/dev"
#elif defined(__CYGWIN__)
#define PATH_SERIAL_DEVICES "/dev"
#elif defined(__HAIKU__)
#define PATH_SERIAL_DEVICES "/dev/ports"
#else
#define PATH_SERIAL_DEVICES "/dev"
#endif

#ifndef CMSPAR
#define CMSPAR   010000000000
#endif

#define MAX_LINE_LEN PATH_MAX
#define MAX_HISTORY 1000

#define KEY_0 0x30
#define KEY_1 0x31
#define KEY_2 0x32
#define KEY_3 0x33
#define KEY_4 0x34
#define KEY_5 0x35
#define KEY_6 0x36
#define KEY_7 0x37
#define KEY_8 0x38
#define KEY_9 0x39
#define KEY_QUESTION 0x3f
#define KEY_A 0x61
#define KEY_B 0x62
#define KEY_C 0x63
#define KEY_E 0x65
#define KEY_F 0x66
#define KEY_SHIFT_F 0x46
#define KEY_G 0x67
#define KEY_I 0x69
#define KEY_L 0x6C
#define KEY_SHIFT_L 0x4C
#define KEY_M 0x6D
#define KEY_O 0x6F
#define KEY_P 0x70
#define KEY_Q 0x71
#define KEY_R 0x72
#define KEY_SHIFT_R 0x52
#define KEY_S 0x73
#define KEY_T 0x74
#define KEY_V 0x76
#define KEY_X 0x78
#define KEY_Y 0x79
#define KEY_Z 0x7a

typedef enum
{
    LINE_TOGGLE,
    LINE_PULSE
} tty_line_mode_t;

typedef enum
{
    SUBCOMMAND_NONE,
    SUBCOMMAND_LINE_TOGGLE,
    SUBCOMMAND_LINE_PULSE,
    SUBCOMMAND_XMODEM,
    SUBCOMMAND_MAP,
} sub_command_t;

const char random_array[] =
{
0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x28, 0x20, 0x28, 0x0A, 0x20,
0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x29, 0x20, 0x29, 0x0A, 0x20,
0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E,
0x2E, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x7C, 0x20, 0x20, 0x20,
0x20, 0x20, 0x20, 0x7C, 0x5D, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
0x5C, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x2F, 0x0A, 0x20, 0x20, 0x20, 0x20,
0x20, 0x20, 0x20, 0x20, 0x60, 0x2D, 0x2D, 0x2D, 0x2D, 0x27, 0x0A, 0x0A, 0x54,
0x69, 0x6D, 0x65, 0x20, 0x66, 0x6F, 0x72, 0x20, 0x61, 0x20, 0x63, 0x6F, 0x66,
0x66, 0x65, 0x65, 0x20, 0x62, 0x72, 0x65, 0x61, 0x6B, 0x21, 0x0A, 0x20, 0x0A,
0x00
};

bool interactive_mode = true;

char key_hit = 0xff;

const char* device_name = NULL;
GList *device_list = NULL;
static struct termios tio, tio_old, stdout_new, stdout_old, stdin_new, stdin_old;
static unsigned long rx_total = 0, tx_total = 0;
static bool connected = false;
static bool standard_baudrate = true;
static void (*printchar)(char c);
static int device_fd;
static char hex_chars[2];
static unsigned char hex_char_index = 0;
static char tty_buffer[BUFSIZ*2];
static size_t tty_buffer_count = 0;
static char *tty_buffer_write_ptr = tty_buffer;
static pthread_t thread;
static int pipefd[2];
static pthread_mutex_t mutex_input_ready = PTHREAD_MUTEX_INITIALIZER;
static char line[MAX_LINE_LEN];

static void optional_local_echo(char c)
{
    if (!option.local_echo)
    {
        return;
    }

    printchar(c);

    if ((option.output_mode == OUTPUT_MODE_NORMAL) && (c == 127))
    {
        // Force destructive backspace
        printf("\b \b");
    }

    if (option.log)
    {
        log_putc(c);
    }
}

inline static bool is_valid_hex(char c)
{
    return ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
}

inline static unsigned char char_to_nibble(char c)
{
    if(c >= '0' && c <= '9')
    {
        return c - '0';
    }
    else if (c >= 'a' && c <= 'f')
    {
        return c - 'a' + 10;
    }
    else if (c >= 'A' && c <= 'F')
    {
        return c - 'A' + 10;
    }
    else
    {
        return 0;
    }
}

void tty_sync(int fd)
{
    ssize_t count;

    while (tty_buffer_count > 0)
    {
        count = write(fd, tty_buffer, tty_buffer_count);
        if (count < 0)
        {
            // Error
            tio_debug_printf("Write error while flushing tty buffer (%s)", strerror(errno));
            break;
        }
        tty_buffer_count -= count;
        fsync(fd);
        tcdrain(fd);
    }

    // Reset
    tty_buffer_write_ptr = tty_buffer;
    tty_buffer_count = 0;
}

ssize_t tty_write(int fd, const void *buffer, size_t count)
{
    ssize_t retval = 0, bytes_written = 0;
    size_t i;

    if (option.map_o_ltu)
    {
        // Convert lower case to upper case
        for (i = 0; i<count; i++)
        {
            *((unsigned char*)buffer+i) = toupper(*((unsigned char*)buffer+i));
        }
    }

    if (option.output_delay || option.output_line_delay)
    {
        // Write byte by byte with output delay
        for (i=0; i<count; i++)
        {
            retval = write(fd, buffer, 1);
            if (retval < 0)
            {
                // Error
                tio_debug_printf("Write error (%s)", strerror(errno));
                break;
            }
            bytes_written += retval;

            if (option.output_line_delay && *(unsigned char*)buffer == '\n')
            {
                delay(option.output_line_delay);
            }

            fsync(fd);
            tcdrain(fd);

            if (option.output_delay)
            {
                delay(option.output_delay);
            }
        }
    }
    else
    {
        // Force write of tty buffer if too full
        if ((tty_buffer_count + count) > BUFSIZ)
        {
            tty_sync(fd);
        }

        // Copy bytes to tty write buffer
        memcpy(tty_buffer_write_ptr, buffer, count);
        tty_buffer_write_ptr += count;
        tty_buffer_count += count;
        bytes_written = count;
    }

    return bytes_written;
}

void *tty_stdin_input_thread(void *arg)
{
    UNUSED(arg);
    char input_buffer[BUFSIZ];
    ssize_t byte_count;
    ssize_t bytes_written;

    // Create FIFO pipe
    if (pipe(pipefd) == -1)
    {
        tio_error_printf("Failed to create pipe");
        exit(EXIT_FAILURE);
    }

    // Signal that input pipe is ready
    pthread_mutex_unlock(&mutex_input_ready);

    // Input loop for stdin
    while (1)
    {
        /* Input from stdin ready */
        byte_count = read(STDIN_FILENO, input_buffer, BUFSIZ);
        if (byte_count < 0)
        {
            /* No error actually occurred */
            if (errno == EINTR)
            {
                continue;
            }
            tio_warning_printf("Could not read from stdin (%s)", strerror(errno));
        }
        else if (byte_count == 0)
        {
            // Close write end to signal EOF in read end
            close(pipefd[1]);
            pthread_exit(0);
        }

        if (interactive_mode)
        {
            static char previous_char = 0;
            char input_char;

            // Process quit and flush key command
            for (int i = 0; i<byte_count; i++)
            {
                // first do key hit check for xmodem abort
                if (!key_hit) {
                    key_hit = input_buffer[i];
                    byte_count--;
                    memcpy(input_buffer+i, input_buffer+i+1, byte_count-i);
                    continue;
                }

                input_char = input_buffer[i];

                if (option.prefix_enabled && previous_char == option.prefix_code)
                {
                    if (input_char == option.prefix_code)
                    {
                        previous_char = 0;
                        continue;
                    }

                    switch (input_char)
                    {
                        case KEY_Q:
                            exit(EXIT_SUCCESS);
                            break;
                        case KEY_SHIFT_F:
                            tio_printf("Flushed data I/O buffers")
                            tcflush(device_fd, TCIOFLUSH);
                            break;
                        default:
                            break;
                    }
                }
                previous_char = input_char;
            }
        }

        // Write all bytes read to pipe
        while (byte_count > 0)
        {
            bytes_written = write(pipefd[1], input_buffer, byte_count);
            if (bytes_written < 0)
            {
                tio_warning_printf("Could not write to pipe (%s)", strerror(errno));
                break;
            }
            byte_count -= bytes_written;
        }
    }

    pthread_exit(0);
}

void tty_input_thread_create(void)
{
    pthread_mutex_lock(&mutex_input_ready);

    if (pthread_create(&thread, NULL, tty_stdin_input_thread, NULL) != 0) {
        tio_error_printf("pthread_create() error");
        exit(1);
    }
}

void tty_input_thread_wait_ready(void)
{
    pthread_mutex_lock(&mutex_input_ready);
}

static void handle_hex_prompt(char c)
{
    hex_chars[hex_char_index++] = c;

    printf("%c", c);
    print_tainted_set();

    if (hex_char_index == 2)
    {
        usleep(100*1000);
        if (option.local_echo == false)
        {
            printf("\b \b");
            printf("\b \b");
        }
        else
        {
            printf(" ");
        }

        unsigned char hex_value = char_to_nibble(hex_chars[0]) << 4 | (char_to_nibble(hex_chars[1]) & 0x0F);
        hex_char_index = 0;

        ssize_t status = tty_write(device_fd, &hex_value, 1);
        if (status < 0)
        {
            tio_warning_printf("Could not write to tty device");
        }
        else
        {
            tx_total++;
        }
    }
}

static const char *tty_line_name(int mask)
{
    switch (mask)
    {
        case TIOCM_DTR:
            return "DTR";
        case TIOCM_RTS:
            return "RTS";
        case TIOCM_CTS:
            return "CTS";
        case TIOCM_DSR:
            return "DSR";
        case TIOCM_CD:
            return "CD";
        case TIOCM_RI:
            return "RI";
        default:
            return NULL;
    }
}

void tty_line_set(int fd, tty_line_config_t line_config[])
{
    static int state;
    int i = 0;

    if (ioctl(fd, TIOCMGET, &state) < 0)
    {
        tio_warning_printf("Could not get line state (%s)", strerror(errno));
        return;
    }

    for (i=0; i<6; i++)
    {
        if (line_config[i].reserved)
        {
            if (line_config[i].value == 0)
            {
                // Low
                state |= line_config[i].mask;
                tio_printf("Setting %s to LOW", tty_line_name(line_config[i].mask));
            }
            else if (line_config[i].value == 1)
            {
                // High
                state &= ~line_config[i].mask;
                tio_printf("Setting %s to HIGH", tty_line_name(line_config[i].mask));
            }
            else if (line_config[i].value == 2)
            {
                // Toggle
                state ^= line_config[i].mask;

                if (state & line_config[i].mask)
                {
                    tio_printf("Setting %s to LOW", tty_line_name(line_config[i].mask));
                }
                else
                {
                    tio_printf("Setting %s to HIGH", tty_line_name(line_config[i].mask));
                }
            }
        }
    }

    if (ioctl(fd, TIOCMSET, &state) < 0)
    {
        tio_warning_printf("Could not set line state (%s)", strerror(errno));
    }
}

void tty_line_toggle(int fd, int mask)
{
    int state;

    if (ioctl(fd, TIOCMGET, &state) < 0)
    {
        tio_warning_printf("Could not get line state (%s)", strerror(errno));
        return;
    }

    if (state & mask)
    {
        state &= ~mask;
        tio_printf("Setting %s to HIGH", tty_line_name(mask));
    }
    else
    {
        state |= mask;
        tio_printf("Setting %s to LOW", tty_line_name(mask));
    }

    if (ioctl(fd, TIOCMSET, &state) < 0)
    {
        tio_warning_printf("Could not set line state (%s)", strerror(errno));
    }
}

static void tty_line_pulse(int fd, int mask, unsigned int duration)
{
    tty_line_toggle(fd, mask);

    if (duration > 0)
    {
        tio_printf("Waiting %d ms", duration);
        delay(duration);
    }

    tty_line_toggle(fd, mask);
}

static void tty_line_poke(int fd, int mask, tty_line_mode_t mode, unsigned int duration)
{
    switch (mode)
    {
        case LINE_TOGGLE:
            tty_line_toggle(fd, mask);
            break;

        case LINE_PULSE:
            tty_line_pulse(fd, mask, duration);
            break;
    }
}

static int tio_readln(void)
{
    char *p = line;

    /* Read line, accept BS and DEL as rubout characters */
    for (p = line ; p < &line[MAX_LINE_LEN-1]; )
    {
        if (read(pipefd[0], p, 1) > 0)
        {
            if (*p == 0x08 || *p == 0x7f)
            {
                if (p > line)
                {
                    write(STDOUT_FILENO, "\b \b", 3);
                    p--;
                }
                continue;
            }
            write(STDOUT_FILENO, p, 1);
            if (*p == '\r') break;
            p++;
        }
    }
    *p = 0;
    return (p - line);
}

void tty_output_mode_set(output_mode_t mode)
{
    switch (mode)
    {
        case OUTPUT_MODE_NORMAL:
            printchar = print_normal;
            break;

        case OUTPUT_MODE_HEX:
            printchar = print_hex;
            break;

        case OUTPUT_MODE_END:
            break;
    }
}

static void mappings_print(void)
{
    if (option.map_i_cr_nl || option.map_ign_cr || option.map_i_ff_escc ||
        option.map_i_nl_cr || option.map_i_nl_crnl || option.map_o_cr_nl ||
        option.map_o_del_bs || option.map_o_nl_crnl || option.map_o_ltu ||
        option.map_o_nulbrk || option.map_o_msblsb)
    {
        tio_printf(" Mappings:%s%s%s%s%s%s%s%s%s%s%s",
                option.map_i_cr_nl ? " ICRNL" : "",
                option.map_ign_cr ? " IGNCR" : "",
                option.map_i_ff_escc ? " IFFESCC" : "",
                option.map_i_nl_cr ? " INLCR" : "",
                option.map_i_nl_crnl ? " INLCRNL" : "",
                option.map_o_cr_nl ? " OCRNL" : "",
                option.map_o_del_bs ? " ODELBS" : "",
                option.map_o_nl_crnl ? " ONLCRNL" : "",
                option.map_o_ltu ? " OLTU" : "",
                option.map_o_nulbrk ? " ONULBRK" : "",
                option.map_o_msblsb ? " MSB2LSB" : "");
    }
    else
    {
        tio_printf(" Mappings: none");
    }
}

void handle_command_sequence(char input_char, char *output_char, bool *forward)
{
    char unused_char;
    bool unused_bool;
    int state;
    static tty_line_mode_t line_mode;
    static sub_command_t sub_command = SUBCOMMAND_NONE;
    static char previous_char = 0;

    /* Ignore unused arguments */
    if (output_char == NULL)
    {
        output_char = &unused_char;
    }

    if (forward == NULL)
    {
        forward = &unused_bool;
    }

    // Handle sub commands
    if (sub_command)
    {
        *forward = false;

        switch (sub_command)
        {
            case SUBCOMMAND_NONE:
                break;

            case SUBCOMMAND_LINE_TOGGLE:
            case SUBCOMMAND_LINE_PULSE:
                switch (input_char)
                {
                    case KEY_0:
                        tty_line_poke(device_fd, TIOCM_DTR, line_mode, option.dtr_pulse_duration);
                        break;
                    case KEY_1:
                        tty_line_poke(device_fd, TIOCM_RTS, line_mode, option.rts_pulse_duration);
                        break;
                    case KEY_2:
                        tty_line_poke(device_fd, TIOCM_CTS, line_mode, option.cts_pulse_duration);
                        break;
                    case KEY_3:
                        tty_line_poke(device_fd, TIOCM_DSR, line_mode, option.dsr_pulse_duration);
                        break;
                    case KEY_4:
                        tty_line_poke(device_fd, TIOCM_CD, line_mode, option.dcd_pulse_duration);
                        break;
                    case KEY_5:
                        tty_line_poke(device_fd, TIOCM_RI, line_mode, option.ri_pulse_duration);
                        break;
                    default:
                        tio_error_print("Invalid line number");
                        break;
                }
                break;

            case SUBCOMMAND_XMODEM:
                switch (input_char)
                {
                    case KEY_0:
                        tio_printf("Send file with XMODEM-1K");
                        tio_printf_raw("Enter file name: ");
                        if (tio_readln())
                        {
                            tio_printf("Sending file '%s'  ", line);
                            tio_printf("Press any key to abort transfer");
                            tio_printf("%s", xymodem_send(device_fd, line, XMODEM_1K) < 0 ? "Aborted" : "Done");
                        }
                        break;

                    case KEY_1:
                        tio_printf("Send file with XMODEM-CRC");
                        tio_printf_raw("Enter file name: ");
                        if (tio_readln())
                        {
                            tio_printf("Sending file '%s'  ", line);
                            tio_printf("Press any key to abort transfer");
                            tio_printf("%s", xymodem_send(device_fd, line, XMODEM_CRC) < 0 ? "Aborted" : "Done");
                        }
                        break;

                    case KEY_2:
                        tio_printf("Receive file with XMODEM-CRC");
                        tio_printf_raw("Enter file name: ");
                        if (tio_readln())
                        {
                            tio_printf("Ready to receiving file '%s'  ", line);
                            tio_printf("Press any key to abort transfer");
                            tio_printf("%s", xymodem_receive(device_fd, line, XMODEM_CRC) < 0 ? "Aborted" : "Done");
                        }
                        break;

                    default:
                        tio_error_print("Invalid protocol option");
                        break;
                }
                break;

            case SUBCOMMAND_MAP:
                switch (input_char)
                {
                    case KEY_0:
                        option.map_i_cr_nl = !option.map_i_cr_nl;
                        tty_reconfigure();
                        tio_printf("ICRNL is %s", option.map_i_cr_nl ? "set" : "unset");
                        break;
                    case KEY_1:
                        option.map_ign_cr = !option.map_ign_cr;
                        tty_reconfigure();
                        tio_printf("IGNCR is %s", option.map_ign_cr ? "set" : "unset");
                        break;
                    case KEY_2:
                        option.map_i_ff_escc = !option.map_i_ff_escc;
                        tio_printf("IFFESCC is %s", option.map_i_ff_escc ? "set" : "unset");
                        break;
                    case KEY_3:
                        option.map_i_nl_cr = !option.map_i_nl_cr;
                        tty_reconfigure();
                        tio_printf("INLCR is %s", option.map_i_nl_cr ? "set" : "unset");
                        break;
                    case KEY_4:
                        option.map_i_nl_crnl = !option.map_i_nl_crnl;
                        tio_printf("INLCRNL is %s", option.map_i_nl_crnl ? "set" : "unset");
                        break;
                    case KEY_5:
                        option.map_o_cr_nl = !option.map_o_cr_nl;
                        tio_printf("OCRNL is %s", option.map_o_cr_nl ? "set" : "unset");
                        break;
                    case KEY_6:
                        option.map_o_del_bs = !option.map_o_del_bs;
                        tio_printf("ODELBS is %s", option.map_o_del_bs ? "set" : "unset");
                        break;
                    case KEY_7:
                        option.map_o_nl_crnl = !option.map_o_nl_crnl;
                        tio_printf("ONLCRNL is %s", option.map_o_nl_crnl ? "set" : "unset");
                        break;
                    case KEY_8:
                        option.map_o_ltu = !option.map_o_ltu;
                        tio_printf("OLTU is %s", option.map_o_ltu ? "set" : "unset");
                        break;
                    case KEY_9:
                        option.map_o_nulbrk = !option.map_o_nulbrk;
                        tio_printf("ONULBRK is %s", option.map_o_nulbrk ? "set" : "unset");
                        break;
                    case KEY_A:
                        option.map_o_msblsb = !option.map_o_msblsb;
                        tio_printf("MSB2LSB is %s", option.map_o_msblsb ? "set" : "unset");
                        break;
                    default:
                        tio_error_print("Invalid input");
                        break;
                }
                break;
        }

        sub_command = SUBCOMMAND_NONE;
        return;
    }

    /* Handle escape key commands */
    if (option.prefix_enabled && previous_char == option.prefix_code)
    {
        /* Do not forward input char to output by default */
        *forward = false;

        /* Handle special double prefix key input case */
        if (input_char == option.prefix_code)
        {
            /* Forward prefix character to tty */
            *output_char = option.prefix_code;
            *forward = true;
            previous_char = 0;
            return;
        }

        // Handle commands
        switch (input_char)
        {
            case KEY_QUESTION:
                tio_printf("Key commands:");
                tio_printf(" ctrl-%c ?       List available key commands", option.prefix_key);
                tio_printf(" ctrl-%c b       Send break", option.prefix_key);
                tio_printf(" ctrl-%c c       Show configuration", option.prefix_key);
                tio_printf(" ctrl-%c e       Toggle local echo mode", option.prefix_key);
                tio_printf(" ctrl-%c f       Toggle log to file", option.prefix_key);
                tio_printf(" ctrl-%c F       Flush data I/O buffers", option.prefix_key);
                tio_printf(" ctrl-%c g       Toggle serial port line", option.prefix_key);
                tio_printf(" ctrl-%c i       Toggle input mode", option.prefix_key);
                tio_printf(" ctrl-%c l       Clear screen", option.prefix_key);
                tio_printf(" ctrl-%c L       Show line states", option.prefix_key);
                tio_printf(" ctrl-%c m       Change mapping of characters on input or output", option.prefix_key);
                tio_printf(" ctrl-%c o       Toggle output mode", option.prefix_key);
                tio_printf(" ctrl-%c p       Pulse serial port line", option.prefix_key);
                tio_printf(" ctrl-%c q       Quit", option.prefix_key);
                tio_printf(" ctrl-%c r       Run script", option.prefix_key);
                tio_printf(" ctrl-%c R       Execute shell command with I/O redirected to device", option.prefix_key);
                tio_printf(" ctrl-%c s       Show statistics", option.prefix_key);
                tio_printf(" ctrl-%c t       Toggle line timestamp mode", option.prefix_key);
                tio_printf(" ctrl-%c v       Show version", option.prefix_key);
                tio_printf(" ctrl-%c x       Send/Receive file via Xmodem", option.prefix_key);
                tio_printf(" ctrl-%c y       Send file via Ymodem", option.prefix_key);
                tio_printf(" ctrl-%c ctrl-%c  Send ctrl-%c character", option.prefix_key, option.prefix_key, option.prefix_key);
                break;

            case KEY_SHIFT_L:
                if (ioctl(device_fd, TIOCMGET, &state) < 0)
                {
                    tio_warning_printf("Could not get line state (%s)", strerror(errno));
                    break;
                }
                tio_printf("Line states:");
                tio_printf(" DTR: %s", (state & TIOCM_DTR) ? "LOW" : "HIGH");
                tio_printf(" RTS: %s", (state & TIOCM_RTS) ? "LOW" : "HIGH");
                tio_printf(" CTS: %s", (state & TIOCM_CTS) ? "LOW" : "HIGH");
                tio_printf(" DSR: %s", (state & TIOCM_DSR) ? "LOW" : "HIGH");
                tio_printf(" DCD: %s", (state & TIOCM_CD) ? "LOW" : "HIGH");
                tio_printf(" RI : %s", (state & TIOCM_RI) ? "LOW" : "HIGH");
                break;

            case KEY_F:
                if (option.log)
                {
                    log_close();
                    option.log = false;
                }
                else
                {
                    if (log_open(option.log_filename) == 0)
                    {
                        option.log = true;
                    }
                }
                tio_printf("Switched log to file %s", option.log ? "on" : "off");
                break;

            case KEY_SHIFT_F:
                break;

            case KEY_G:
                tio_printf("Please enter which serial line number to toggle:");
                tio_printf("(0) DTR");
                tio_printf("(1) RTS");
                tio_printf("(2) CTS");
                tio_printf("(3) DSR");
                tio_printf("(4) DCD");
                tio_printf("(5) RI");
                line_mode = LINE_TOGGLE;
                // Process next input character as sub command
                sub_command = SUBCOMMAND_LINE_TOGGLE;
                break;

            case KEY_P:
                tio_printf("Please enter which serial line number to pulse:");
                tio_printf("(0) DTR");
                tio_printf("(1) RTS");
                tio_printf("(2) CTS");
                tio_printf("(3) DSR");
                tio_printf("(4) DCD");
                tio_printf("(5) RI");
                line_mode = LINE_PULSE;
                // Process next input character as sub command
                sub_command = SUBCOMMAND_LINE_PULSE;
                break;

            case KEY_B:
                tcsendbreak(device_fd, 0);
                break;

            case KEY_C:
                tio_printf("Configuration:");
                options_print();
                config_file_print();
                if (option.rs485)
                {
                    rs485_print_config();
                }
                mappings_print();
                break;

            case KEY_E:
                option.local_echo = !option.local_echo;
                tio_printf("Switched local echo %s", option.local_echo ? "on" : "off");
                break;

            case KEY_I:
                option.input_mode += 1;
                switch (option.input_mode)
                {
                    case INPUT_MODE_NORMAL:
                        break;

                    case INPUT_MODE_HEX:
                        option.input_mode = INPUT_MODE_HEX;
                        tio_printf("Switched input mode to hex");
                        break;

                    case INPUT_MODE_LINE:
                        option.input_mode = INPUT_MODE_LINE;
                        tio_printf("Switched input mode to line");
                        break;

                    case INPUT_MODE_END:
                        option.input_mode = INPUT_MODE_NORMAL;
                        tio_printf("Switched input mode to normal");
                        break;
                }
                break;

            case KEY_O:
                option.output_mode += 1;
                switch (option.output_mode)
                {
                    case OUTPUT_MODE_NORMAL:
                        break;

                    case OUTPUT_MODE_HEX:
                        tty_output_mode_set(OUTPUT_MODE_HEX);
                        tio_printf("Switched output mode to hex");
                        break;

                    case OUTPUT_MODE_END:
                        option.output_mode = OUTPUT_MODE_NORMAL;
                        tty_output_mode_set(OUTPUT_MODE_NORMAL);
                        tio_printf("Switched output mode to normal");
                        break;
                }
                break;

            case KEY_L:
                /* Clear screen using ANSI/VT100 escape code */
                printf("\033c");
                break;

            case KEY_M:
                /* Change mapping of characters on input or output */
                tio_printf("Please enter which mapping to set or unset:");
                tio_printf(" (0) ICRNL: %s mapping CR to NL on input (unless IGNCR is set)",
                        option.map_i_cr_nl ? "Unset" : "Set");
                tio_printf(" (1) IGNCR: %s ignoring CR on input",
                        option.map_ign_cr ? "Unset" : "Set");
                tio_printf(" (2) IFFESCC: %s mapping FF to ESC-c on input",
                        option.map_i_ff_escc ? "Unset" : "Set");
                tio_printf(" (3) INLCR: %s mapping NL to CR on input",
                        option.map_i_nl_cr ? "Unset" : "Set");
                tio_printf(" (4) INLCRNL: %s mapping NL to CR-NL on input",
                        option.map_i_nl_cr ? "Unset" : "Set");
                tio_printf(" (5) OCRNL: %s mapping CR to NL on output",
                        option.map_o_cr_nl ? "Unset" : "Set");
                tio_printf(" (6) ODELBS: %s mapping DEL to BS on output",
                        option.map_o_del_bs ? "Unset" : "Set");
                tio_printf(" (7) ONLCRNL: %s mapping NL to CR-NL on output",
                        option.map_o_nl_crnl ? "Unset" : "Set");
                tio_printf(" (8) OLTU: %s mapping lowercase to uppercase on output",
                        option.map_o_ltu ? "Unset" : "Set");
                tio_printf(" (9) ONULBRK: %s mapping NUL to send break signal on output",
                        option.map_o_nulbrk ? "Unset" : "Set");
                tio_printf(" (a) MSB2LSB: %s mapping MSB bit order to LSB on output",
                        option.map_o_msblsb ? "Unset" : "Set");

                // Process next input character as sub command
                sub_command = SUBCOMMAND_MAP;
                break;

            case KEY_Q:
                /* Exit upon ctrl-t q sequence */
                exit(EXIT_SUCCESS);

            case KEY_R:
                /* Run script */
                tio_printf("Run Lua script")
                tio_printf_raw("Enter file name: ");
                if (tio_readln())
                {
                    clear_line();
                    script_run(device_fd, line);
                }
                else
                {
                    clear_line();
                    script_run(device_fd, NULL);
                }
                break;

            case KEY_SHIFT_R:
                /* Execute shell command */
                tio_printf("Execute shell command with I/O redirected to device");
                tio_printf_raw("Enter command: ");
                if (tio_readln())
                    execute_shell_command(device_fd, line);
                break;

            case KEY_S:
                /* Show tx/rx statistics upon ctrl-t s sequence */
                tio_printf("Statistics:");
                tio_printf(" Sent %lu bytes", tx_total);
                tio_printf(" Received %lu bytes", rx_total);
                break;

            case KEY_T:
                option.timestamp += 1;
                switch (option.timestamp)
                {
                    case TIMESTAMP_NONE:
                        break;
                    case TIMESTAMP_24HOUR:
                        tio_printf("Switched timestamp mode to 24hour");
                        break;
                    case TIMESTAMP_24HOUR_START:
                        tio_printf("Switched timestamp mode to 24hour-start");
                        break;
                    case TIMESTAMP_24HOUR_DELTA:
                        tio_printf("Switched timestamp mode to 24hour-delta");
                        break;
                    case TIMESTAMP_ISO8601:
                        tio_printf("Switched timestamp mode to iso8601");
                        break;
                    case TIMESTAMP_END:
                        option.timestamp = TIMESTAMP_NONE;
                        tio_printf("Switched timestamp mode off");
                        break;
                }
                break;

            case KEY_V:
                tio_printf("tio v%s", VERSION);
                break;

            case KEY_X:
                tio_printf("Please enter which X modem protocol to use:");
                tio_printf(" (0) XMODEM-1K send");
                tio_printf(" (1) XMODEM-CRC send");
                tio_printf(" (2) XMODEM-CRC receive");
                // Process next input character as sub command
                sub_command = SUBCOMMAND_XMODEM;
                break;

            case KEY_Y:
                tio_printf("Send file with YMODEM");
                tio_printf_raw("Enter file name: ");
                if (tio_readln()) {
                    tio_printf("Sending file '%s'  ", line);
                    tio_printf("Press any key to abort transfer");
                    tio_printf("%s", xymodem_send(device_fd, line, YMODEM) < 0 ? "Aborted" : "Done");
                }
                break;

            case KEY_Z:
                tio_printf_array(random_array);
                break;

            default:
                /* Ignore unknown ctrl-t escaped keys */
                break;
        }
    }

    previous_char = input_char;
}

void stdin_restore(void)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &stdin_old);
}

void stdin_configure(void)
{
    int status;

    /* Save current stdin settings */
    if (tcgetattr(STDIN_FILENO, &stdin_old) < 0)
    {
        tio_error_printf("Saving current stdin settings failed");
        exit(EXIT_FAILURE);
    }

    /* Prepare new stdin settings */
    memcpy(&stdin_new, &stdin_old, sizeof(stdin_old));

    /* Reconfigure stdin (RAW configuration) */
    cfmakeraw(&stdin_new);

    /* Control characters */
    stdin_new.c_cc[VTIME] = 0; /* Inter-character timer unused */
    stdin_new.c_cc[VMIN]  = 1; /* Blocking read until 1 character received */

    /* Activate new stdin settings */
    status = tcsetattr(STDIN_FILENO, TCSANOW, &stdin_new);
    if (status == -1)
    {
        tio_error_printf("Could not apply new stdin settings (%s)", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Make sure we restore old stdin settings on exit */
    atexit(&stdin_restore);
}

void stdout_restore(void)
{
    tcsetattr(STDOUT_FILENO, TCSANOW, &stdout_old);

    // If terminal is vt100
    if (option.vt100)
    {
      // Disable DEC Special Graphics character set just in case it was randomly
      // enabled by noise from serial device.
      putchar('\017');
    }
}

void stdout_configure(void)
{
    int status;

    /* Disable line buffering in stdout. This is necessary if we
     * want things like local echo to work correctly. */
    setvbuf(stdout, NULL, _IONBF, 0);

    /* Save current stdout settings */
    if (tcgetattr(STDOUT_FILENO, &stdout_old) < 0)
    {
        tio_error_printf("Saving current stdio settings failed");
        exit(EXIT_FAILURE);
    }

    /* Prepare new stdout settings */
    memcpy(&stdout_new, &stdout_old, sizeof(stdout_old));

    /* Reconfigure stdout (RAW configuration) */
    cfmakeraw(&stdout_new);

    /* Allow ^C / SIGINT (to allow termination when piping to tio) */
    if (!interactive_mode)
    {
        stdout_new.c_lflag |= ISIG;
    }

    /* Control characters */
    stdout_new.c_cc[VTIME] = 0; /* Inter-character timer unused */
    stdout_new.c_cc[VMIN]  = 1; /* Blocking read until 1 character received */

    /* Activate new stdout settings */
    status = tcsetattr(STDOUT_FILENO, TCSANOW, &stdout_new);
    if (status == -1)
    {
        tio_error_printf("Could not apply new stdout settings (%s)", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* At start use normal print function */
    printchar = print_normal;

    /* Make sure we restore old stdout settings on exit */
    atexit(&stdout_restore);
}

void tty_configure(void)
{
    int status;
    speed_t baudrate;

    memset(&tio, 0, sizeof(tio));

    /* Set speed */
    switch (option.baudrate)
    {
        /* The macro below expands into switch cases autogenerated by meson
         * configure. Each switch case verifies and configures the baud
         * rate and is of the form:
         *
         * case $baudrate: baudrate = B$baudrate; break;
         *
         * Only switch cases for baud rates detected supported by the host
         * system are inserted.
         *
         * To see which baud rates are being probed see meson.build
         */
        BAUDRATE_CASES

        default:
#if defined (HAVE_TERMIOS2) || defined (HAVE_IOSSIOSPEED)
            standard_baudrate = false;
            break;
#else
            tio_error_printf("Invalid baud rate");
            exit(EXIT_FAILURE);
#endif
    }

    if (standard_baudrate)
    {
        // Set input speed
        status = cfsetispeed(&tio, baudrate);
        if (status == -1)
        {
            tio_error_printf("Could not configure input speed (%s)", strerror(errno));
            exit(EXIT_FAILURE);
        }

        // Set output speed
        status = cfsetospeed(&tio, baudrate);
        if (status == -1)
        {
            tio_error_printf("Could not configure output speed (%s)", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    /* Set databits */
    tio.c_cflag &= ~CSIZE;
    switch (option.databits)
    {
        case 5:
            tio.c_cflag |= CS5;
            break;
        case 6:
            tio.c_cflag |= CS6;
            break;
        case 7:
            tio.c_cflag |= CS7;
            break;
        case 8:
            tio.c_cflag |= CS8;
            break;
        default:
            tio_error_printf("Invalid data bits");
            exit(EXIT_FAILURE);
    }

    /* Set flow control */
    switch (option.flow)
    {
        case FLOW_NONE:
            tio.c_cflag &= ~CRTSCTS;
            tio.c_iflag &= ~(IXON | IXOFF | IXANY);
            break;

        case FLOW_HARD:
            tio.c_cflag |= CRTSCTS;
            tio.c_iflag &= ~(IXON | IXOFF | IXANY);
            break;

        case FLOW_SOFT:
            tio.c_cflag &= ~CRTSCTS;
            tio.c_iflag |= IXON | IXOFF;
            break;

        default:
            tio_error_printf("Invalid flow control");
            exit(EXIT_FAILURE);
    }

    /* Set stopbits */
    switch (option.stopbits)
    {
        case 1:
            tio.c_cflag &= ~CSTOPB;
            break;
        case 2:
            tio.c_cflag |= CSTOPB;
            break;
        default:
            tio_error_printf("Invalid stop bits");
            exit(EXIT_FAILURE);
    }

    /* Set parity */
    switch (option.parity)
    {
        case PARITY_NONE:
            tio.c_cflag &= ~PARENB;
            break;

        case PARITY_ODD:
            tio.c_cflag |= PARENB;
            tio.c_cflag |= PARODD;
            break;

        case PARITY_EVEN:
            tio.c_cflag |= PARENB;
            tio.c_cflag &= ~PARODD;
            break;

        case PARITY_MARK:
            tio.c_cflag |= PARENB;
            tio.c_cflag |= PARODD;
            tio.c_cflag |= CMSPAR;
            break;

        case PARITY_SPACE:
            tio.c_cflag |= PARENB;
            tio.c_cflag &= ~PARODD;
            tio.c_cflag |= CMSPAR;
            break;

        default:
            tio_error_printf("Invalid parity");
            exit(EXIT_FAILURE);
    }

    /* Control, input, output, local modes for tty device */
    tio.c_cflag |= CLOCAL | CREAD;
    tio.c_oflag = 0;
    tio.c_lflag = 0;

    /* Control characters */
    tio.c_cc[VTIME] = 0; // Inter-character timer unused
    tio.c_cc[VMIN]  = 1; // Blocking read until 1 character received

    /* Configure input mappings */
    if (option.map_i_nl_cr)
    {
        tio.c_iflag |= INLCR;
    }
    if (option.map_ign_cr)
    {
        tio.c_iflag |= IGNCR;
    }
    if (option.map_i_cr_nl)
    {
        tio.c_iflag |= ICRNL;
    }
}

void tty_reconfigure(void)
{
    tty_configure();

    if (connected)
    {
        /* Activate new port settings */
        tcsetattr(device_fd, TCSANOW, &tio);
    }
}

static bool is_serial_device(const char *format, ...)
{
    char filename[PATH_MAX];
    struct winsize ws;
    int bytes_printed;
    int status = true;
    struct stat st;
    va_list args;
    int fd = -1;

    va_start(args, format);
    bytes_printed = vsnprintf(filename, sizeof(filename), format, args);
    va_end(args);

    if (bytes_printed < 0)
    {
        return false;
    }

    fd = open(filename, O_RDONLY | O_NONBLOCK | O_NOCTTY);
    if (fd == -1)
    {
        return false;
    }

    if (fstat(fd, &st) == -1)
    {
        return false;
    }

    // Make sure it is a character device
    if ((st.st_mode & S_IFMT) != S_IFCHR)
    {
        return false;
    }

    // Make sure it is a tty
    status = isatty(fd);
    if (status == 0)
    {
        goto error;
    }

    // Serial devices do not have rows and columns
    status = ioctl(fd, TIOCGWINSZ, &ws);
    if (status == 0)
    {
        status = true;
        if (ws.ws_row && ws.ws_col)
        {
            status = false;
            goto error;
        }
    }

error:
    close(fd);
    return status;
}

static void list_serial_devices_by_id(void)
{
#ifdef PATH_SERIAL_DEVICES_BY_ID
    DIR *d = opendir(PATH_SERIAL_DEVICES_BY_ID);
    if (d)
    {
        struct dirent *dir;

        printf("By-id\n");
        printf("--------------------------------------------------------------------------------\n");

        while ((dir = readdir(d)) != NULL)
        {
            if ((strcmp(dir->d_name, ".")) && (strcmp(dir->d_name, "..")))
            {
                if (is_serial_device("%s/%s", PATH_SERIAL_DEVICES_BY_ID, dir->d_name))
                {
                    printf("%s/%s\n", PATH_SERIAL_DEVICES_BY_ID, dir->d_name);
                }
            }
        }
        closedir(d);
    }
#endif
}

static void list_serial_devices_by_path(void)
{
#ifdef PATH_SERIAL_DEVICES_BY_PATH
    DIR *d = opendir(PATH_SERIAL_DEVICES_BY_PATH);
    if (d)
    {
        struct dirent *dir;

        printf("\nBy-path\n");
        printf("--------------------------------------------------------------------------------\n");

        while ((dir = readdir(d)) != NULL)
        {
            if ((strcmp(dir->d_name, ".")) && (strcmp(dir->d_name, "..")))
            {
                if (is_serial_device("%s/%s", PATH_SERIAL_DEVICES_BY_PATH, dir->d_name))
                {
                    printf("%s/%s\n", PATH_SERIAL_DEVICES_BY_PATH, dir->d_name);
                }
            }
        }
        closedir(d);
    }
#endif
}

static gint compare_uptime(gconstpointer a, gconstpointer b)
{
    device_t *device_a = (device_t *) a;
    device_t *device_b = (device_t *) b;

    // Make sure we end up with device with smallest uptime last in list
    if (device_a->uptime > device_b->uptime)
        return -1;
    else if (device_a->uptime < device_b->uptime)
        return 1;
    else
        return 0;
}

#if defined(__linux__)

// Function to get serial port type as a string
const char* get_serial_port_type(const char* port_name)
{
    int fd;
    static struct serial_struct serial_info;

    // Open the serial port
    fd = open(port_name, O_RDWR);
    if (fd == -1)
    {
        return "";
    }

    // Get serial port information
    if (ioctl(fd, TIOCGSERIAL, &serial_info) == -1)
    {
        close(fd);
        return "";
    }

    // Close the serial port
    close(fd);

    // Return the serial port type as a string
    switch (serial_info.type)
    {
        case PORT_UNKNOWN:
            return "Unknown";

        case PORT_8250:
            return "8250 UART";

        case PORT_16450:
            return "16450 UART";

        case PORT_16550:
            return "16550 UART";

        case PORT_16550A:
            return "16550A UART";

        case PORT_16650:
            return "16650 UART";

        case PORT_16650V2:
            return "16650V2 UART";

        case PORT_16750:
            return "16750 UART";

        case PORT_STARTECH:
            return "Startech UART";

        case PORT_16850:
            return "16850 UART";

        case PORT_16C950:
            return "16C950 UART";

        case PORT_16654:
            return "16654 UART";

        case PORT_RSA:
            return "RSA UART";

        default:
            return "";
    }
}

#else

const char* get_serial_port_type(const char* port_name)
{
    return "";
}

#endif

static void search_reset(void)
{
    GList *iter;

    if (g_list_length(device_list) == 0)
    {
        return;
    }

    // Free data of all list elements
    for (iter = device_list; iter != NULL; iter = g_list_next(iter))
    {
        device_t *device = (device_t *) iter->data;
        g_free(device->tid);
        g_free(device->path);
        g_free(device->driver);
        g_free(device->description);
    }

    // Free all list elements
    g_list_free_full(device_list, g_free);

    // Indicate an empty list
    device_list = NULL;
}

#if defined(__linux__)

GList *tty_search_for_serial_devices(void)
{
    DIR *dir;
    char path[PATH_MAX] = {};
    char device_path[PATH_MAX] = {};
    char driver_path[PATH_MAX] = {};
    double current_time, creation_time;
    ssize_t length;

    search_reset();

    // Open the sysfs directory for the tty subsystem
    dir = opendir("/sys/class/tty");
    if (!dir)
    {
        return NULL;
    }

    current_time = get_current_time();

    // Iterate through each device in the subsystem directory
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        // Skip . and .. entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        // Skip non serial devices
        if (is_serial_device("/dev/%s", entry->d_name) == false)
        {
            continue;
        }

        // Construct the path to the device's device symlink
        snprintf(path, sizeof(path), "/sys/class/tty/%s/device", entry->d_name);

        // Read the device symlink to get the device path
        // Example symlinks:
        //  /sys/class/tty/ttyUSB0/device -> ../../../ttyUSB0
        //  /sys/class/tty/ttyACM0/device -> ../../../3-6.4:1.2
        length = readlink(path, device_path, sizeof(device_path) - 1);
        if (length == -1)
        {
            continue;
        }

        // Null-terminate the string
        device_path[length] = '\0';

        // Extract last part of device path (string after last '/')
        // Example resulting device_name:
        //  "ttyUSB0"
        //  "3-6.4:1.2"
        char *last_part = strrchr(device_path, '/');
        last_part++; // Move past the '/'

        // Find that part in /sys/devices and return first result string
        // Example devices_path:
        //  "/sys/devices/pci0000:00/0000:00:14.0/usb3/3-6/3-6.3/3-6.3:1.0/ttyUSB0"
        //  "/sys/devices/pci0000:00/0000:00:14.0/usb3/3-6/3-6.4/3-6.4:1.2"
        char *devices_path = fs_search_directory("/sys/devices", last_part);
        if (devices_path == NULL)
        {
            continue;
        }

        // Remove last part if it contains device short name (e.g ttyUSB0)
        // Example resulting devices_path:
        //  "/sys/devices/pci0000:00/0000:00:14.0/usb3/3-6/3-6.3/3-6.3:1.0"
        //  "/sys/devices/pci0000:00/0000:00:14.0/usb3/3-6/3-6.4/3-6.4:1.2"
        last_part = strrchr(devices_path, '/');
        last_part++;
        if (strcmp(last_part, entry->d_name) == 0)
        {
            // Remove last part (string after last '/')
            char *slash = strrchr(devices_path, '/');
            int index = (int) (slash - devices_path);
            devices_path[index] = '\0';
        }

        // Hash remaining string to get unique topology ID
        unsigned long hash = djb2_hash((const unsigned char *)devices_path);
        char *tid = base62_encode(hash);
        free(devices_path);

        // Construct the path to the device's driver symlink
        snprintf(path, sizeof(path), "/sys/class/tty/%s/device/driver", entry->d_name);

        // Read the symlink to get the driver's path
        length = readlink(path, driver_path, sizeof(driver_path) - 1);
        if (length == -1)
        {
            continue;
        }

        // Null-terminate the string
        driver_path[length] = '\0';

        // Extract the driver name from the path
        char *driver = strrchr(driver_path, '/');
        if (driver == NULL)
        {
            continue;
        }
        driver++; // Move past the last '/'

        // Construct the path to the TTY device file
        snprintf(path, sizeof(path), "/dev/%s", entry->d_name);

        // Calculate uptime
        creation_time = fs_get_creation_time(path);
        double uptime = current_time - creation_time;

        // Read sysfs files to get best possible description of the driver
        char description[50] = {};
        length = fs_read_file_stripped(description, sizeof(description), "/sys/class/tty/%s/device/interface", entry->d_name);
        if (length == -1)
        {
            length = fs_read_file_stripped(description, sizeof(description), "/sys/class/tty/%s/device/../interface", entry->d_name);
        }
        if (length == -1)
        {
            length = fs_read_file_stripped(description, sizeof(description), "/sys/class/tty/%s/device/../../product", entry->d_name);
        }
        if (length == -1)
        {
            snprintf(description, sizeof(description), "%s", get_serial_port_type(path));
        }

        // Do not add devices excluded by exclude patterns
        if (match_patterns(path, option.exclude_devices))
        {
            continue;
        }
        if (match_patterns(driver, option.exclude_drivers))
        {
            continue;
        }
        if (match_patterns(tid, option.exclude_tids))
        {
            continue;
        }

        // Allocate new device item for device list
        device_t *device = g_new0(device_t, 1);
        if (device == NULL)
        {
            continue;
        }

        // Fill in device information
        device->path = g_strdup(path);
        device->tid = g_strdup(tid);
        device->uptime = uptime;
        device->driver = g_strdup(driver);
        device->description = g_strdup(description);

        // Add device information to device list
        device_list = g_list_append(device_list, device);
    }

    if (g_list_length(device_list) == 0)
    {
        // Return NULL if no serial devices found
        return NULL;
    }

    // Sort device list device with respect to uptime
    device_list = g_list_sort(device_list, compare_uptime);

    closedir(dir);

    return device_list;
}

#else

GList *tty_search_for_serial_devices(void)
{
    DIR *dir;
    char path[PATH_MAX] = {};
    double current_time, creation_time;

    search_reset();

    // Open the directory containing serial devices
    dir = opendir(PATH_SERIAL_DEVICES);
    if (!dir)
    {
        return NULL;
    }

    current_time = get_current_time();

    // Iterate through each device in the subsystem directory
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        // Skip . and .. entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        // Construct the path to the TTY device file
        snprintf(path, sizeof(path), PATH_SERIAL_DEVICES "/%s", entry->d_name);

        // Skip non serial devices
        if (is_serial_device(path) == false)
        {
            continue;
        }

        // Calculate uptime
        creation_time = fs_get_creation_time(path);
        double uptime = current_time - creation_time;

        // Do not add devices excluded by exclude patterns
        if (match_patterns(path, option.exclude_devices))
        {
            continue;
        }

        // Allocate new device item for device list
        device_t *device = g_new0(device_t, 1);
        if (device == NULL)
        {
            continue;
        }

        // Fill in device information
        device->path = g_strdup(path);
        device->tid = "";
        device->uptime = uptime;
        device->driver = "";
        device->description = "";

        // Add device information to device list
        device_list = g_list_append(device_list, device);
    }

    if (g_list_length(device_list) == 0)
    {
        // Return NULL if no serial devices found
        return NULL;
    }

    // Sort device list device with respect to uptime
    device_list = g_list_sort(device_list, compare_uptime);

    closedir(dir);

    return device_list;
}

#endif

void list_serial_devices(void)
{
    tty_search_for_serial_devices();

    if (g_list_length(device_list) > 0)
    {
        printf("Device            TID     Uptime [s] Driver           Description\n");
        printf("----------------- ---- ------------- ---------------- --------------------------\n");

        // Iterate through the device list
        GList *iter;
        for (iter = device_list; iter != NULL; iter = g_list_next(iter))
        {
            device_t *device = (device_t *) iter->data;

            // Print device information
            printf("%-17s %4s %13.3f %-16s %s\n", device->path, device->tid, device->uptime, device->driver, device->description);
        }
        printf("\n");
    }

    list_serial_devices_by_id();
    list_serial_devices_by_path();
}

void tty_search(void)
{
    GList *iter;
    device_t *device = NULL;
    double uptime_minimum = 0;
    bool no_new = true;

    switch (option.auto_connect)
    {
        case AUTO_CONNECT_NEW:
            tty_search_for_serial_devices();

            // Save smallest uptime
            if (g_list_length(device_list) > 0)
            {
                // Get latest registered device (smallest uptime)
                GList *last = g_list_last(device_list);
                device = last->data;
                uptime_minimum = device->uptime;
            }

            tio_printf("Waiting for tty device..");

            while (no_new)
            {
                tty_search_for_serial_devices();

                // Iterate through the device list generated by search
                for (iter = device_list; iter != NULL; iter = g_list_next(iter))
                {
                    device = (device_t *) iter->data;

                    // Find first new device
                    if (device->uptime < uptime_minimum)
                    {
                        // Match found -> update device
                        device_name = device->path;
                        no_new = false;
                        break;
                    }
                }
                if (no_new)
                {
                    usleep(500*1000); // Sleep 0.5 s
                }
            }
            return;

        case AUTO_CONNECT_LATEST:
            tty_search_for_serial_devices();
            if (g_list_length(device_list) > 0)
            {
                // Get latest registered device (smallest uptime)
                GList *last = g_list_last(device_list);
                device = last->data;
                device_name = device->path;
            }
            return;

        case AUTO_CONNECT_DIRECT:
            if (config.device != NULL)
            {
                // Prioritize any device result of the configuration file first
                // Meaning a pattern or section/group have been matched the cmdline target.
                device_name = config.device;
            }
            else
            {
                // Fallback to use the target direcly
                device_name = option.target;
            }

            if (strlen(device_name) == TOPOLOGY_ID_SIZE)
            {
                // Potential topology ID detected -> trigger device search
                tty_search_for_serial_devices();

                // Iterate through the device list generated by search
                for (iter = device_list; iter != NULL; iter = g_list_next(iter))
                {
                    device = (device_t *) iter->data;

                    if (strcmp(device->tid, device_name) == 0)
                    {
                        // Topology ID match found -> use corresponding device name
                        device_name = device->path;

                        return;
                    }
                }
            }
            break;

        default:
            // Should never be reached
            tio_printf("Unknown connection strategy");
            exit(EXIT_FAILURE);
    }
}

void tty_wait_for_device(void)
{
    fd_set rdfs;
    int    status;
    int    maxfd;
    struct timeval tv;
    static char input_char;
    static bool first = true;
    static int last_errno = 0;

    /* Loop until device pops up */
    while (true)
    {
        tty_search();

        if (interactive_mode)
        {
            /* In interactive mode, while waiting for tty device, we need to
             * read from stdin to react on input key commands. */
            if (first)
            {
                /* Don't wait first time */
                tv.tv_sec = 0;
                tv.tv_usec = 1;
                first = false;
            }
            else
            {
                /* Wait up to 1 second for input */
                tv.tv_sec = 1;
                tv.tv_usec = 0;
            }

            FD_ZERO(&rdfs);
            FD_SET(pipefd[0], &rdfs);
            maxfd = MAX(pipefd[0], socket_add_fds(&rdfs, false));

            /* Block until input becomes available or timeout */
            status = select(maxfd + 1, &rdfs, NULL, NULL, &tv);
            if (status > 0)
            {
                if (FD_ISSET(pipefd[0], &rdfs))
                {
                    /* Input from stdin ready */

                    /* Read one character */
                    status = read(pipefd[0], &input_char, 1);
                    if (status <= 0)
                    {
                        tio_error_printf("Could not read from stdin");
                        exit(EXIT_FAILURE);
                    }

                    /* Handle commands */
                    handle_command_sequence(input_char, NULL, NULL);
                }
                socket_handle_input(&rdfs, NULL);
            }
            else if (status == -1)
            {
                tio_error_printf("select() failed (%s)", strerror(errno));
                exit(EXIT_FAILURE);
            }
        }

        /* Test for accessible device file */
        status = access(device_name, R_OK);
        if (status == 0)
        {
            last_errno = 0;
            return;
        }
        else if (last_errno != errno)
        {
            tio_warning_printf("Could not open tty device (%s)", strerror(errno));
            tio_printf("Waiting for tty device..");
            last_errno = errno;
        }

        if (!interactive_mode)
        {
            /* In non-interactive mode we do not need to handle input key
             * commands so we simply sleep 1 second between checking for
             * presence of tty device */
            sleep(1);
        }
    }
}

void tty_disconnect(void)
{
    if (connected)
    {
        tio_printf("Disconnected");
        flock(device_fd, LOCK_UN);
        close(device_fd);
        connected = false;

        /* Fire alert action */
        alert_disconnect();
    }
}

void tty_restore(void)
{
    tcsetattr(device_fd, TCSANOW, &tio_old);

    if (option.rs485)
    {
        /* Restore original RS-485 mode */
        rs485_mode_restore(device_fd);
    }

    if (connected)
    {
        tty_disconnect();
    }
}

void forward_to_tty(int fd, char output_char)
{
    int status;

    /* Map output character */
    if ((output_char == 127) && (option.map_o_del_bs))
    {
        output_char = '\b';
    }
    if ((output_char == '\r') && (option.map_o_cr_nl))
    {
        output_char = '\n';
    }

    /* Map newline character */
    if ((output_char == '\n' || output_char == '\r') && (option.map_o_nl_crnl))
    {
        const char *crlf = "\r\n";

        optional_local_echo(crlf[0]);
        optional_local_echo(crlf[1]);
        status = tty_write(fd, crlf, 2);
        if (status < 0)
        {
            tio_warning_printf("Could not write to tty device");
        }

        tx_total += 2;
    }
    else
    {
        switch (option.output_mode)
        {
            case OUTPUT_MODE_NORMAL:
                if (option.input_mode == INPUT_MODE_HEX)
                {
                    handle_hex_prompt(output_char);
                }
                else
                {
                    /* Send output to tty device */
                    if (option.input_mode != INPUT_MODE_LINE)
                    {
                        optional_local_echo(output_char);
                    }

                    if ((output_char == 0) && (option.map_o_nulbrk))
                    {
                        status = tcsendbreak(fd, 0);
                    }
                    else
                    {
                        status = tty_write(fd, &output_char, 1);
                    }

                    if (status < 0)
                    {
                        tio_warning_printf("Could not write to tty device");
                    }

                    /* Update transmit statistics */
                    tx_total++;
                }
                break;

            case OUTPUT_MODE_HEX:
                if (option.input_mode == INPUT_MODE_HEX)
                {
                    handle_hex_prompt(output_char);
                }
                else
                {
                    if (option.input_mode == INPUT_MODE_NORMAL)
                    {
                        optional_local_echo(output_char);
                    }
                }
                break;

            case OUTPUT_MODE_END:
                break;
        }
    }
}

static void print_line(const char *string, int cursor_pos)
{
    clear_line();
    print("%s", string);
    print("\r"); // Move the cursor back to the beginning
    for (int i = 0; i < cursor_pos; ++i)
    {
        print("\x1b[C"); // Move the cursor right
    }
}

int tty_connect(void)
{
    fd_set rdfs;           /* Read file descriptor set */
    int    maxfd;          /* Maximum file descriptor used */
    char   input_char, output_char;
    char   input_buffer[BUFSIZ] = {};
    static bool first = true;
    int    status;
    bool   do_timestamp = false;
    char*  now = NULL;
    struct timeval tval_before = {}, tval_now, tval_result;

    /* Open tty device */
    device_fd = open(device_name, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (device_fd < 0)
    {
        tio_error_printf_silent("Could not open tty device (%s)", strerror(errno));
        goto error_open;
    }

    /* Make sure device is of tty type */
    if (!isatty(device_fd))
    {
        tio_error_printf("Not a tty device");
        exit(EXIT_FAILURE);;
    }

    /* Lock device file */
    status = flock(device_fd, LOCK_EX | LOCK_NB);
    if ((status == -1) && (errno == EWOULDBLOCK))
    {
        tio_error_printf("Device file is locked by another process");
        exit(EXIT_FAILURE);
    }

    /* Flush stale I/O data (if any) */
    tcflush(device_fd, TCIOFLUSH);

    /* Print connect status */
    tio_printf("Connected to %s", device_name);
    connected = true;
    print_tainted = false;

    /* Fire alert action */
    alert_connect();

    if (option.timestamp)
    {
        do_timestamp = true;
    }

    /* Manage print output mode */
    tty_output_mode_set(option.output_mode);

    /* Save current port settings */
    if (tcgetattr(device_fd, &tio_old) < 0)
    {
        tio_error_printf_silent("Could not get port settings (%s)", strerror(errno));
        goto error_tcgetattr;
    }

#ifdef HAVE_IOSSIOSPEED
    if (!standard_baudrate)
    {
        /* OS X wants these fields left alone before setting arbitrary baud rate */
        tio.c_ispeed = tio_old.c_ispeed;
        tio.c_ospeed = tio_old.c_ospeed;
    }
#endif

    /* Manage RS-485 mode */
    if (option.rs485)
    {
        rs485_mode_enable(device_fd);
    }

    /* Make sure we restore tty settings on exit */
    if (first)
    {
        atexit(&tty_restore);
        first = false;
    }

    /* Activate new port settings */
    status = tcsetattr(device_fd, TCSANOW, &tio);
    if (status == -1)
    {
        tio_error_printf_silent("Could not apply port settings (%s)", strerror(errno));
        goto error_tcsetattr;
    }

    /* Set arbitrary baudrate (only works on supported platforms) */
    if (!standard_baudrate)
    {
        if (setspeed(device_fd, option.baudrate) != 0)
        {
            tio_error_printf_silent("Could not set baudrate speed (%s)", strerror(errno));
            goto error_setspeed;
        }
    }

    /* If stdin is a pipe forward all input to tty device */
    if (interactive_mode == false)
    {
        while (true)
        {
            int ret = read(pipefd[0], &input_char, 1);
            if (ret < 0)
            {
                tio_error_printf("Could not read from pipe (%s)", strerror(errno));
                exit(EXIT_FAILURE);
            }
            else if (ret > 0)
            {
                // Forward to tty device
                ret = write(device_fd, &input_char, 1);
                if (ret < 0)
                {
                    tio_error_printf("Could not write to serial device (%s)", strerror(errno));
                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                // EOF - finished forwarding
                break;
            }
        }
    }

    /* Manage script activation */
    if (option.script_run != SCRIPT_RUN_NEVER)
    {
        script_run(device_fd, NULL);

        if (option.script_run == SCRIPT_RUN_ONCE)
        {
            option.script_run = SCRIPT_RUN_NEVER;
        }
    }

    // Exit if piped input
    if (interactive_mode == false)
    {
        exit(EXIT_SUCCESS);
    }

    if (option.exec != NULL)
    {
        status = execute_shell_command(device_fd, option.exec);
        exit(status);
    }

    // Initialize readline like history
    char *history[MAX_HISTORY];
    int history_count = 0;
    int history_index = 0;

    for (int i = 0; i < MAX_HISTORY; ++i)
    {
        history[i] = NULL;
    }

    int line_length = 0;
    int cursor_pos = 0;
    int escape = 0;

    /* Input loop */
    while (true)
    {
        FD_ZERO(&rdfs);
        FD_SET(device_fd, &rdfs);
        FD_SET(pipefd[0], &rdfs);

        maxfd = MAX(device_fd, pipefd[0]);
        maxfd = MAX(maxfd, socket_add_fds(&rdfs, true));

        /* Block until input becomes available */
        status = select(maxfd + 1, &rdfs, NULL, NULL, NULL);
        if (status > 0)
        {
            bool forward = false;
            if (FD_ISSET(device_fd, &rdfs))
            {
                /*******************************/
                /* Input from tty device ready */
                /*******************************/

                ssize_t bytes_read = read(device_fd, input_buffer, BUFSIZ);
                if (bytes_read <= 0)
                {
                    /* Error reading - device is likely unplugged */
                    tio_error_printf_silent("Could not read from tty device");
                    goto error_read;
                }

                /* Update receive statistics */
                rx_total += bytes_read;

                // Manage timeout based timestamping in hex mode
                if ((option.output_mode == OUTPUT_MODE_HEX) && (option.hex_n_value == 0))
                {
                    if (option.timestamp != TIMESTAMP_NONE)
                    {
                        gettimeofday(&tval_now, NULL);
                        timersub(&tval_now, &tval_before, &tval_result);
                        if ((tval_result.tv_sec * 1000 + tval_result.tv_usec / 1000) > option.timestamp_timeout)
                        {
                            now = timestamp_current_time();
                            if (now)
                            {
                                ansi_printf_raw("\r\n[%s] ", now);
                                if (option.log)
                                {
                                    log_printf("\r\n[%s] ", now);
                                }
                                do_timestamp = false;
                            }
                        }
                        tval_before = tval_now;
                    }
                }

                /* Process input byte by byte */
                for (int i=0; i<bytes_read; i++)
                {
                    static unsigned long count = 0;

                    input_char = input_buffer[i];

                    /* Handle timestamps */
                    switch (option.output_mode)
                    {
                        case OUTPUT_MODE_NORMAL:
                            // Support timestamp per line
                            if ((do_timestamp && input_char != '\n' && input_char != '\r'))
                            {
                                now = timestamp_current_time();
                                if (now)
                                {
                                    ansi_printf_raw("[%s] ", now);
                                    if (option.log)
                                    {
                                        log_printf("[%s] ", now);
                                    }
                                    do_timestamp = false;
                                }
                            }
                            break;

                        case OUTPUT_MODE_HEX:
                            // Support hexN mode
                            if (option.hex_n_value > 0)
                            {
                                static bool first_ = true;
                                if ((count % option.hex_n_value) == 0)
                                {
                                    if (option.timestamp != TIMESTAMP_NONE)
                                    {
                                        now = timestamp_current_time();
                                        if (first_)
                                        {
                                            ansi_printf_raw("[%s] ", now);
                                            if (option.log)
                                            {
                                                log_printf("[%s] ", now);
                                            }
                                            first_ = false;
                                        }
                                        else
                                        {
                                            ansi_printf_raw("\r\n[%s] ", now);
                                            if (option.log)
                                            {
                                                log_printf("\n[%s] ", now);
                                            }
                                        }
                                    }
                                    else
                                    {
                                        if (first_)
                                        {
                                            // Do nothing
                                            first_ = false;
                                        }
                                        else
                                        {
                                            putchar('\r');
                                            putchar('\n');

                                            if (option.log)
                                            {
                                                log_putc('\n');
                                            }
                                        }
                                    }
                                }
                            }
                            count++;
                        break;

                        default:
                            tio_error_printf("Unknown outut mode");
                            exit(EXIT_FAILURE);
                        break;
                    }

                    /* Convert MSB to LSB bit order */
                    if (option.map_o_msblsb)
                    {
                        char ch = input_char;
                        input_char = 0;
                        for (int j = 0; j < 8; ++j)
                        {
                            input_char |= ((1 << j) & ch) ? (1 << (7 - j)) : 0;
                        }
                    }

                    /* Map input character */
                    if ((input_char == '\n') && (option.map_i_nl_crnl) && (!option.map_o_msblsb))
                    {
                        printchar('\r');
                        printchar('\n');
                        if (option.timestamp)
                        {
                            do_timestamp = true;
                        }
                    }
                    else if ((input_char == '\f') && (option.map_i_ff_escc) && (!option.map_o_msblsb))
                    {
                        printchar('\e');
                        printchar('c');
                    }
                    else
                    {
                        /* Print received tty character to stdout */
                        printchar(input_char);
                    }

                    /* Write to log */
                    if (option.log)
                    {
                        log_putc(input_char);
                    }

                    socket_write(input_char);

                    print_tainted = true;

                    if (input_char == '\n' && option.timestamp)
                    {
                        do_timestamp = true;
                    }
                }
            }
            else if (FD_ISSET(pipefd[0], &rdfs))
            {
                /**************************/
                /* Input from stdin ready */
                /**************************/

                ssize_t bytes_read = read(pipefd[0], input_buffer, BUFSIZ);
                if (bytes_read < 0)
                {
                    tio_error_printf_silent("Could not read from stdin (%s)", strerror(errno));
                    goto error_read;
                }
                else if (bytes_read == 0)
                {
                    /* Reached EOF (when piping to stdin, never reached) */
                    tty_sync(device_fd);
                    exit(EXIT_SUCCESS);
                }

                /* Process input byte by byte */
                for (int i=0; i<bytes_read; i++)
                {
                    input_char = input_buffer[i];

                    /* Forward input to output */
                    output_char = input_char;
                    forward = true;

                    if (interactive_mode)
                    {
                        /* Do not forward prefix key */
                        if (option.prefix_enabled && input_char == option.prefix_code)
                        {
                            forward = false;
                        }

                        /* Handle commands */
                        handle_command_sequence(input_char, &output_char, &forward);

                        if (forward)
                        {
                            switch (option.input_mode)
                            {
                                case INPUT_MODE_HEX:
                                    if (!is_valid_hex(input_char))
                                    {
                                        tio_warning_printf("Invalid hex character: '%d' (0x%02x)", input_char, input_char);
                                        forward = false;
                                    }
                                    break;

                                case INPUT_MODE_LINE:
                                    switch (input_char)
                                    {
                                        case '\r': // Carriage return
                                            if (line_length > 0)
                                            {
                                                // Save to history
                                                if (history_count < MAX_HISTORY)
                                                {
                                                    history[history_count] = strndup(line, line_length);
                                                    history_count++;
                                                }
                                                else
                                            {
                                                    free(history[0]);
                                                    memmove(&history[0], &history[1], (MAX_HISTORY - 1) * sizeof(char*));
                                                    history[MAX_HISTORY - 1] = strndup(line, line_length);
                                                }
                                            }

                                            line[line_length] = '\0';
                                            if (option.local_echo == false)
                                            {
                                                clear_line();
                                            }
                                            else
                                            {
                                                print("\r\n");
                                            }

                                            // Write line to tty device
                                            tty_write(device_fd, line, line_length);
                                            tty_sync(device_fd);

                                            line_length = 0;
                                            cursor_pos = 0;
                                            history_index = history_count;
                                            escape = 0;
                                            break;

                                        case 127: // Backspace
                                            if (cursor_pos > 0)
                                            {
                                                memmove(&line[cursor_pos - 1], &line[cursor_pos], line_length - cursor_pos);
                                                line_length--;
                                                cursor_pos--;
                                                line[line_length] = '\0';
                                                print_line(line, cursor_pos);
                                            }
                                            forward = false;
                                            escape = 0;
                                            break;

                                        case 27: // Escape
                                            escape = 1;
                                            forward = false;
                                            break;

                                        case '[':
                                            if (escape == 1)
                                            {
                                                escape = 2;
                                            }
                                            else
                                            {
                                                escape = 0;
                                            }
                                            break;

                                        case 'A':
                                            if (escape == 2)
                                            {
                                                // Up arrow
                                                if (history_index > 0)
                                                {
                                                    history_index--;
                                                    strncpy(line, history[history_index], MAX_LINE_LEN-1);
                                                    line_length = strlen(line);
                                                    cursor_pos = line_length;
                                                    print_line(line, cursor_pos);
                                                }
                                            }
                                            forward = false;
                                            escape = 0;
                                            break;

                                        case 'B':
                                            if (escape == 2)
                                            {
                                                // Down arrow
                                                if (history_index < history_count - 1)
                                                {
                                                    history_index++;
                                                    strncpy(line, history[history_index], MAX_LINE_LEN-1);
                                                    line_length = strlen(line);
                                                    cursor_pos = line_length;
                                                    print_line(line, cursor_pos);
                                                }
                                                else if (history_index == history_count - 1)
                                                {
                                                    history_index++;
                                                    line_length = 0;
                                                    cursor_pos = 0;
                                                    line[line_length] = '\0';
                                                    print_line(line, cursor_pos);
                                                }
                                            }
                                            forward = false;
                                            escape = 0;
                                            break;

                                        case 'C':
                                            if (escape == 2)
                                            {
                                                // Right arrow
                                                if (cursor_pos < line_length)
                                                {
                                                    cursor_pos++;
                                                    print("\x1b[C");
                                                }
                                            }
                                            forward = false;
                                            escape = 0;
                                            break;

                                        case 'D':
                                            if (escape == 2)
                                            {
                                                    // Left arrow
                                                    if (cursor_pos > 0)
                                                    {
                                                        cursor_pos--;
                                                        print("\b");
                                                    }
                                            }
                                            forward = false;
                                            escape = 0;
                                            break;

                                        default:
                                            if (line_length < MAX_LINE_LEN - 1)
                                            {
                                                memmove(&line[cursor_pos + 1], &line[cursor_pos], line_length - cursor_pos);
                                                line[cursor_pos] = input_char;
                                                line_length++;
                                                cursor_pos++;
                                                line[line_length] = '\0';
                                                print_line(line, cursor_pos);
                                            }
                                            escape = 0;
                                            break;
                                    }

                                default:
                                    break;
                            }
                        }
                    }

                    if (forward)
                    {
                        forward_to_tty(device_fd, output_char);
                    }
                }

                tty_sync(device_fd);
            }
            else
            {
                /***************************/
                /* Input from socket ready */
                /***************************/

                forward = socket_handle_input(&rdfs, &output_char);

                if (forward)
                {
                    forward_to_tty(device_fd, output_char);
                }

                tty_sync(device_fd);
            }
        }
        else if (status == -1)
        {
#if defined(__CYGWIN__)
            // Happens when port unpluged
            if (errno == EACCES)
            {
                goto error_read;
            }
#elif defined(__APPLE__)
            if (errno == EBADF)
            {
                break; // tty_disconnect() will be naturally triggered by atexit()
            }
#else
            tio_error_printf("select() failed (%s)", strerror(errno));
            exit(EXIT_FAILURE);
#endif
        }
        else
        {
            // Timeout (only happens in response wait mode)
            exit(EXIT_FAILURE);
        }
    }

    return TIO_SUCCESS;

error_setspeed:
error_tcsetattr:
error_tcgetattr:
error_read:
    tty_disconnect();
error_open:
    return TIO_ERROR;
}

