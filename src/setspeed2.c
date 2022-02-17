/*
 * tio - a simple TTY terminal I/O tool
 *
 * Copyright (c) 2017-2022  Martin Lund
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

#define termios asmtermios
#include <sys/ioctl.h>
#undef termios
#include <asm-generic/ioctls.h>
#include <asm-generic/termbits.h>

int setspeed2(int fd, int baudrate)
{
    struct termios2 tio;
    int status;

    status = ioctl(fd, TCGETS2, &tio);

    // Set baudrate speed using termios2 interface
    tio.c_cflag &= ~CBAUD;
    tio.c_cflag |= BOTHER;
    tio.c_ispeed = baudrate;
    tio.c_ospeed = baudrate;

    status = ioctl(fd, TCSETS2, &tio);

    return status;
}
