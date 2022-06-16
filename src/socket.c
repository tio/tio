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
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <unistd.h>

#define MAX_SOCKET_CLIENTS 16
#define SOCKET_PORT_DEFAULT 3333

static int sockfd;
static int clientfds[MAX_SOCKET_CLIENTS];
static int socket_family = AF_UNSPEC;
static int port_number = SOCKET_PORT_DEFAULT;

static const char *socket_filename(void)
{
    /* skip 'unix:' */
    return option.socket + 5;
}

static int socket_inet_port(void)
{
    /* skip 'inet:' */
    int port_number = atoi(option.socket + 5);
    if (port_number == 0)
    {
        port_number = SOCKET_PORT_DEFAULT;
    }
    return port_number;
}

static int socket_inet6_port(void)
{
    /* skip 'inet6:' */
    int port_number = atoi(option.socket + 6);
    if (port_number == 0)
    {
        port_number = SOCKET_PORT_DEFAULT;
    }
    return port_number;
}

static void socket_exit(void)
{
    if (socket_family == AF_UNIX)
    {
        unlink(socket_filename());
    }
}

void socket_configure(void)
{
    struct sockaddr_un sockaddr_unix = {};
    struct sockaddr_in sockaddr_inet = {};
    struct sockaddr_in6 sockaddr_inet6 = {};
    struct sockaddr *sockaddr_p;
    socklen_t socklen;

    /* Parse socket string */

    if (strncmp(option.socket, "unix:", 5) == 0)
    {
        socket_family = AF_UNIX;

        if (strlen(socket_filename()) == 0)
        {
            error_printf("Missing socket filename");
            exit(EXIT_FAILURE);
        }

        if (strlen(socket_filename()) > sizeof(sockaddr_unix.sun_path) - 1)
        {
            error_printf("Socket file path %s too long", option.socket);
            exit(EXIT_FAILURE);
        }
    }

    if (strncmp(option.socket, "inet:", 5) == 0)
    {
        socket_family = AF_INET;

        port_number = socket_inet_port();

        if (port_number < 0)
        {
            error_printf("Invalid port number: %d", port_number);
            exit(EXIT_FAILURE);
        }
    }

    if (strncmp(option.socket, "inet6:", 6) == 0)
    {
        socket_family = AF_INET6;

        port_number = socket_inet6_port();

        if (port_number < 0)
        {
            error_printf("Invalid port number: %d", port_number);
            exit(EXIT_FAILURE);
        }
    }

    if (socket_family == AF_UNSPEC)
    {
        error_printf("%s: Invalid socket scheme, must be prefixed with 'unix:', 'inet:', or 'inet6:'", option.socket);
        exit(EXIT_FAILURE);
    }
 
    /* Configure socket */

    switch (socket_family)
    {
        case AF_UNIX:
            sockaddr_unix.sun_family = AF_UNIX;
            strncpy(sockaddr_unix.sun_path, socket_filename(), sizeof(sockaddr_unix.sun_path) - 1);
            sockaddr_p = (struct sockaddr *) &sockaddr_unix;
            socklen = sizeof(sockaddr_unix);
            break;

        case AF_INET:
            sockaddr_inet.sin_family = AF_INET;
            sockaddr_inet.sin_addr.s_addr = INADDR_ANY;
            sockaddr_inet.sin_port = htons(port_number);
            sockaddr_p = (struct sockaddr *) &sockaddr_inet;
            socklen = sizeof(sockaddr_inet);
            break;

        case AF_INET6:
            sockaddr_inet6.sin6_family = AF_INET6;
            sockaddr_inet6.sin6_addr = in6addr_any;
            sockaddr_inet6.sin6_port = htons(port_number);
            sockaddr_p = (struct sockaddr *) &sockaddr_inet6;
            socklen = sizeof(sockaddr_inet6);
            break;

        default:
            error_printf("Invalid socket family (%d)", socket_family);
            exit(EXIT_FAILURE);
            break;
    }

    /* Create socket */
    sockfd = socket(socket_family, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        error_printf("Failed to create socket (%s)", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Bind */
    if (bind(sockfd, sockaddr_p, socklen) < 0)
    {
        error_printf("Failed to bind to socket (%s)", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Listen */
    if (listen(sockfd, MAX_SOCKET_CLIENTS) < 0)
    {
        error_printf("Failed to listen on socket (%s)", strerror(errno));
        exit(EXIT_FAILURE);
    }

    memset(clientfds, -1, sizeof(clientfds));
    atexit(socket_exit);

    if (socket_family == AF_UNIX)
    {
        tio_printf("Listening on socket %s", socket_filename());
    }
    else
    {
        tio_printf("Listening on socket port %d", port_number);
    }
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
                error_printf_silent("Failed to write to socket (%s)", strerror(errno));
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
                error_printf_silent("Failed to read from socket (%s)", strerror(errno));
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
