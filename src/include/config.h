/* src/include/config.h.  Generated from config.h.in by configure.  */
/* src/include/config.h.in.  Generated from configure.ac by autoheader.  */

/* Switch cases for detected baud rates */
#define AUTOCONF_BAUDRATE_CASES case 0: baudrate = B0; break; case 50: baudrate = B50; break; case 75: baudrate = B75; break; case 110: baudrate = B110; break; case 134: baudrate = B134; break; case 150: baudrate = B150; break; case 200: baudrate = B200; break; case 300: baudrate = B300; break; case 600: baudrate = B600; break; case 1200: baudrate = B1200; break; case 1800: baudrate = B1800; break; case 2400: baudrate = B2400; break; case 4800: baudrate = B4800; break; case 9600: baudrate = B9600; break; case 19200: baudrate = B19200; break; case 38400: baudrate = B38400; break; case 57600: baudrate = B57600; break; case 115200: baudrate = B115200; break; case 230400: baudrate = B230400; break; case 460800: baudrate = B460800; break; case 500000: baudrate = B500000; break; case 576000: baudrate = B576000; break; case 921600: baudrate = B921600; break; case 1000000: baudrate = B1000000; break; case 1152000: baudrate = B1152000; break; case 1500000: baudrate = B1500000; break; case 2000000: baudrate = B2000000; break; case 2500000: baudrate = B2500000; break; case 3000000: baudrate = B3000000; break; case 3500000: baudrate = B3500000; break; case 4000000: baudrate = B4000000; break;

/* Define if termios2 exists. */
#define HAVE_TERMIOS2 1

/* Name of package */
#define PACKAGE "tio"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME "tio"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "tio 1.32"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "tio"

/* Define to the home page for this package. */
#define PACKAGE_URL "https://tio.github.io"

/* Define to the version of this package. */
#define PACKAGE_VERSION "1.32"

/* Version number of package */
#define VERSION "1.32"

/* Enable large inode numbers on Mac OS X 10.5.  */
#ifndef _DARWIN_USE_64_BIT_INODE
# define _DARWIN_USE_64_BIT_INODE 1
#endif

/* Number of bits in a file offset, on hosts where this is settable. */
/* #undef _FILE_OFFSET_BITS */

/* Define for large files, on AIX-style hosts. */
/* #undef _LARGE_FILES */
