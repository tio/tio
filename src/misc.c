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

#define _GNU_SOURCE  // For FNM_EXTMATCH
#include <unistd.h>
#include <string.h>
#include <poll.h>
#include <sys/wait.h>
#include <fnmatch.h>
#include <regex.h>
#include <errno.h>
#include "print.h"
#include "misc.h"

static pid_t shell_command_pid = 0;

void delay(long ms)
{
    struct timespec ts;

    if (ms <= 0)
    {
        return;
    }

    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;

    nanosleep(&ts, NULL);
}

int ctrl_key_code(unsigned char key)
{
    if ((key >= 'a') && (key <= 'z'))
    {
        return key & ~0x60;
    }

    return -1;
}

int ctrl_key_char(int key_code)
{
    if (key_code >= ('a' & ~0x60) && key_code <= ('z' & ~0x60))
    {
        return key_code | 0x60;
    }

    return -1;
}

bool regex_match(const char *string, const char *pattern)
{
    regex_t regex;
    int status;

    if (regcomp(&regex, pattern, REG_EXTENDED | REG_NOSUB) != 0)
    {
        // No match
        return false;
    }

    status = regexec(&regex, string, (size_t) 0, NULL, 0);
    regfree(&regex);

    if (status != 0)
    {
        // No match
        return false;
    }

    // Match
    return true;
}

int read_poll(int fd, void *data, size_t len, int timeout)
{
    struct pollfd fds;
    int ret = 0;

    fds.events = POLLIN;
    fds.fd = fd;

    /* Wait data available */
    ret = poll(&fds, 1, timeout);
    if (ret < 0)
    {
        tio_error_print("%s", strerror(errno));
        return ret;
    }
    else if (ret > 0)
    {
        if (fds.revents & POLLIN)
        {
            // Read ready data
            // return value should not be 0
            return read(fd, data, len);
        }
        else /* if (fds.revents & (POLLERR | POLLHUP | POLLNVAL)) */
        {
            return -1;
        }
    }

    /* Timeout */
    return 0;
}

ssize_t write_poll(int fd, const void *data, size_t len, int timeout)
{
    struct pollfd fds;
    ssize_t ret = 0;

    fds.events = POLLOUT;
    fds.fd = fd;

    /* Wait data available */
    ret = poll(&fds, 1, timeout);
    if (ret < 0)
    {
        tio_error_print("%s", strerror(errno));
        return ret;
    }
    else if (ret > 0)
    {
        if (fds.revents & POLLOUT)
        {
            // Ready to write
            // return value should not be 0
            return write(fd, data, len);
        }
        else /* if (fds.revents & (POLLERR | POLLHUP | POLLNVAL)) */
        {
            return -1;
        }
    }

    /* Timeout */
    return 0;
}


// Function to calculate djb2 hash of string
unsigned long djb2_hash(const unsigned char *str)
{
    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
    {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }

    return hash;
}

// Function to encode a number to base62
void *base62_encode(unsigned long num, char *output)
{
    const char base62_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    if (output == NULL)
    {
        tio_error_print("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < 4; ++i)
    {
        output[i] = base62_chars[num % 62];
        num /= 62;
    }
    output[4] = '\0';

    return output;
}

// Function to return current time
double get_current_time(void)
{
    struct timespec current_time_ts;

    if (clock_gettime(CLOCK_REALTIME, &current_time_ts) == -1)
    {
        // Error
        return -1;
    }

    return current_time_ts.tv_sec + current_time_ts.tv_nsec / 1e9;
}

bool match_patterns(const char *string, const char *patterns)
{
    char *pattern;
    char *patterns_copy;

    if ((string == NULL) || (patterns == NULL))
    {
        return false;
    }

    patterns_copy = strdup(patterns);

    // Tokenize the patterns string using strtok
    pattern = strtok(patterns_copy, ",");
    while (pattern != NULL)
    {
        // clang-format off
        // Check if the string matches the current pattern
        #ifdef FNM_EXTMATCH
            if (fnmatch(pattern, string, FNM_EXTMATCH) == 0)
        #else
            if (fnmatch(pattern, string, 0) == 0)
        #endif
        {
            free(patterns_copy);
            return true;
        }
        // clang-format on

        // Move to the next pattern
        pattern = strtok(NULL, ",");
    }

    free(patterns_copy);
    return false;
}

// Function that forks subprocess, redirects its stdout and stderr to the
// specified filedescriptor, and runs command.
int execute_shell_command(int fd, const char *command)
{
    #define READ_END 0
    #define WRITE_END 1
    int status;
    int pipefd_c2p[2];
    int pipefd_p2c[2];

#if defined(__linux__)
    static bool done_once = false;
    if (!done_once)
    {
        atexit(&terminate_shell_command);
        done_once = true;
    }
#endif

    // Create Pipes
    if (pipe(pipefd_c2p) == -1 || pipe(pipefd_p2c) == -1)
    {
        tio_error_print("pipe() failed (%s)", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Fork a child process
    shell_command_pid = fork();
    if (shell_command_pid == -1)
    {
        // Error occurred
        tio_error_print("fork() failed (%s)", strerror(errno));
        exit(EXIT_FAILURE);
    }
    else if (shell_command_pid == 0)
    {
        // Child process
        close(pipefd_c2p[READ_END]);
        close(pipefd_p2c[WRITE_END]);

        tio_printf("Executing shell command '%s'", command);

        // Redirect stdin and stdout to the parent-pipe
        if (dup2(pipefd_c2p[WRITE_END], STDOUT_FILENO) == -1 ||
            dup2(pipefd_p2c[READ_END], STDIN_FILENO) == -1)
        {
            tio_error_print("dup2() failed (%s)", strerror(errno));
            exit(EXIT_FAILURE);
        }

        // command prefix '?' excludes stderr from redirection
        if (command[0] == '?')
        {
            command += 1;
        }
        else
        {
            if (dup2(pipefd_c2p[WRITE_END], STDERR_FILENO) == -1)
            {
                tio_error_print("dup2() failed (%s)", strerror(errno));
                exit(EXIT_FAILURE);
            }
        }

        // Execute the shell command
        execl("/bin/sh", "sh", "-c", command, (char *)NULL);

        // If execlp() returns, it means an error occurred
        close(pipefd_c2p[WRITE_END]);
        close(pipefd_p2c[READ_END]);
        perror("execlp");
        tio_error_print("execlp() failed (%s)", strerror(errno));
        exit(EXIT_FAILURE);
    }
    else
    {
        // Parent process
        fd_set rdfs;
        int maxfd;
        char buf[BUFSIZ];
        int bytes;

        close(pipefd_c2p[WRITE_END]);
        close(pipefd_p2c[READ_END]);

        while (true)
        {
            FD_ZERO(&rdfs);
            FD_SET(fd, &rdfs);
            FD_SET(pipefd_c2p[READ_END], &rdfs);
            maxfd = MAX(fd, pipefd_c2p[READ_END]);

            /* Block until input becomes available or timeout */
            status = select(maxfd + 1, &rdfs, NULL, NULL, NULL);
            if (status < 0)
            {
                tio_warning_printf("select() failed(%s)", strerror(errno));
                break;
            }

            if (FD_ISSET(fd, &rdfs))
            {
                bytes = read(fd, buf, sizeof(buf));
                if (bytes <= 0)
                {
                    tio_warning_printf("Could not read from tty device");
                    break;
                }
                rx_total += bytes;
                write(pipefd_p2c[WRITE_END], buf, bytes);
            }

            if (FD_ISSET(pipefd_c2p[READ_END], &rdfs))
            {
                // Read pipe and transfer to tty device.
                bytes = read(pipefd_c2p[READ_END], buf, sizeof(buf));
                if (bytes < 0)
                {
                    tio_warning_printf("Could not write to tty device");
                }
                else if (bytes == 0)
                {
                    // Shell command has finished.
                    break;
                }

                if (tty_write(fd, buf, bytes) < 0)
                {
                    tio_warning_printf("Could not write to tty device");
                }
                tty_sync(fd);
            }
        }

        close(pipefd_p2c[WRITE_END]);
        close(pipefd_c2p[READ_END]);

        // Wait for the child process to finish
        waitpid(shell_command_pid, &status, 0);
        shell_command_pid = 0;

        if (WIFEXITED(status))
        {
            tio_printf("Command exited with status %d", WEXITSTATUS(status));
            return WEXITSTATUS(status);
        }
        else
        {
            tio_error_printf("Child process exited abnormally\n");
            return -1;
        }
    }
    return 0;
}

#if defined(__linux__)

void terminate_shell_command(void)
{
    // If previous shell command pid is remain, terminate it.
    if (shell_command_pid != 0)
    {
        #define PKILL_BUFSIZ 80
        char pkill_buf[PKILL_BUFSIZ] = {0};
        int bytes;
        bytes = snprintf(pkill_buf, PKILL_BUFSIZ, "/usr/bin/pkill -P %d", shell_command_pid);
        if (bytes > 0 && bytes < PKILL_BUFSIZ)
        {
            system(pkill_buf);
        }
    }
}

#endif

void clear_line()
{
    printf("\r\033[K");
}
