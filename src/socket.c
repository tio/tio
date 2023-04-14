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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>

#include "socket.h"
#include "options.h"
#include "print.h"

#define MAX_SOCKET_CLIENTS 16
#define SOCKET_PORT_DEFAULT 3333

static int sockfd;
static int clientfds[MAX_SOCKET_CLIENTS];
static int socket_family = AF_UNSPEC;
static int port_number = SOCKET_PORT_DEFAULT;
bool map_i_nl_cr = false;
bool map_i_cr_nl = false;
bool map_ign_cr = false;

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

static bool socket_stale(const char *path)
{
    struct sockaddr_un addr;
    bool stale = false;
    int sockfd;

    /* Test if socket file exists */
    if (access(path, F_OK) == 0)
    {
        /* Create test socket  */
        sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sockfd < 0)
        {
            tio_warning_printf("Failure opening socket (%s)", strerror(errno));
            return false;
        }

        /* Prepare address */
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

        /* Perform connect to test if socket is active */
        if (connect(sockfd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un)) == -1)
        {
            if (errno == ECONNREFUSED)
            {
                // No one is listening on socket file
                stale = true;
            }
        }

        /* Cleanup */
        close(sockfd);
    }

    return stale;
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
            tio_error_printf("Missing socket filename");
            exit(EXIT_FAILURE);
        }

        if (strlen(socket_filename()) > sizeof(sockaddr_unix.sun_path) - 1)
        {
            tio_error_printf("Socket file path %s too long", option.socket);
            exit(EXIT_FAILURE);
        }
    }

    if (strncmp(option.socket, "inet:", 5) == 0)
    {
        socket_family = AF_INET;

        port_number = socket_inet_port();

        if (port_number < 0)
        {
            tio_error_printf("Invalid port number: %d", port_number);
            exit(EXIT_FAILURE);
        }
    }

    if (strncmp(option.socket, "inet6:", 6) == 0)
    {
        socket_family = AF_INET6;

        port_number = socket_inet6_port();

        if (port_number < 0)
        {
            tio_error_printf("Invalid port number: %d", port_number);
            exit(EXIT_FAILURE);
        }
    }

    if (socket_family == AF_UNSPEC)
    {
        tio_error_printf("%s: Invalid socket scheme, must be prefixed with 'unix:', 'inet:', or 'inet6:'", option.socket);
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

            /* Test for stale unix socket file */
            if (socket_stale(socket_filename()))
            {
                tio_printf("Cleaning up old socket file");
                unlink(socket_filename());
            }

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
            tio_error_printf("Invalid socket family (%d)", socket_family);
            exit(EXIT_FAILURE);
            break;
    }

    /* Create socket */
    sockfd = socket(socket_family, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        tio_error_printf("Failed to create socket (%s)", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Bind */
    if (bind(sockfd, sockaddr_p, socklen) < 0)
    {
        tio_error_printf("Failed to bind to socket (%s)", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Listen */
    if (listen(sockfd, MAX_SOCKET_CLIENTS) < 0)
    {
        tio_error_printf("Failed to listen on socket (%s)", strerror(errno));
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
                tio_error_printf_silent("Failed to write to socket (%s)", strerror(errno));
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
                tio_error_printf_silent("Failed to read from socket (%s)", strerror(errno));
                close(clientfds[i]);
                clientfds[i] = -1;
                continue;
            }

            /* If INLCR is set, a received NL character shall be translated into a CR character */
            if (*output_char == '\n' && map_i_nl_cr)
            {
                *output_char = '\r';
            }
            else if (*output_char == '\r')
            {
                /* If IGNCR is set, a received CR character shall be ignored (not read). */
                if (map_ign_cr)
                {
                    return false;
                }

                /* If IGNCR is not set and ICRNL is set, a received CR character shall be translated into an NL character. */
                if (map_i_cr_nl)
                {
                    *output_char = '\n';
                }
            }
            return true;
        }
    }
    return false;
}
