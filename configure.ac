#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([libpthread_workqueue], [0.9.2])
AC_CONFIG_AUX_DIR([.])
LT_INIT
AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_PROG_CC([clang gcc cc])
AC_CONFIG_FILES([Makefile])
AM_CONDITIONAL([LINUX], [test `uname` = 'Linux'])
AC_OUTPUT
