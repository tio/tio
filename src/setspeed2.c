#include <sys/ioctl.h>
#include <asm/termbits.h>

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
