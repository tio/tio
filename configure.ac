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

# Check for available terminal I/O speeds
AC_CHECK_DECL([B57600], [AC_SUBST([B57600],["57600"])], [], [[#include <termios.h>]])
AC_CHECK_DECL([B115200], [AC_SUBST([B115200],["115200"])], [], [[#include <termios.h>]])
AC_CHECK_DECL([B230400], [AC_SUBST([B230400],["230400"])], [], [[#include <termios.h>]])
AC_CHECK_DECL([B460800], [AC_SUBST([B460800],["460800"])], [], [[#include <termios.h>]])
AC_CHECK_DECL([B500000], [AC_SUBST([B500000],["500000"])], [], [[#include <termios.h>]])
AC_CHECK_DECL([B576000], [AC_SUBST([B576000],["576000"])], [], [[#include <termios.h>]])
AC_CHECK_DECL([B921600], [AC_SUBST([B921600],["921600"])], [], [[#include <termios.h>]])
AC_CHECK_DECL([B1000000], [AC_SUBST([B1000000],["1000000"])], [], [[#include <termios.h>]])
AC_CHECK_DECL([B1152000], [AC_SUBST([B1152000],["1152000"])], [], [[#include <termios.h>]])
AC_CHECK_DECL([B1500000], [AC_SUBST([B1500000],["1500000"])], [], [[#include <termios.h>]])
AC_CHECK_DECL([B2000000], [AC_SUBST([B2000000],["2000000"])], [], [[#include <termios.h>]])
AC_CHECK_DECL([B2500000], [AC_SUBST([B2500000],["2500000"])], [], [[#include <termios.h>]])
AC_CHECK_DECL([B3000000], [AC_SUBST([B3000000],["3000000"])], [], [[#include <termios.h>]])
AC_CHECK_DECL([B3500000], [AC_SUBST([B3500000],["3500000"])], [], [[#include <termios.h>]])
AC_CHECK_DECL([B4000000], [AC_SUBST([B4000000],["4000000"])], [], [[#include <termios.h>]])

AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([src/Makefile])
AC_CONFIG_FILES([man/Makefile])
AC_OUTPUT
