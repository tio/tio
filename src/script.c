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
#include "misc.h"
#include "print.h"
#include "options.h"
#include "tty.h"
#include "xymodem.h"

static int serial_fd;

// lua: sleep(seconds)
static int sleep_(lua_State *L)
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

// lua: msleep(miliseconds)
static int msleep(lua_State *L)
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

// lua: high(line)
static int high(lua_State *L)
{
    long line = lua_tointeger(L, 1);

    if (line < 0)
    {
        return 0;
    }

    tty_line_set(serial_fd, line, LINE_HIGH);

    return 0;
}

// lua: low(line)
static int low(lua_State *L)
{
    long line = lua_tointeger(L, 1);

    if (line < 0)
    {
        return 0;
    }

    tty_line_set(serial_fd, line, LINE_LOW);

    return 0;
}

// lua: toggle(line)
static int toggle(lua_State *L)
{
    long line = lua_tointeger(L, 1);

    if (line < 0)
    {
        return 0;
    }

    tty_line_toggle(serial_fd, line);

    return 0;
}

// lua: config_high(line)
static int config_high(lua_State *L)
{
    long line = lua_tointeger(L, 1);

    if (line < 0)
    {
        return 0;
    }

    tty_line_config(line, true);

    return 0;
}

// lua: config_low(line)
static int config_low(lua_State *L)
{
    long line = lua_tointeger(L, 1);

    if (line < 0)
    {
        return 0;
    }

    tty_line_config(line, false);

    return 0;
}

// lua: config_apply(line)
static int config_apply(lua_State *L)
{
    UNUSED(L);

    tty_line_config_apply();

    return 0;
}

// lua: modem_send(file, protocol)
static int modem_send(lua_State *L)
{
    const char *file = lua_tostring(L, 1);
    int protocol = lua_tointeger(L, 2);

    if (file == NULL)
    {
        return 0;
    }

    switch (protocol)
    {
        case XMODEM_1K:
            tio_printf("Sending file '%s' using XMODEM-1K", file);
            tio_printf("%s", xymodem_send(serial_fd, file, XMODEM_1K) < 0 ? "Aborted" : "Done");
            break;

        case XMODEM_CRC:
            tio_printf("Sending file '%s' using XMODEM-CRC", file);
            tio_printf("%s", xymodem_send(serial_fd, file, XMODEM_CRC) < 0 ? "Aborted" : "Done");
        break;

        case YMODEM:
            tio_printf("Sending file '%s' using YMODEM", file);
            tio_printf("%s", xymodem_send(serial_fd, file, YMODEM) < 0 ? "Aborted" : "Done");
        break;
    }

    return 0;
}

// lua: send(string)
static int send(lua_State *L)
{
    const char *string = lua_tostring(L, 1);
    int ret;

    if (string == NULL)
    {
        return 0;
    }

    ret = write(serial_fd, string, strlen(string));
    if (ret < 0)
    {
        tio_error_print("%s\n", strerror(errno));
    }

    lua_pushnumber(L, ret);

    return 1;
}

static void script_buffer_run(lua_State *L, const char *script_buffer)
{
    int error;

    error = luaL_loadbuffer(L, script_buffer, strlen(script_buffer), "tio") ||
        lua_pcall(L, 0, 0, 0);
    if (error)
    {
        tio_warning_printf("%s\n", lua_tostring(L, -1));
        lua_pop(L, 1);  /* pop error message from the stack */
    }
}

static const struct luaL_Reg tio_lib[] =
{
    { "sleep", sleep_},
    { "msleep", msleep},
    { "high", high},
    { "low", low},
    { "toggle", toggle},
    { "config_high", config_high},
    { "config_low", config_low},
    { "config_apply", config_apply},
    { "modem_send", modem_send},
    { "send", send},
    {NULL, NULL}
};

#if !defined LUA_VERSION_NUM || LUA_VERSION_NUM==501
/*
** Adapted from Lua 5.2.0 (for backwards compatibility)
*/
static void luaL_setfuncs (lua_State *L, const luaL_Reg *l, int nup)
{
  luaL_checkstack(L, nup+1, "too many upvalues");
  for (; l->name != NULL; l++) {  /* fill the table with given functions */
    int i;
    lua_pushstring(L, l->name);
    for (i = 0; i < nup; i++)  /* copy upvalues to the top */
      lua_pushvalue(L, -(nup+1));
    lua_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
    lua_settable(L, -(nup + 3));
  }
  lua_pop(L, nup);  /* remove upvalues */
}
#endif

int lua_register_tio(lua_State *L)
{
    // Register lxi functions
    lua_getglobal(L, "_G");
    luaL_setfuncs(L, tio_lib, 0);
    lua_pop(L, 1);

    return 0;
}

void script_file_run(lua_State *L, const char *filename)
{
    if (strlen(filename) == 0)
    {
        tio_warning_printf("Missing script filename\n");
        return;
    }

    if (luaL_dofile(L, filename))
    {
        tio_warning_printf("%s\n", lua_tostring(L, -1));
        lua_pop(L, 1);  /* pop error message from the stack */
        return;
    }
}

void script_set_global(lua_State *L, const char *name, long value)
{
    lua_pushnumber(L, value);
    lua_setglobal(L, name);
}

void script_set_globals(lua_State *L)
{
    script_set_global(L, "DTR", TIOCM_DTR);
    script_set_global(L, "RTS", TIOCM_RTS);
    script_set_global(L, "CTS", TIOCM_CTS);
    script_set_global(L, "DSR", TIOCM_DSR);
    script_set_global(L, "CD", TIOCM_CD);
    script_set_global(L, "RI", TIOCM_RI);
    script_set_global(L, "XMODEM_CRC", XMODEM_CRC);
    script_set_global(L, "XMODEM_1K", XMODEM_1K);
    script_set_global(L, "YMODEM", YMODEM);
}

void script_run(int fd)
{
    lua_State *L;

    serial_fd = fd;

    L = luaL_newstate();
    luaL_openlibs(L);

    // Bind tio functions
    lua_register_tio(L);

    // Initialize globals
    script_set_globals(L);

    if (option.script_filename != NULL)
    {
        tio_printf("Running script %s", option.script_filename);
        script_file_run(L, option.script_filename);
    }
    else if (option.script != NULL)
    {
        tio_printf("Running script");
        script_buffer_run(L, option.script);
    }

    lua_close(L);
}
