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

#define _GNU_SOURCE  // For statx()
#include "config.h"
#include <dirent.h>
#include <fcntl.h>
#include <regex.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <sys/poll.h>
#include <termios.h>
#include "error.h"
#include "print.h"
#include "options.h"

bool fs_dir_exists(const char *path)
{
    struct stat st;

    if (stat(path, &st) != 0)
    {
        return false;
    }
    else if (!S_ISDIR(st.st_mode))
    {
        return false;
    }

    return true;
}

// Function to read the content of a file but stripped of newline
ssize_t fs_read_file_stripped(char *buf, size_t bufsiz, const char *format, ...)
{
    char filename[PATH_MAX];
    int bytes_printed = 0;
    va_list args;

    va_start(args, format);
    bytes_printed = vsnprintf(filename, sizeof(filename), format, args);
    va_end(args);

    if (bytes_printed < 0)
    {
        return -1;
    }

    FILE *file = fopen(filename, "r");
    if (!file)
    {
        return -1;
    }
    ssize_t length = fread(buf, 1, bufsiz - 1, file);
    if (length == -1)
    {
        fclose(file);
        return -1;
    }

    // Strip any newline
    buf[strcspn(buf, "\n")] = 0;
    buf[length] = '\0'; // Make sure to null-terminate the string

    fclose(file);

    return length;
}

bool fs_file_exists(const char *format, ...)
{
    char filename[PATH_MAX];
    int bytes_printed = 0;
    struct stat st;
    va_list args;

    va_start(args, format);
    bytes_printed = vsnprintf(filename, sizeof(filename), format, args);
    va_end(args);

    if (bytes_printed < 0)
    {
        return false;
    }

    return stat(filename, &st) == 0;
}

char* fs_search_directory(const char *dir_path, const char *dirname)
{
    struct dirent *entry;
    char path[PATH_MAX];
    struct stat st;
    DIR *dir;

    if ((dir = opendir(dir_path)) == NULL)
    {
        // Error opening directory
        return NULL;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        snprintf(path, PATH_MAX, "%s/%s", dir_path, entry->d_name);

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        if (lstat(path, &st) == -1)
        {
            // Error getting directory status
            closedir(dir);
            return NULL;
        }

        if (S_ISLNK(st.st_mode))
        {
            // Skip symbolic links
            continue;
        }

        if (S_ISDIR(st.st_mode))
        {
            // If it's a directory, check if it's the one we're looking for
            if (strcmp(entry->d_name, dirname) == 0)
            {
                char* result = malloc(strlen(path) + 1);
                if (result == NULL)
                {
                    // Error allocating memory
                    closedir(dir);
                    return NULL;
                }
                strcpy(result, path);
                closedir(dir);
                return result;
            }
            else
            {
                // Recursively search within directories
                char* result = fs_search_directory(path, dirname);
                if (result != NULL)
                {
                    closedir(dir);
                    return result;
                }
            }
        }
    }

    closedir(dir);
    return NULL;
}

// Function to return creation time of file
double fs_get_creation_time(const char *path)
{

#if defined(__APPLE__) || defined(__MACH__)
    // Use stat on macOS to access creation time
    struct stat st;

    if (stat(path, &st) != 0)
    {
        return -1;
    }

    return st.st_birthtimespec.tv_sec + st.st_birthtimespec.tv_nsec / 1e9;
#else
    struct statx stx;
    int fd = open(path, O_RDONLY);
    if (fd == -1)
    {
        return -1;
    }

    if (statx(fd, "", AT_EMPTY_PATH, STATX_ALL, &stx) != 0)
    {
        close(fd);
        return -1;
    }

    // Close the file
    close(fd);

    return stx.stx_btime.tv_sec + stx.stx_btime.tv_nsec / 1e9;
#endif
}
