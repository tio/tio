AC_PREREQ([2.68])
AC_INIT([tio], [1.17], [], [tio], [https://tio.github.io])
AC_CONFIG_HEADERS([src/include/config.h])
AM_INIT_AUTOMAKE([1.11 foreign dist-xz no-dist-gzip -Wall -Werror])
AM_SILENT_RULES([yes])
AC_PROG_CC
AC_LANG([C])
AC_PROG_INSTALL
AC_SYS_LARGEFILE
PKG_PROG_PKG_CONFIG

# Handle bash completion
AC_ARG_WITH([bash-completion-dir],
    AS_HELP_STRING([--with-bash-completion-dir[=PATH]],
                   [Install the bash auto-completion script in this directory. @<:@default=yes@:>@]),
                   [],
                   [with_bash_completion_dir=yes])

if test "x$with_bash_completion_dir" = "xyes"; then
    PKG_CHECK_MODULES([BASH_COMPLETION], [bash-completion >= 2.0],
        [BASH_COMPLETION_DIR="`pkg-config --variable=completionsdir bash-completion`"],
        [BASH_COMPLETION_DIR="${sysconfdir}/bash_completion.d"])
else
    BASH_COMPLETION_DIR="$with_bash_completion_dir"
fi
AC_SUBST([BASH_COMPLETION_DIR])
AM_CONDITIONAL([ENABLE_BASH_COMPLETION],[test "x$with_bash_completion_dir" != "xno"])

# TIO_CHECK_BAUDRATE(N)
AC_DEFUN(
    [TIO_CHECK_BAUDRATE],
    [
        tio_have_decl=0
        AS_IF([test $1 -le 38400],
            # Baud rates up to 38400 are defined by POSIX,
            # so we don't have to check for them.
            [tio_have_decl=1],
            [AC_CHECK_DECLS([B$1], [tio_have_decl=1], [], [[#include <termios.h>]])]
        )
        AS_IF([test $tio_have_decl = 1], [AC_SUBST([BAUDRATES], ["$BAUDRATES $1"])])
    ]
)

# TIO_CHECK_BAUDRATES(N1, N2, ...)
AC_DEFUN(
    [TIO_CHECK_BAUDRATES],
    [m4_foreach([n], [$@], [TIO_CHECK_BAUDRATE(m4_normalize(n))])]
)

# Check for available terminal I/O speeds
BAUDRATES=
TIO_CHECK_BAUDRATES(
    0,
    50,
    75,
    110,
    134,
    150,
    200,
    300,
    600,
    1200,
    1800,
    2400,
    4800,
    9600,
    19200,
    38400,
    57600,
    115200,
    230400,
    460800,
    500000,
    576000,
    921600,
    1000000,
    1152000,
    1500000,
    2000000,
    2500000,
    3000000,
    3500000,
    4000000
)

AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([src/Makefile])
AC_CONFIG_FILES([src/bash-completion/tio])
AC_CONFIG_FILES([man/Makefile])
AC_OUTPUT
