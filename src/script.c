/*
 * tio - a simple serial terminal I/O tool
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <lauxlib.h>
#include <lualib.h>
#include <sys/ioctl.h>
#include "print.h"
#include "options.h"
#include "tty.h"

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

static void script_line_set(int line, bool value)
{
    switch (line)
    {
        case TIOCM_DTR:
            tty_line_set(serial_fd, "DTR", line, value);
            break;
        case TIOCM_RTS:
            tty_line_set(serial_fd, "RTS", line, value);
            break;
        case TIOCM_CTS:
            tty_line_set(serial_fd, "CTS", line, value);
            break;
        case TIOCM_DSR:
            tty_line_set(serial_fd, "DSR", line, value);
            break;
        case TIOCM_CD:
            tty_line_set(serial_fd, "CD", line, value);
            break;
        case TIOCM_RI:
            tty_line_set(serial_fd, "RI", line, value);
            break;
        default:
            break;
    }
}

static void script_line_toggle(int line)
{
    switch (line)
    {
        case TIOCM_DTR:
            tty_line_toggle(serial_fd, "DTR", line);
            break;
        case TIOCM_RTS:
            tty_line_toggle(serial_fd, "RTS", line);
            break;
        case TIOCM_CTS:
            tty_line_toggle(serial_fd, "CTS", line);
            break;
        case TIOCM_DSR:
            tty_line_toggle(serial_fd, "DSR", line);
            break;
        case TIOCM_CD:
            tty_line_toggle(serial_fd, "CD", line);
            break;
        case TIOCM_RI:
            tty_line_toggle(serial_fd, "RI", line);
            break;
        default:
            break;
    }
}

// lua: high(line)
static int high(lua_State *L)
{
    long line = lua_tointeger(L, 1);

    if (line < 0)
    {
        return 0;
    }

    script_line_set(line, LINE_HIGH);

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

    script_line_set(line, LINE_LOW);

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

    script_line_toggle(line);

    return 0;
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
