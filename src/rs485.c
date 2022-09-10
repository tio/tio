/*
 * tio - a simple serial device I/O tool
 *
 * Copyright (c) 2022  Martin Lund
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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include "options.h"
#include "print.h"
#include "error.h"

#ifdef HAVE_RS485

#include <linux/serial.h>

static struct serial_rs485 rs485_config_saved;
static struct serial_rs485 rs485_config;
static bool rs485_config_written = false;

void rs485_parse_config(const char *arg)
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
            char keyname[31];
            unsigned int value;
            sscanf(token, "%30[^=]=%d", keyname, &value);

            if (!strcmp(keyname, "RTS_ON_SEND"))
            {
                if (value)
                {

                    /* Set logical level for RTS pin equal to 1 when sending */
                    option.rs485_config_flags |= SER_RS485_RTS_ON_SEND;
                }
                else
                {
                    /* Set logical level for RTS pin equal to 0 when sending */
                    option.rs485_config_flags &= ~(SER_RS485_RTS_ON_SEND);
                }
            }
            else if (!strcmp(keyname, "RTS_AFTER_SEND"))
            {
                if (value)
                {
                    /* Set logical level for RTS pin equal to 1 after sending */
                    option.rs485_config_flags |= SER_RS485_RTS_AFTER_SEND;
                }
                else
                {
                    /* Set logical level for RTS pin equal to 0 after sending */
                    option.rs485_config_flags &= ~(SER_RS485_RTS_AFTER_SEND);
                }
            }
            else if (!strcmp(keyname, "RTS_DELAY_BEFORE_SEND"))
            {
                /* Set RTS delay before send */
                option.rs485_delay_rts_before_send = value;
            }
            else if (!strcmp(keyname, "RTS_DELAY_AFTER_SEND"))
            {
                /* Set RTS delay after send */
                option.rs485_delay_rts_after_send = value;
            }
            else if (!strcmp(keyname, "RX_DURING_TX"))
            {
                /* Receive data even while sending data */
                option.rs485_config_flags |= SER_RS485_RX_DURING_TX;
            }
        }
        else
        {
            token_found = false;
        }
    }
    free(buffer);
}

void rs485_print_config(void)
{
    tio_printf(" RS-485 Configuration:");
    tio_printf("  RTS_ON_SEND: %s", (rs485_config.flags & SER_RS485_RTS_ON_SEND) ? "high" : "low");
    tio_printf("  RTS_AFTER_SEND: %s", (rs485_config.flags & SER_RS485_RTS_AFTER_SEND) ? "high" : "low");
    tio_printf("  RTS_DELAY_BEFORE_SEND = %d", rs485_config.delay_rts_before_send);
    tio_printf("  RTS_DELAY_AFTER_SEND = %d", rs485_config.delay_rts_after_send);
    tio_printf("  RX_DURING_TX: %s", (rs485_config.flags & SER_RS485_RX_DURING_TX) ? "enabled" : "disabled");
}

int rs485_mode_enable(int fd)
{
    /* Save existing RS-485 configuration */
    ioctl (fd, TIOCGRS485, &rs485_config_saved);

    /* Prepare new RS-485 configuration */
    rs485_config.flags = SER_RS485_ENABLED;
    rs485_config.flags |= option.rs485_config_flags;

    if (option.rs485_delay_rts_before_send > 0)
    {
        rs485_config.delay_rts_before_send = option.rs485_delay_rts_before_send;
    }
    else
    {
        rs485_config.delay_rts_before_send = rs485_config_saved.delay_rts_before_send;
    }

    if (option.rs485_delay_rts_after_send > 0)
    {
        rs485_config.delay_rts_after_send = option.rs485_delay_rts_after_send;
    }
    else
    {
        rs485_config.delay_rts_after_send = rs485_config_saved.delay_rts_after_send;
    }

    /* Write new RS-485 configuration */
    if (ioctl(fd, TIOCSRS485, &rs485_config) < 0)
    {
        tio_warning_printf("RS-485 mode is not supported by your device (%s)", strerror(errno));
        return -1;
    }

    rs485_config_written = true;

    return 0;
}

void rs485_mode_restore(int fd)
{
    if (rs485_config_written)
    {
        /* Write saved RS-485 configuration */
        if (ioctl(fd, TIOCSRS485, &rs485_config_saved) < 0)
        {
            tio_warning_printf("TIOCGRS485 ioctl failed (%s)", strerror(errno));
        }
    }
}

#else

void rs485_parse_config(const char *arg)
{
    UNUSED(arg);
    return;
}

void rs485_print_config(void)
{
    return;
}

int rs485_mode_enable(int fd)
{
    UNUSED(fd);
    tio_error_printf("RS485 mode is not supported on your system");
    exit(EXIT_FAILURE);
}

void rs485_mode_restore(int fd)
{
    UNUSED(fd);
    return;
}

#endif
