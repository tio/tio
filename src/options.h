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
    char *target;
    int baudrate;
    int databits;
    flow_t flow;
    int stopbits;
    parity_t parity;
    int output_delay;
    int output_line_delay;
    int dtr_pulse_duration;
    int rts_pulse_duration;
    int cts_pulse_duration;
    int dsr_pulse_duration;
    int dcd_pulse_duration;
    int ri_pulse_duration;
    bool no_reconnect;
    auto_connect_t auto_connect;
    bool log;
    bool log_append;
    bool log_strip;
    bool local_echo;
    timestamp_t timestamp;
    char *log_filename;
    char *log_directory;
    char *map;
    char *socket;
    int color;
    input_mode_t input_mode;
    output_mode_t output_mode;
    char prefix_code;
    char prefix_key;
    bool prefix_enabled;
    bool mute;
    bool rs485;
    uint32_t rs485_config_flags;
    int32_t rs485_delay_rts_before_send;
    int32_t rs485_delay_rts_after_send;
    alert_t alert;
    bool complete_profiles;
    char *script;
    char *script_filename;
    script_run_t script_run;
    int timestamp_timeout;
    char *exclude_devices;
    char *exclude_drivers;
    char *exclude_tids;
    int hex_n_value;
    bool vt100;
    char *exec;
};

extern struct option_t option;

void options_print();
void options_parse(int argc, char *argv[]);
void options_parse_final(int argc, char *argv[]);

int option_string_to_integer(const char *string, int *value, const char *desc, int min, int max);

void option_parse_flow(const char *arg, flow_t *flow);
void option_parse_parity(const char *arg, parity_t *parity);

void option_parse_output_mode(const char *arg, output_mode_t *mode);
void option_parse_input_mode(const char *arg, input_mode_t *mode);

void option_parse_line_pulse_duration(const char *arg);
void option_parse_script_run(const char *arg, script_run_t *script_run);
void option_parse_alert(const char *arg, alert_t *alert);

void option_parse_auto_connect(const char *arg, auto_connect_t *auto_connect);
const char *option_auto_connect_state_to_string(auto_connect_t strategy);

void option_parse_timestamp(const char *arg, timestamp_t *timestamp);
const char* option_timestamp_format_to_string(timestamp_t timestamp);
