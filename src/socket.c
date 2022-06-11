/*
 * tio - a simple serial terminal I/O tool
 *
 * Copyright (c) 2014-2022  Martin Lund
 * Copyright (c) 2022  Google LLC
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

#include "socket.h"
#include "options.h"
#include "print.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define MAX_SOCKET_CLIENTS 16

static int sockfd;
static int clientfds[MAX_SOCKET_CLIENTS];

static const char *socket_filename(void)
{
    /* skip 'unix:' */
    return option.socket + 5;
}

static void socket_exit(void)
{
    unlink(socket_filename());
}

void socket_configure(void)
{
    if (!option.socket)
    {
        return;
    }

    if (strncmp(option.socket, "unix:", 5) != 0)
    {
        error_printf("%s: Invalid socket scheme, must be 'unix:'", option.socket);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_un sockaddr = {};
    if (strlen(socket_filename()) > sizeof(sockaddr.sun_path) - 1)
    {
        error_printf("Socket file path %s too long", option.socket);
        exit(EXIT_FAILURE);
    }
 
    sockaddr.sun_family = AF_UNIX;
    strncpy(sockaddr.sun_path, socket_filename(), sizeof(sockaddr.sun_path) - 1);

    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        error_printf("Failed to create socket: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (bind(sockfd, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0)
    {
        error_printf("Failed to bind to socket %s: %s", socket_filename(), strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (listen(sockfd, MAX_SOCKET_CLIENTS) < 0)
    {
        error_printf("Failed to listen on socket %s: %s", socket_filename(), strerror(errno));
        exit(EXIT_FAILURE);
    }

    memset(clientfds, -1, sizeof(clientfds));
    atexit(socket_exit);

    tio_printf("Listening on socket %s", socket_filename());
}

void socket_write(char input_char)
{
    if (!option.socket)
    {
        return;
    }

    for (int i = 0; i != MAX_SOCKET_CLIENTS; ++i)
    {
        if (clientfds[i] != -1)
        {
            if (write(clientfds[i], &input_char, 1) <= 0)
            {
                error_printf_silent("Failed to write to socket: %s", strerror(errno));
                close(clientfds[i]);
                clientfds[i] = -1;
            }
        }
    }
}

int socket_add_fds(fd_set *rdfs, bool connected)
{
    if (!option.socket)
    {
        return 0;
    }

    int numclients = 0, maxfd = 0;
    for (int i = 0; i != MAX_SOCKET_CLIENTS; ++i)
    {
        if (clientfds[i] != -1)
        {
            /* let clients block if they try to send while we're disconnected */
            if (connected)
            {
                FD_SET(clientfds[i], rdfs);
                maxfd = MAX(maxfd, clientfds[i]);
            }
            numclients++;
        }
    }
    /* don't bother to accept clients if we're already full */
    if (numclients != MAX_SOCKET_CLIENTS)
    {
        FD_SET(sockfd, rdfs);
        maxfd = MAX(maxfd, sockfd);
    }
    return maxfd;
}

bool socket_handle_input(fd_set *rdfs, char *output_char)
{
    if (!option.socket)
    {
        return false;
    }

    if (FD_ISSET(sockfd, rdfs))
    {
        int clientfd = accept(sockfd, NULL, NULL);
        /* this loop should always succeed because we don't select on sockfd when full */
        for (int i = 0; i != MAX_SOCKET_CLIENTS; ++i)
        {
            if (clientfds[i] == -1)
            {
                clientfds[i] = clientfd;
                break;
            }
        }
    }
    for (int i = 0; i != MAX_SOCKET_CLIENTS; ++i)
    {
        if (clientfds[i] != -1 && FD_ISSET(clientfds[i], rdfs))
        {
            int status = read(clientfds[i], output_char, 1);
            if (status == 0)
            {
                close(clientfds[i]);
                clientfds[i] = -1;
                continue;
            }
            if (status < 0)
            {
                error_printf_silent("Failed to read from socket: %s", strerror(errno));
                close(clientfds[i]);
                clientfds[i] = -1;
                continue;
            }
            /* match the behavior of a terminal in raw mode */
            if (*output_char == '\n')
            {
                *output_char = '\r';
            }
            return true;
        }
    }
    return false;
}
