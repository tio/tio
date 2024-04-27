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

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <termios.h>
#include <sys/param.h>
#include "script.h"
#include "timestamp.h"
#include "alert.h"
#include "tty.h"

typedef enum
{
    INPUT_MODE_NORMAL,
    INPUT_MODE_HEX,
    INPUT_MODE_LINE,
    INPUT_MODE_END,
} input_mode_t;

typedef enum
{
    OUTPUT_MODE_NORMAL,
    OUTPUT_MODE_HEX,
    OUTPUT_MODE_END,
} output_mode_t;

/* Options */
struct option_t
{
    const char *target;
    unsigned int baudrate;
    int databits;
    char *flow;
    int stopbits;
    char *parity;
    int output_delay;
    int output_line_delay;
    unsigned int dtr_pulse_duration;
    unsigned int rts_pulse_duration;
    unsigned int cts_pulse_duration;
    unsigned int dsr_pulse_duration;
    unsigned int dcd_pulse_duration;
    unsigned int ri_pulse_duration;
    bool no_reconnect;
    auto_connect_t auto_connect;
    bool log;
    bool log_append;
    bool log_strip;
    bool local_echo;
    timestamp_t timestamp;
    const char *log_filename;
    const char *log_directory;
    const char *map;
    const char *socket;
    int color;
    input_mode_t input_mode;
    output_mode_t output_mode;
    unsigned char prefix_code;
    unsigned char prefix_key;
    bool prefix_enabled;
    bool mute;
    bool rs485;
    uint32_t rs485_config_flags;
    int32_t rs485_delay_rts_before_send;
    int32_t rs485_delay_rts_after_send;
    alert_t alert;
    bool complete_profiles;
    const char *script;
    const char *script_filename;
    script_run_t script_run;
    unsigned int timestamp_timeout;
    const char *exclude_devices;
    const char *exclude_drivers;
    const char *exclude_tids;
};

extern struct option_t option;

void options_print();
void options_parse(int argc, char *argv[]);
void options_parse_final(int argc, char *argv[]);

void line_pulse_duration_option_parse(const char *arg);
script_run_t script_run_option_parse(const char *arg);

input_mode_t input_mode_option_parse(const char *arg);
output_mode_t output_mode_option_parse(const char *arg);
auto_connect_t auto_connect_option_parse(const char *arg);
const char *auto_connect_state_to_string(auto_connect_t strategy);
