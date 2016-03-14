#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([libpthread_workqueue], [0.9.4])
AC_CONFIG_AUX_DIR([.])
LT_INIT
AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_HEADERS([config.h])
AC_PROG_CC
AC_CONFIG_MACRO_DIR([m4])

# When building as submodule allow selection of noinst targets
AC_ARG_ENABLE([libpwq-install],
  [AS_HELP_STRING([--disable-libpwq-install],
    [Disable install rules and build noinst libtool library])],,
  [enable_libpwq_install=yes]
)
AM_CONDITIONAL([INSTALL],[test "x$enable_libpwq_install" != "xno"])

AC_CONFIG_FILES([Makefile])
AM_CONDITIONAL([LINUX], [test `uname` = 'Linux'])
AC_OUTPUT
