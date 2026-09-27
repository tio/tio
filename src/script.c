/*
 * tio - a serial device I/O tool
 *
 * Copyright (c) 2014-2024  Martin Lund
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <lauxlib.h>
#include <lualib.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <assert.h>
#include "misc.h"
#include "print.h"
#include "options.h"
#include "tty.h"
#include "xymodem.h"
#include "log.h"
#include "script.h"
#include "fs.h"
#include "timestamp.h"
#include "termios.h"
#include "version.h"

#define MAX_BUFFER_SIZE 2000 // Maximum size of circular buffer
#define READ_LINE_SIZE 4096 // read_line buffer length

static int device_fd = 0;
static lua_State *script_interp = NULL;

// clang-format off
static char script_init[] =
"table.unpack = table.unpack or unpack\n"
"tio.C = {\n"
"    EXPECT_CLEANUP_READ_SIZE = 4096,\n"
"    WAIT_FOREVER = 0,\n"
"    NOWAIT = -1,\n"
"}\n"
"tio.clear_screen = function()\n"
"    io.write(string.format('%cc',0x1b))\n"
"end\n"
"tio.set = function(arg)\n"
"    local dtr = arg.DTR or -1\n"
"    local rts = arg.RTS or -1\n"
"    local cts = arg.CTS or -1\n"
"    local dsr = arg.DSR or -1\n"
"    local cd = arg.CD or -1\n"
"    local ri = arg.RI or -1\n"
"    tio.line_set(dtr, rts, cts, dsr, cd, ri)\n"
"end\n"
"tio.expect = function(pattern, timeout)\n"
"    local str = ''\n"
"    while true do\n"
"        local astr = tio.read(tio.C.EXPECT_CLEANUP_READ_SIZE, tio.C.NOWAIT)\n"
"        local c = nil\n"
"        if astr == nil then\n"
"            c = tio.read(1, timeout)\n"
"            if c == nil then\n"
"                return nil, str\n"
"            end\n"
"        end\n"
"        str = table.concat{str, astr or '', c or ''}\n"
"        local captured = { string.match(str, pattern) }\n"
"        if #captured > 0 then\n"
"            return table.unpack(captured), str\n"
"        end\n"
"    end\n"
"end\n"
"tio.expects = function(patterns, timeout)\n"
"    local str = ''\n"
"    if type(patterns) ~= 'table' then\n"
"        patterns = { patterns }\n"
"    end\n"
"    while true do\n"
"        local astr = tio.read(tio.C.EXPECT_CLEANUP_READ_SIZE, tio.C.NOWAIT)\n"
"        local c = nil\n"
"        if astr == nil then\n"
"            c = tio.read(1, timeout)\n"
"            if c == nil then\n"
"                return nil, nil, str\n"
"            end\n"
"        end\n"
"        str = table.concat{str, astr or '', c or ''}\n"
"        for idx, pat in ipairs(patterns) do\n"
"            local captured = { string.match(str, pat) }\n"
"            if #captured > 0 then\n"
"                return idx, captured, str\n"
"            end\n"
"        end\n"
"    end\n"
"end\n"
"tio.subcmd_println = function(fmt, ...)\n"
"    tio.subcmd_puts(string.format(fmt, select(1, ...)))\n"
"end\n"
"tio.subcmd_warning_println = function(fmt, ...)\n"
"    tio.subcmd_warning_puts(string.format(fmt, select(1, ...)))\n"
"end\n"
"tio.subcmd_error_println = function(fmt, ...)\n"
"    tio.subcmd_error_puts(string.format(fmt, select(1, ...)))\n"
"end\n"
"tio.alwaysecho = true\n"
"setmetatable(tio, tio)\n";
// clang-format on

static bool alwaysecho(lua_State *L)
{
    bool b;

    lua_getglobal(L, "tio");
    lua_getfield(L, -1, "alwaysecho");
    b = lua_toboolean(L, -1);
    lua_pop(L, 2);

    return b;
}

static int api_echo(lua_State *L)
{
    size_t len = 0;
    const char *str = luaL_checklstring(L, 1, &len);

    if (option.timestamp)
    {
        char *pTimeStampNow = timestamp_current_time();
        if (pTimeStampNow)
        {
            tio_printf("%s", str);
            if (option.log)
            {
                log_printf("\n[%s] %s", pTimeStampNow, str);
            }
        }
    }
    else
    {
        for (size_t i=0; i<len; i++)
        {
            putchar(str[i]);

            if (option.log)
                log_putc(str[i]);
        }
    }

    return 0;
}

static void maybe_echo(lua_State *L)
{
    if (alwaysecho(L))
    {
        lua_pushcfunction(L, api_echo);
        lua_pushvalue(L, -2);
        lua_call(L, 1, 0);
    }
}

// lua: tio.sleep(seconds)
static int api_sleep(lua_State *L)
{
    long seconds = lua_tointeger(L, 1);

    if (seconds < 0)
    {
        return 0;
    }

    tio_printf("Sleeping %ld seconds", seconds);

    sleep(seconds);

    return 0;
}

// lua: tio.msleep(miliseconds)
static int api_msleep(lua_State *L)
{
    long mseconds = lua_tointeger(L, 1);
    long useconds = mseconds * 1000;

    if (useconds < 0)
    {
        return 0;
    }

    tio_printf("Sleeping %ld ms", mseconds);
    usleep(useconds);

    return 0;
}

// lua: tio.line_set(dtr,rts,cts,dsr,cd,ri)
static int api_line_set(lua_State *L)
{
    tty_line_config_t line_config[6] = { };

    int dtr = lua_tointeger(L, 1);
    int rts = lua_tointeger(L, 2);
    int cts = lua_tointeger(L, 3);
    int dsr = lua_tointeger(L, 4);
    int cd = lua_tointeger(L, 5);
    int ri = lua_tointeger(L, 6);

    if (device_fd == 0)
    {
        return luaL_error(L, "tty device not ready");
    }

    if (dtr != -1)
    {
        line_config[0].mask = TIOCM_DTR;
        line_config[0].value = dtr;
        line_config[0].reserved = true;
    }
    if (rts != -1)
    {
        line_config[1].mask = TIOCM_RTS;
        line_config[1].value = rts;
        line_config[1].reserved = true;
    }
    if (cts != -1)
    {
        line_config[2].mask = TIOCM_CTS;
        line_config[2].value = cts;
        line_config[2].reserved = true;
    }
    if (dsr != -1)
    {
        line_config[3].mask = TIOCM_DSR;
        line_config[3].value = dsr;
        line_config[3].reserved = true;
    }
    if (cd != -1)
    {
        line_config[4].mask = TIOCM_CD;
        line_config[4].value = cd;
        line_config[4].reserved = true;
    }
    if (ri != -1)
    {
        line_config[5].mask = TIOCM_RI;
        line_config[5].value = ri;
        line_config[5].reserved = true;
    }

    tty_line_set(device_fd, line_config);

    return 0;
}

// lua: tio.line_get()
static int api_line_get(lua_State *L)
{
    int line_state;
    const int LN_LOW = 0;
    const int LN_HIGH = 1;

    if (device_fd == 0)
    {
        return luaL_error(L, "tty device not ready");
    }

    if (ioctl(device_fd, TIOCMGET, &line_state) < 0)
    {
        return luaL_error(L, "Could not get line state");
    }

    lua_pushinteger(L, (line_state & TIOCM_DTR) ? LN_LOW : LN_HIGH);
    lua_pushinteger(L, (line_state & TIOCM_RTS) ? LN_LOW : LN_HIGH);
    lua_pushinteger(L, (line_state & TIOCM_CTS) ? LN_LOW : LN_HIGH);
    lua_pushinteger(L, (line_state & TIOCM_DSR) ? LN_LOW : LN_HIGH);
    lua_pushinteger(L, (line_state & TIOCM_CD) ? LN_LOW : LN_HIGH);
    lua_pushinteger(L, (line_state & TIOCM_RI) ? LN_LOW : LN_HIGH);

    return 6;
}

// lua: tio.send(file, protocol)
static int api_send(lua_State *L)
{
    const char *file = luaL_checkstring(L, 1);
    int protocol = luaL_checkinteger(L, 2);
    int ret;

    if (device_fd == 0)
    {
        return luaL_error(L, "tty device not ready");
    }

    if (file == NULL)
    {
        return 0;
    }

    state_t state_orig = state;
    state = STATE_XYMODEM;
    tty_tcsetattr(device_fd);

    switch (protocol)
    {
        case XMODEM_1K:
            tio_printf("Sending file '%s' using XMODEM-1K", file);
            ret = xymodem_send(device_fd, file, XMODEM_1K);
            tio_printf("%s", ret < 0 ? "Aborted" : "Done");
            break;

        case XMODEM_CRC:
            tio_printf("Sending file '%s' using XMODEM-CRC", file);
            ret = xymodem_send(device_fd, file, XMODEM_CRC);
            tio_printf("%s", ret < 0 ? "Aborted" : "Done");
            break;

        case XMODEM_SUM:
            tio_printf("Sending file '%s' using XMODEM-SUM", file);
            ret = xymodem_send(device_fd, file, XMODEM_SUM);
            tio_printf("%s", ret < 0 ? "Aborted" : "Done");
            break;

        case YMODEM:
            tio_printf("Sending file '%s' using YMODEM", file);
            ret = xymodem_send(device_fd, file, YMODEM);
            tio_printf("%s", ret < 0 ? "Aborted" : "Done");
            break;
    }

    state = state_orig;
    tty_tcsetattr(device_fd);

    return 0;
}

// lua: tio.receive(file, protocol)
static int api_receive(lua_State *L)
{
    const char *file = luaL_checkstring(L, 1);
    int protocol = luaL_checkinteger(L, 2);
    int ret;

    if (device_fd == 0)
    {
        return luaL_error(L, "tty device not ready");
    }

    if (file == NULL)
    {
        return 0;
    }

    state_t state_orig = state;
    state = STATE_XYMODEM;
    tty_tcsetattr(device_fd);

    switch (protocol)
    {
        case XMODEM_CRC:
            tio_printf("Receiving file '%s' using XMODEM-CRC", file);
            ret = xymodem_receive(device_fd, file, XMODEM_CRC);
            tio_printf("%s", ret < 0 ? "Aborted" : "Done");
            break;

        case XMODEM_SUM:
            tio_printf("Receiving file '%s' using XMODEM-SUM", file);
            ret = xymodem_receive(device_fd, file, XMODEM_SUM);
            tio_printf("%s", ret < 0 ? "Aborted" : "Done");
            break;

        case XMODEM_1K:
        case YMODEM:
        default:
            tio_error_printf("Not supported");
            break;
    }

    state = state_orig;
    tty_tcsetattr(device_fd);

    return 0;
}

// lua: tio.write(string)
static int api_write(lua_State *L)
{
    size_t len = 0;
    const char *string = luaL_checklstring(L, 1, &len);
    ssize_t ret;
    int attempts = 100;

    if (device_fd == 0)
    {
        return luaL_error(L, "tty device not ready");
    }

    do
    {
        ret = write_poll(device_fd, string, len, POLL_FOREVER);
        if (ret < 0)
            return luaL_error(L, "%s", strerror(errno));

        len -= ret;
        string += ret;
    } while (len > 0 && --attempts);

    if (len > 0)
        return luaL_error(L, "partial write");

    fsync(device_fd);  // flush these characters now
    tcdrain(device_fd); //ensure we flushed characters to our device

    lua_getglobal(L, "tio");

    return 1;
}

// lua: tio.twrite(string)
static int api_twrite(lua_State *L)
{
    size_t len = 0;
    const char *string = luaL_checklstring(L, 1, &len);

    if (device_fd == 0)
    {
        return luaL_error(L, "tty device not ready");
    }

    for (; len > 0; --len, string++)
    {
        forward_to_tty(device_fd, *string);
    }
    tty_sync(device_fd);

    lua_getglobal(L, "tio");

    return 1;
}

// lua: tio.read(size, timeout)
static int api_read(lua_State *L)
{
    int size = luaL_checkinteger(L, 1);
    int timeout = luaL_optinteger(L, 2, 0); // ms, zero value means forever, negative value means nowait.

    if (device_fd == 0)
    {
        return luaL_error(L, "tty device not ready");
    }

    // For C API, the values for forever and nowait are swapped.
    int timeout_c;
    if (timeout > 0)
        timeout_c = timeout;
    else if (timeout == 0)
        timeout_c = POLL_FOREVER;
    else if (timeout < 0)
        timeout_c = POLL_NOWAIT;

    luaL_Buffer buffer;
    luaL_buffinit(L, &buffer);

#if LUA_VERSION_NUM >= 502
    char *p = luaL_prepbuffsize(&buffer, size);
#else
    if (size > LUAL_BUFFERSIZE)
        return luaL_error(L, "buffer overflow, max size is: %d", LUAL_BUFFERSIZE);
    char *p = luaL_prepbuffer(&buffer);
#endif

    ssize_t ret = read_poll(device_fd, p, size, timeout_c);
    if (ret < 0)
        return luaL_error(L, "%s", strerror(errno));

    luaL_addsize(&buffer, ret);
    luaL_pushresult(&buffer);

    if (ret == 0)
    {
        // On timeout return nil instead of an empty string
        lua_pop(L, 1);
        lua_pushnil(L);
    }
    else
    {
        maybe_echo(L);
    }

    return 1;
}

// lua: string = tio.readline(timeout)
static int api_readline(lua_State *L)
{
    int timeout = luaL_optinteger(L, 1, 0); // ms, zero value means forever, negative value means nowait.
    luaL_Buffer b;
    char ch;

    if (device_fd == 0)
    {
        return luaL_error(L, "tty device not ready");
    }

    // For C API, the values for forever and nowait are swapped.
    int timeout_c;
    if (timeout > 0)
        timeout_c = timeout;
    else if (timeout == 0)
        timeout_c = POLL_FOREVER;
    else if (timeout < 0)
        timeout_c = POLL_NOWAIT;

    luaL_buffinit(L, &b);
    luaL_prepbuffer(&b);
    while (true)
    {
        int ret = read_poll(device_fd, &ch, 1, timeout_c);

        if (ret < 0)
            return luaL_error(L, "%s", strerror(errno));

        if (ret == 0)
        {
            luaL_pushresult(&b);
            maybe_echo(L);
            lua_pushnil(L);
            lua_insert(L, -2);
            return 2;
        }

        if (ch == '\n')
        {
            luaL_pushresult(&b);
            maybe_echo(L);
            return 1;
        }

        luaL_addchar(&b, ch);
    }
}

// lua: str = tio.inkey(mseconds)
static int api_inkey(lua_State *L)
{
    extern char inkey_chars[];
    int ret;
    int mseconds;
    int arg_num = lua_gettop(L);
    int arg;
    if (arg_num == 0)
    {
        arg = -1;
    }
    else
    {
        arg = lua_tointeger(L, 1);
    }
    if (arg == 0)
    {
        mseconds = POLL_FOREVER;
    }
    else if (arg < 0)
    {
        mseconds = POLL_NOWAIT;
    }
    else
    {
        mseconds = arg;
    }
    ret = tty_inkey(mseconds);
    if (ret == 0)
    {
        /* Timeout */
        lua_pushnil(L);
        return 1;
    }
    else if (ret < 0) {
        return luaL_error(L, "inkey failed");
    }
    lua_pushlstring(L, inkey_chars, ret);
    return 1;
}

// lua: str = tio.input(prompt)
static int api_input(lua_State *L)
{
    extern char line[];
    int arg_num = lua_gettop(L);
    const char *prompt = "";
    if (arg_num > 0)
    {
        prompt = luaL_checkstring(L, 1);
    }
    tty_simple_readln(prompt);
    lua_pushstring(L, line);
    return 1;
}

// lua: str = tio.inputline(title_prompt)
static int api_input_line(lua_State *L)
{
    extern char line[];
    int arg_num = lua_gettop(L);
    const char *prompt = "";
    if (arg_num > 0)
    {
        prompt = luaL_checkstring(L, 1);
    }
    tty_subcmd_readln(prompt);
    lua_pushstring(L, line);
    return 1;
}

// lua: api_subcmd_puts(str)
static int api_subcmd_puts(lua_State *L)
{
    int arg_num = lua_gettop(L);
    const char *str;
    if (arg_num != 1)
    {
        return luaL_error(L, "arguments error");
    }
    str = luaL_checkstring(L, 1);
    tio_printf("%s", str);
    return 0;
}

// lua: api_subcmd_warning_puts(str)
static int api_subcmd_warning_puts(lua_State *L)
{
    int arg_num = lua_gettop(L);
    const char *str;
    if (arg_num != 1)
    {
        return luaL_error(L, "arguments error");
    }
    str = luaL_checkstring(L, 1);
    tio_warning_printf("%s", str);
    return 0;
}

// lua: api_subcmd_error_puts(str)
static int api_subcmd_error_puts(lua_State *L)
{
    int arg_num = lua_gettop(L);
    const char *str;
    if (arg_num != 1)
    {
        return luaL_error(L, "arguments error");
    }
    str = luaL_checkstring(L, 1);
    tio_error_printf("%s", str);
    return 0;
}

// lua: true/false, error = tio.set_keymap(keymap_str)
static int api_set_keymap(lua_State *L)
{
    int arg_num = lua_gettop(L);
    const char *keymap_str;
    if (arg_num != 1)
    {
        return luaL_error(L, "arguments error");
    }
    keymap_str = luaL_checkstring(L, 1);
#if 1
    option_parse_key_mappings(keymap_str);
    return 0;
#else
    ret = option_parse_key_mappings(keymap_str);
    if (ret < 0)
    {
        return luaL_error(L, "keymap setting failed");
    }
    lua_pushboolean(L, true);
    return 1;
#endif
}


// lua: table = tio.ttysearch()
static int api_ttysearch(lua_State *L)
{
    UNUSED(L);
    GList *iter;
    int i = 1;

    GList *device_list = tty_search_for_serial_devices();

    if (device_list == NULL)
    {
        return 0;
    }

    // Create a new table
    lua_newtable(L);

    // Iterate through found devices
    for (iter = device_list; iter != NULL; iter = g_list_next(iter))
    {
        device_t *device = (device_t *) iter->data;

        // Create a new sub-table for each serial device
        lua_newtable(L);

        // Add elements to the table
        lua_pushstring(L, "path");
        lua_pushstring(L, device->path);
        lua_settable(L, -3);

        lua_pushstring(L, "tid");
        lua_pushstring(L, device->tid);
        lua_settable(L, -3);

        lua_pushstring(L, "uptime");
        lua_pushnumber(L, device->uptime);
        lua_settable(L, -3);

        lua_pushstring(L, "driver");
        lua_pushstring(L, device->driver);
        lua_settable(L, -3);

        lua_pushstring(L, "description");
        lua_pushstring(L, device->description);
        lua_settable(L, -3);

        // Set the sub-table as a row in the main table
        lua_rawseti(L, -2, i++);
    }

    // Return table
    return 1;
}

// lua: tio.send_break()
static int api_send_break(lua_State *L)
{
    if (device_fd == 0)
    {
        return luaL_error(L, "tty device not ready");
    }

    tcsendbreak(device_fd, 0);
    return 0;
}

// lua: tio.set_local_echo(boolean local_echo)
static int api_set_local_echo(lua_State *L)
{
    int arg_num = lua_gettop(L);
    if (arg_num == 0)
    {
        option.local_echo = true;
        return 0;
    }
    if ( ! (lua_isboolean(L, 1) || lua_isnil(L, 1)) )
    {
        return luaL_error(L, "argument is not boolean");
    }
    option.local_echo = lua_toboolean(L, 1);
    return 0;
}

// lua: tio.set_log(boolean log)
static int api_set_log(lua_State *L)
{
    int arg_num = lua_gettop(L);
    if (arg_num == 0)
    {
        option.log = true;
    }
    else /* arg_num > 0 */
    {
        if ( ! (lua_isboolean(L, 1) || lua_isnil(L, 1)) )
        {
            return luaL_error(L, "argument is not boolean");
        }
        option.log = lua_toboolean(L, 1);
    }

    if (option.log)
    {
        if (log_open(option.log_filename) != 0)
        {
            option.log = false;
            return luaL_error(L, "cant open log file");
        }
    }
    else
    {
        log_close();
    }
    return 0;
}

// lua: tio.flush_data_io_buffer()
static int api_flush_data_io_buffer(lua_State *L)
{
    if (device_fd == 0)
    {
        return luaL_error(L, "tty device not ready");
    }
    tcflush(device_fd, TCIOFLUSH);
    return 0;
}

// lua: tio.set_input_mode(tio.C.IM_NORMAL | tio.C.IM_HEX | tio.C.IM_LINE)
static int api_set_input_mode(lua_State *L)
{
    int input_mode = luaL_optinteger(L, 1, INPUT_MODE_NORMAL);
    switch (input_mode)
    {
        case INPUT_MODE_NORMAL:
        case INPUT_MODE_HEX:
        case INPUT_MODE_LINE:
            break;
        default:
            return luaL_error(L, "invalid input mode");
    }
    option.input_mode = input_mode;
    return 0;
}

// lua: tio.set_output_mode(tio.C.OM_NORMAL | tio.C.OM_HEX)
static int api_set_output_mode(lua_State *L)
{
    int output_mode = luaL_optinteger(L, 1, OUTPUT_MODE_NORMAL);
    switch (output_mode)
    {
        case OUTPUT_MODE_NORMAL:
        case OUTPUT_MODE_HEX:
            break;
        default:
            return luaL_error(L, "invalid output mode");
    }
    option.output_mode = output_mode;
    return 0;
}

// lua: tio.set_raw_mode(tio.C.RAW_OFF | tio.C.RAW_ON | tio.C.RAW_ON_NODELAY)
static int api_set_raw_mode(lua_State *L)
{
    int raw_mode = luaL_optinteger(L, 1, RAW_ON_DELAY);
    switch (raw_mode)
    {
        case RAW_OFF:
        case RAW_ON_DELAY:
        case RAW_ON_NODELAY:
            break;
        default:
            return luaL_error(L, "invalid raw mode");
    }
    option.raw = raw_mode;
    if (state != STATE_INTERACTIVE)
    {
        tty_tcsetattr(device_fd);
    }
    return 0;
}

// lua: tio.set_raw_mode_interactive(tio.C.RAW_OFF | tio.C.RAW_ON | tio.C.RAW_ON_NODELAY)
static int api_set_raw_mode_interactive(lua_State *L)
{
    int raw_mode = luaL_optinteger(L, 1, RAW_ON_DELAY);
    switch (raw_mode)
    {
        case RAW_OFF:
        case RAW_ON_DELAY:
        case RAW_ON_NODELAY:
            break;
        default:
            return luaL_error(L, "invalid raw mode");
    }
    option.raw_interactive = raw_mode;
    if (state == STATE_INTERACTIVE)
    {
        tty_tcsetattr(device_fd);
    }
    return 0;
}

// lua: tio.set_timestamp_mode(tio.C.TS_NONE | tio.C.TS_24HOUR | ...)
static int api_set_timestamp_mode(lua_State *L)
{
    int timestamp_mode = luaL_optinteger(L, 1, TIMESTAMP_24HOUR);
    switch (timestamp_mode)
    {
        case TIMESTAMP_NONE:
        case TIMESTAMP_24HOUR:
        case TIMESTAMP_24HOUR_START:
        case TIMESTAMP_24HOUR_DELTA:
        case TIMESTAMP_ISO8601:
        case TIMESTAMP_EPOCH:
        case TIMESTAMP_EPOCH_USEC:
            break;
        default:
            return luaL_error(L, "invalid timestamp mode");
    }
    option.timestamp = timestamp_mode;
    return 0;
}

// lua: tio.exec_shell_command(string:command)
int api_exec_shell_command(lua_State *L)
{
    const char *command = luaL_checkstring(L, 1);
    if (command == NULL)
    {
        return luaL_error(L, "no command");
    }
    if (device_fd == 0)
    {
        return luaL_error(L, "tty device not ready");
    }
    int result;
    state_t state_orig = state;
    state = STATE_EXEC_SHELL_COMMAND;
    tty_tcsetattr(device_fd);
    result = execute_shell_command(device_fd, command);
    state = state_orig;
    tty_tcsetattr(device_fd);
    if (result < 0)
    {
        return luaL_error(L, "command failed.");
    }
    return 0;
}

// lua: tio.get_state()
static int api_get_state(lua_State *L)
{
    lua_pushinteger(L, state);
    return 1;
}

// lua: tio.get_version()
static int api_get_version(lua_State *L)
{
    lua_pushstring(L, VERSION);
    return 1;
}

static void script_buffer_run(lua_State *L, const char *script_buffer)
{
    int error;

    error = luaL_loadbuffer(L, script_buffer, strlen(script_buffer), "tio") ||
        lua_pcall(L, 0, 0, 0);
    if (error)
    {
        tio_warning_printf("lua: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);  /* Pop error message from the stack */
    }
}

static void script_file_run(lua_State *L, const char *filename)
{
    if (strlen(filename) == 0)
    {
        tio_warning_printf("Missing script filename\n");
        return;
    }

    if (luaL_dofile(L, filename))
    {
        tio_warning_printf("lua: %s", lua_tostring(L, -1));
        lua_pop(L, 1);  /* pop error message from the stack */
        return;
    }
}

// clang-format off
static const struct luaL_Reg tio_lib[] =
{
    { "echo", api_echo},
    { "sleep", api_sleep},
    { "msleep", api_msleep},
    { "line_set", api_line_set},
    { "line_get", api_line_get},
    { "send", api_send},
    { "receive", api_receive},
    { "write", api_write},
    { "twrite", api_twrite},
    { "read", api_read},
    { "readline", api_readline},
    { "ttysearch", api_ttysearch},

    { "send_break", api_send_break},
    { "set_local_echo", api_set_local_echo},
    { "set_log", api_set_log},
    { "flush_data_io_buffer", api_flush_data_io_buffer},
    { "set_input_mode", api_set_input_mode},
    { "set_output_mode", api_set_output_mode},
    { "set_raw_mode", api_set_raw_mode},
    { "set_raw_mode_interactive", api_set_raw_mode_interactive},
    { "set_timestamp_mode", api_set_timestamp_mode},
    { "exec_shell_command", api_exec_shell_command},
    { "get_state", api_get_state},
    { "get_version", api_get_version},

    { "inkey", api_inkey},
    { "input", api_input},
    { "inputline", api_input_line},
    { "set_keymap", api_set_keymap},

    { "subcmd_puts", api_subcmd_puts},
    { "subcmd_warning_puts", api_subcmd_warning_puts},
    { "subcmd_error_puts", api_subcmd_error_puts},

    {NULL, NULL}
};
// clang-format on

static void script_load(lua_State *L)
{
    int error;

    error = luaL_loadbuffer(L, script_init, strlen(script_init), "tio") || lua_pcall(L, 0, 0, 0);
    if (error)
    {
        tio_error_print("%s\n", lua_tostring(L, -1));
        lua_pop(L, 1); // Pop error message from the stack
    }
}

static void script_set_global_integer(lua_State *L, const char *name, int value)
{
    lua_pushinteger(L, value);
    lua_setglobal(L, name);
}

static void script_set_globals(lua_State *L)
{
    script_set_global_integer(L, "toggle", 2);
    script_set_global_integer(L, "high", 1);
    script_set_global_integer(L, "low", 0);
    script_set_global_integer(L, "XMODEM_SUM", XMODEM_SUM);
    script_set_global_integer(L, "XMODEM_CRC", XMODEM_CRC);
    script_set_global_integer(L, "XMODEM_1K", XMODEM_1K);
    script_set_global_integer(L, "YMODEM", YMODEM);
}

static void script_set_field_integer(lua_State *L, const char *name, int value)
{
    lua_pushinteger(L, value);
    lua_setfield(L, -2, name);
}

static void script_set_consts(lua_State *L)
{
    lua_getglobal(L, "tio");
    lua_getfield(L, -1, "C");

    script_set_field_integer(L, "IM_NORMAL", INPUT_MODE_NORMAL);
    script_set_field_integer(L, "IM_HEX", INPUT_MODE_HEX);
    script_set_field_integer(L, "IM_LINE", INPUT_MODE_LINE);
    script_set_field_integer(L, "OM_NORMAL", OUTPUT_MODE_NORMAL);
    script_set_field_integer(L, "OM_HEX", OUTPUT_MODE_HEX);
    script_set_field_integer(L, "RAW_OFF", RAW_OFF);
    script_set_field_integer(L, "RAW_ON", RAW_ON_DELAY);
    script_set_field_integer(L, "RAW_ON_NODELAY", RAW_ON_NODELAY);
    script_set_field_integer(L, "TS_OFF", TIMESTAMP_NONE);
    script_set_field_integer(L, "TS_24HOUR", TIMESTAMP_24HOUR);
    script_set_field_integer(L, "TS_24HOUR_START", TIMESTAMP_24HOUR_START);
    script_set_field_integer(L, "TS_24HOUR_DELTA", TIMESTAMP_24HOUR_DELTA);
    script_set_field_integer(L, "TS_ISO8601", TIMESTAMP_ISO8601);
    script_set_field_integer(L, "TS_EPOCH", TIMESTAMP_EPOCH);
    script_set_field_integer(L, "TS_EPOCH_USEC", TIMESTAMP_EPOCH_USEC);
    script_set_field_integer(L, "LN_TOGGLE", 2);
    script_set_field_integer(L, "LN_HIGH", 1);
    script_set_field_integer(L, "LN_LOW", 0);
    script_set_field_integer(L, "XM_SUM", XMODEM_SUM);
    script_set_field_integer(L, "XM_CRC", XMODEM_CRC);
    script_set_field_integer(L, "XM_1K", XMODEM_1K);
    script_set_field_integer(L, "YM_NORMAL", YMODEM);
    script_set_field_integer(L, "SA_INTERACTIVE", STATE_INTERACTIVE);
    script_set_field_integer(L, "SA_STARTING", STATE_STARTING);
    script_set_field_integer(L, "SA_PIPED_INPUT", STATE_PIPED_INPUT);
    script_set_field_integer(L, "SA_EXEC_SHELL_COMMAND", STATE_EXEC_SHELL_COMMAND);
    script_set_field_integer(L, "SA_XYMODEM", STATE_XYMODEM);

    lua_pop(L, 2);
}

static void script_gc_stop(lua_State *L)
{
#if LUA_VERSION_NUM >= 504
    lua_gc(L, LUA_GCSTOP);
#else
    lua_gc(L, LUA_GCSTOP, 0);
#endif
}

#if LUA_VERSION_NUM >= 502
static int luaopen_tio(lua_State *L)
{
    luaL_newlib(L, tio_lib);
    return 1;
}
#endif

static void script_require_tio(lua_State *L)
{
#if LUA_VERSION_NUM >= 502
    luaL_requiref(L, "tio", luaopen_tio, 1);
#else
    luaL_register(L, "tio", tio_lib);
#endif
    lua_pop(L, 1);
}

static void script_gc_restart(lua_State *L)
{
#if LUA_VERSION_NUM >= 505
    // Generational GC
    lua_gc(L, LUA_GCRESTART);
    lua_gc(L, LUA_GCGEN);
#elif LUA_VERSION_NUM >= 504
    lua_gc(L, LUA_GCRESTART);
    lua_gc(L, LUA_GCGEN, 20, 100);
#else
    // Incremental GC
    lua_gc(L, LUA_GCRESTART, 0);
#endif
}


static lua_State *script_interp_new(void)
{
    lua_State *L;

    if (script_interp != NULL) {
        lua_close(script_interp);
        script_interp = NULL;
    }

    L = luaL_newstate();
    if (L == NULL) {
        tio_error_printf("Can't allocate script buffer");
        return NULL;
    }

    script_interp = L;

    // Stop GC during initialization
    script_gc_stop(L);

    luaL_openlibs(L);

    // Equivalent to calling `require("tio")`
    script_require_tio(L);

    // Load lua init script
    script_load(L);

    // Initialize globals
    script_set_globals(L);
    script_set_consts(L);

    // Execute script-init file
    if (option.script_init_filename) {
        if (luaL_dofile(L, option.script_init_filename)) {
            tio_warning_printf("lua: %s", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }

    // Restart GC
    script_gc_restart(L);

    return L;
}

void script_device_bind(int fd)
{
    device_fd = fd;
}

void script_device_unbind(void)
{
    device_fd = 0;
}

void script_do_line(const char *script_line)
{
    assert(script_line != NULL);
    assert(script_interp != NULL);

    script_buffer_run(script_interp, script_line);
}

void script_run(const char *script_filename)
{
    static bool doopt_by_nul = true;

    assert(script_filename != NULL);
    assert(script_interp != NULL);

    if (script_filename[0] == '\0')
    {
        if (doopt_by_nul)
        {
            script_run_as_specified_by_options();
        }
        return;
    }
    else if (script_filename[0] == '@')
    {
        if (strcmp(script_filename, "@new") == 0)
        {
            tio_printf("Restart interpreter");
            script_interp_new();
        }
        else if (strcmp(script_filename, "@doopt") == 0)
        {
            script_run_as_specified_by_options();
        }
        else if (strcmp(script_filename, "@nuldo=opt") == 0)
        {
            doopt_by_nul = true;
        }
        else if (strcmp(script_filename, "@nuldo=none") == 0)
        {
            doopt_by_nul = false;
        }
        else
        {
            tio_printf("Unknown command");
        }
        return;
    }
    else
    {
        // if filename starts with '!', do filename's remain parts as lua commands.
        tio_printf("Running script %s", script_filename);
        if (script_filename[0] == '!')
        {
            script_buffer_run(script_interp, &script_filename[1]);
        }
        else
        {
            script_file_run(script_interp, script_filename);
        }
        return;
    }
}

void script_run_as_specified_by_options(void)
{
    assert(script_interp != NULL);

    if (option.script_filename != NULL)
    {
        tio_printf("Running script %s", option.script_filename);
        script_file_run(script_interp, option.script_filename);

    }
    else if (option.script != NULL)
    {
        tio_printf("Running script !%s", option.script);
        script_buffer_run(script_interp, option.script);
    }
}

const char *script_run_state_to_string(script_run_t run_state)
{
    switch (run_state)
    {
        case SCRIPT_RUN_ONCE:
            return "once";
        case SCRIPT_RUN_ALWAYS:
            return "always";
        case SCRIPT_RUN_NEVER:
            return "never";
        default:
            return "Unknown";
    }
}

void script_interp_init(void)
{
    if (script_interp_new() == NULL)
    {
        tio_error_printf("Could not start script interpreter.");
        exit(EXIT_FAILURE);
    }
}
