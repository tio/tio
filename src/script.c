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
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <lauxlib.h>
#include <lualib.h>
#include <sys/ioctl.h>
#include <ctype.h>
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

#define MAX_BUFFER_SIZE 2000 // Maximum size of circular buffer
#define READ_LINE_SIZE 4096 // read_line buffer length

static int device_fd;
static char circular_buffer[MAX_BUFFER_SIZE];
static char match_string[MAX_BUFFER_SIZE];
static int buffer_size = 0;

static char script_init[] =
"function set(arg)\n"
"    local dtr = arg.DTR or -1\n"
"    local rts = arg.RTS or -1\n"
"    local cts = arg.CTS or -1\n"
"    local dsr = arg.DSR or -1\n"
"    local cd = arg.CD or -1\n"
"    local ri = arg.RI or -1\n"
"    line_set(dtr, rts, cts, dsr, cd, ri)\n"
"end\n";

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

// lua: line_set(dtr,rts,cts,dsr,cd,ri)
static int line_set(lua_State *L)
{
    tty_line_config_t line_config[6] = { };

    int dtr = lua_tointeger(L, 1);
    int rts = lua_tointeger(L, 2);
    int cts = lua_tointeger(L, 3);
    int dsr = lua_tointeger(L, 4);
    int cd = lua_tointeger(L, 5);
    int ri = lua_tointeger(L, 6);

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

// lua: modem_send(file, protocol)
static int modem_send(lua_State *L)
{
    const char *file = lua_tostring(L, 1);
    int protocol = lua_tointeger(L, 2);
    int ret;

    if (file == NULL)
    {
        return 0;
    }

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

        case YMODEM:
            tio_printf("Sending file '%s' using YMODEM", file);
            ret = xymodem_send(device_fd, file, YMODEM);
            tio_printf("%s", ret < 0 ? "Aborted" : "Done");
            break;
    }

    return 0;
}

// lua: send(string)
static int write_(lua_State *L)
{
    size_t len = 0;
    const char *string = lua_tolstring(L, 1, &len);

    int ret;

    if (string == NULL)
    {
        return 0;
    }

    ret = write(device_fd, string, len);
    fsync(device_fd);  // flush these characters now
    tcdrain(device_fd); //ensure we flushed characters to our device

    lua_pushnumber(L, ret);

    return 1;
}

// Function to add a character to the circular expect buffer
static void expect_buffer_add(char c)
{
    if (!c)
    {
        return;
    }

    if (buffer_size < MAX_BUFFER_SIZE)
    {
        circular_buffer[buffer_size++] = c;
    }
    else
    {
        // Shift the buffer to accommodate the new character
        memmove(circular_buffer, circular_buffer + 1, MAX_BUFFER_SIZE - 1);
        circular_buffer[MAX_BUFFER_SIZE - 1] = c;
    }
}

// Function to match against the circular expect buffer using regex
static bool match_regex(regex_t *regex)
{
    char buffer[MAX_BUFFER_SIZE + 1]; // Temporary buffer for regex matching
    const char *s = circular_buffer;
    regmatch_t pmatch[1];
    regoff_t len;

    memcpy(buffer, circular_buffer, buffer_size);
    buffer[buffer_size] = '\0'; // Null-terminate the buffer

    // Match against the regex
    int ret = regexec(regex, buffer, 1, pmatch, 0);
    if (!ret)
    {
        // Match found
        len = pmatch[0].rm_eo - pmatch[0].rm_so;
        memcpy(match_string, s + pmatch[0].rm_so, len);
        match_string[len] = '\0';

        return true;
    }
    else if (ret == REG_NOMATCH)
    {
        // No match found, do nothing
    }
    else
    {
        // Error occurred during matching
        tio_error_print("Regex match failed");
    }

    return false;
}

// Function to echo a buffer to stdout and to the log
// per the option.timestamp and option.log settings
static void echo_buffer(char buffer[], ssize_t len)
{
    if (option.timestamp)
    {
        char *pTimeStampNow;
        pTimeStampNow = timestamp_current_time();
        if (pTimeStampNow)
        {
            tio_printf("%s", buffer); //does timestamps for us
            if (option.log)
            {
                log_printf("\n[%s] %s", pTimeStampNow, buffer);
            }
        }
    } else {
        for (ssize_t i=0; i<len; i++)
        {
            putchar(buffer[i]);

            if (option.log)
            {
                log_putc(buffer[i]);
            }
        }
    }
}

// lua: ret,string = read(size, timeout)
static int read_string(lua_State *L)
{
    int size = lua_tointeger(L, 1) + 1; //plus one for null-terminated string
    int timeout = lua_tointeger(L, 2);
    int ret = 0;

    char *buffer = calloc(1, size);
    if (buffer == NULL)
    {
        ret = -1; // Error
        goto error_rs;
    }

    if (timeout == 0)
    {
        timeout = -1; // Wait forever
    }

    ssize_t bytes_read = read_poll(device_fd, buffer, size, timeout);
    if (bytes_read < 0)
    {
        ret = -1; // Error
        goto error_rs;
    }
    else if (bytes_read == 0)
    {
        ret = 0; // Timeout
        goto error_rs;
    }
    else
    {
        buffer[bytes_read] = (char)0;
    }

    echo_buffer(&buffer[0], bytes_read);
    ret = bytes_read;

error_rs:
    lua_pushnumber(L, ret);
    if (buffer != NULL)
    {
        lua_pushstring(L, ret > 0 ? buffer : "");
        free(buffer);
    }
    else
    {
        lua_pushstring(L, ""); // give empty string to caller
    }
    return 2;
}

// lua: ret,string = read_line(timeout)
static int read_line(lua_State *L)
{
    static char linebuf[READ_LINE_SIZE];
    int timeout = lua_tointeger(L, 1); //ms
    int ret = 0;
    int read_result = 1; //enable reading input from device
    char ch;
    int bytes_read = 0;

    linebuf[0] = '\0';
    if (timeout == 0)
    {
        timeout = -1; // Wait forever
    }

    // loop reading input until a newline seen or timeout
    while (true)
    {
        read_result = read_poll(device_fd, &ch, 1, timeout);
        if (read_result < 0)
        {
            ret = -1; // Error
            linebuf[bytes_read] = '\0';
            goto error_rl;
        }
        else if (!read_result)
        {
            // Timeout returns a non-empty linebuf as a 'line'
            ret = bytes_read;
            linebuf[bytes_read] = '\0';
            break;
        }
        else // we got a character, so handle it
        {
            if (ch == '\n')
            {
                linebuf[bytes_read] = '\0';
                break;
            }
            else if (bytes_read <= (READ_LINE_SIZE-2))
            {
                if (isprint(ch)) // store all printable chars
                {
                    linebuf[bytes_read++] = ch;
                }
            }
        }
    }

    if (bytes_read)
    {
        echo_buffer(linebuf, bytes_read);
    }
    ret = bytes_read;

error_rl:
    lua_pushnumber(L, ret);
    lua_pushstring(L, linebuf);
    return 2;
}

// lua: expect(string, timeout)
static int expect(lua_State *L)
{
    const char *string = lua_tostring(L, 1);
    long timeout = lua_tointeger(L, 2);
    regex_t regex;
    int ret = 0;
    char c;

    // Resets buffer to ignore previous `expect` calls
    buffer_size = 0;
    match_string[0] = '\0';

    if ((string == NULL) || (timeout < 0))
    {
        ret = -1;
        goto error;
    }

    if (timeout == 0)
    {
        // Let poll() wait forever
        timeout = -1;
    }

    // Compile the regular expression
    ret = regcomp(&regex, string, REG_EXTENDED);
    if (ret)
    {
        tio_error_print("Could not compile regex");
        ret = -1;
        goto error;
    }

    // Main loop to read and match
    while (true)
    {
        ssize_t bytes_read = read_poll(device_fd, &c, 1, timeout);
        if (bytes_read > 0)
        {
            putchar(c);
            expect_buffer_add(c);

            if (option.log)
            {
                log_putc(c);
            }

            // Match against the entire buffer
            if (match_regex(&regex))
            {
                ret = 1;
                break;
            }
        }
        else
        {
            // Timeout or error
            break;
        }
    }

    // Cleanup
    regfree(&regex);

error:
    lua_pushnumber(L, ret);
    lua_pushstring(L, match_string);
    return 2;
}

// lua: exit(code)
static int exit_(lua_State *L)
{
    long code = lua_tointeger(L, 1);

    exit(code);

    return 0;
}

// lua: list = tty_search()
static int tty_search_(lua_State *L)
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

static const struct luaL_Reg tio_lib[] =
{
    { "sleep", sleep_},
    { "msleep", msleep},
    { "line_set", line_set},
    { "send", modem_send},
    { "write", write_},
    { "read", read_string},
    { "read_line", read_line},
    { "expect", expect},
    { "exit", exit_},
    { "tty_search", tty_search_},
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

int lua_register_tio(lua_State *L)
{
    // Register lxi functions
    lua_getglobal(L, "_G");
    luaL_setfuncs(L, tio_lib, 0);
    lua_pop(L, 1);

    // Load lua init script
    script_load(L);

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
        tio_warning_printf("lua: %s", lua_tostring(L, -1));
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
    script_set_global(L, "toggle", 2);
    script_set_global(L, "high", 1);
    script_set_global(L, "low", 0);
    script_set_global(L, "XMODEM_CRC", XMODEM_CRC);
    script_set_global(L, "XMODEM_1K", XMODEM_1K);
    script_set_global(L, "YMODEM", YMODEM);
}

void script_run(int fd, const char *script_filename)
{
    lua_State *L;

    device_fd = fd;

    L = luaL_newstate();
    luaL_openlibs(L);

    // Bind tio functions
    lua_register_tio(L);

    // Initialize globals
    script_set_globals(L);

    if (script_filename != NULL)
    {
        tio_printf("Running script %s", script_filename);
        script_file_run(L, script_filename);
    }
    else if (option.script_filename != NULL)
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

const char *script_run_state_to_string(script_run_t state)
{
    switch (state)
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
