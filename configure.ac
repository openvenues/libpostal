#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([libpostal], [0.1])
AM_INIT_AUTOMAKE([foreign subdir-objects])
LT_PREREQ([2.2])
LT_INIT([shared])
AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC_C99
AC_PROG_INSTALL

# Checks for libraries.
AC_SEARCH_LIBS([snappy_compress],
  [snappy],,[AC_MSG_ERROR([Could not find snappy])
])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h float.h inttypes.h limits.h locale.h malloc.h memory.h stddef.h stdint.h stdlib.h string.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_CHECK_TYPES([ptrdiff_t])

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_CHECK_FUNCS([getcwd gettimeofday memmove memset munmap regcomp setlocale sqrt strdup strndup])

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 src/sparkey/Makefile])

AM_CONDITIONAL([HAVE_DATE_STAT], [date -r . >/dev/null 2>&1])
AM_CONDITIONAL([HAVE_STAT], [stat -f %Sm . >/dev/null 2>&1])

AC_OUTPUT
