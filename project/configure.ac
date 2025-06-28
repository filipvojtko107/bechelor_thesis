#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.71])
AC_INIT([WebServerd], [1.0], [Filip Vojtko])
AM_INIT_AUTOMAKE([subdir-objects foreign])
AC_CONFIG_SRCDIR([include/Http1_1.hpp])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_CC
AC_PROG_MAKE_SET

# Checks for libraries.
AC_CHECK_LIB([pthread], [pthread_create], [], [AC_MSG_ERROR([pthread library is missing])])
AC_CHECK_LIB([systemd], [sd_notify], [], [AC_MSG_ERROR([systemd library is missing])])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h inttypes.h netinet/in.h strings.h sys/file.h sys/ioctl.h sys/socket.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MKTIME
AC_FUNC_MMAP
AC_CHECK_FUNCS([memset mkdir munmap regcomp select socket strcasecmp strerror strstr strtoull])

AC_CONFIG_FILES([Makefile])

AC_OUTPUT
