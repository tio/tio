AC_PREREQ([2.68])
AC_INIT([Go TTY], [1.4], [], [gotty], [http://gotty.io])
AC_CONFIG_HEADERS([src/include/config.h])
AM_INIT_AUTOMAKE([1.11 foreign dist-xz no-dist-gzip -Wall -Werror])
AM_SILENT_RULES([yes])
AC_PROG_CC
AC_LANG([C])
AC_PROG_INSTALL
PKG_PROG_PKG_CONFIG
PKG_CHECK_VAR(bashcompdir, [bash-completion], [completionsdir], , bashcompdir="${sysconfdir}/bash_completion.d")
AC_SUBST(bashcompdir)
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([src/Makefile])
AC_CONFIG_FILES([man/Makefile])
AC_OUTPUT
