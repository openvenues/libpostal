#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

m4_define(LIBPOSTAL_MAJOR_VERSION, [1])
m4_define(LIBPOSTAL_MINOR_VERSION, [1])
m4_define(LIBPOSTAL_PATCH_VERSION, [0])

AC_INIT([libpostal], LIBPOSTAL_MAJOR_VERSION.LIBPOSTAL_MINOR_VERSION.LIBPOSTAL_PATCH_VERSION)

AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_CONFIG_SRCDIR([src])
LT_INIT([shared])

AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC_C99
AC_PROG_INSTALL

LDFLAGS="$LDFLAGS -L/usr/local/lib"

# Checks for libraries.
AC_SEARCH_LIBS([log],
  [m],,[AC_MSG_ERROR([Could not find math library])])

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_TIME
AC_HEADER_DIRENT
AC_HEADER_STDBOOL
AC_CHECK_HEADERS([fcntl.h float.h inttypes.h limits.h locale.h malloc.h memory.h stddef.h stdint.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
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
AC_CHECK_FUNCS([malloc realloc drand48 getcwd gettimeofday memmove memset regcomp setlocale sqrt strdup strndup])

AC_SUBST([LIBPOSTAL_DATA_DIR_VERSION_STRING], [v1])

DATA_FILE_LATEST_VERSION=$(cat $srcdir/versions/base_data)
PARSER_MODEL_LATEST_VERSION=$(cat $srcdir/versions/parser)
LANG_CLASS_MODEL_LATEST_VERSION=$(cat $srcdir/versions/language_classifier)

AC_SUBST([LIBPOSTAL_DATA_FILE_LATEST_VERSION], [$DATA_FILE_LATEST_VERSION])
AC_SUBST([LIBPOSTAL_PARSER_MODEL_LATEST_VERSION], [$PARSER_MODEL_LATEST_VERSION])
AC_SUBST([LIBPOSTAL_LANG_CLASS_MODEL_LATEST_VERSION], [$LANG_CLASS_MODEL_LATEST_VERSION])

# Senzing data
AC_SUBST([LIBPOSTAL_SENZING_DATA_DIR_VERSION_STRING], [v1])

SENZING_DATA_FILE_LATEST_VERSION=$(cat $srcdir/versions/senzing/base_data)
SENZING_PARSER_MODEL_LATEST_VERSION=$(cat $srcdir/versions/senzing/parser)
SENZING_LANG_CLASS_MODEL_LATEST_VERSION=$(cat $srcdir/versions/senzing/language_classifier)

AC_SUBST([LIBPOSTAL_SENZING_DATA_FILE_LATEST_VERSION], [$SENZING_DATA_FILE_LATEST_VERSION])
AC_SUBST([LIBPOSTAL_SENZING_PARSER_MODEL_LATEST_VERSION], [$SENZING_PARSER_MODEL_LATEST_VERSION])
AC_SUBST([LIBPOSTAL_SENZING_LANG_CLASS_MODEL_LATEST_VERSION], [$SENZING_LANG_CLASS_MODEL_LATEST_VERSION])

AC_CONFIG_FILES([Makefile
                 libpostal.pc
                 src/Makefile
                 src/libpostal_data
                 test/Makefile], [chmod +x src/libpostal_data])

AC_CHECK_PROG([FOUND_SHUF], [shuf], [yes])
AC_CHECK_PROG([FOUND_GSHUF], [gshuf], [yes])

AS_IF([test "x$FOUND_SHUF" = xyes],  [AC_DEFINE([HAVE_SHUF], [1], [shuf available])])
AS_IF([test "x$FOUND_GSHUF" = xyes],  [AC_DEFINE([HAVE_GSHUF], [1], [gshuf available])])

# ------------------------------------------------------------------
# Checks for SSE2 build
# ------------------------------------------------------------------

AC_ARG_ENABLE([sse2],
    AS_HELP_STRING(
        [--disable-sse2],
        [disable SSE2 optimization routines]
        )
    )

AS_IF([test "x$enable_sse2" != "xno" && test "x$(uname -m)" != "xarm64"], [
    CFLAGS="-mfpmath=sse -msse2 -DUSE_SSE ${CFLAGS}"
])

AC_CHECK_HEADER(cblas.h, [AX_CBLAS])

AC_ARG_ENABLE([data-download],
              [  --disable-data-download    Disable downloading data],
              [case "${enableval}" in
                yes)  DOWNLOAD_DATA=true ;;
                no) DOWNLOAD_DATA=false ;;
                *) AC_MSG_ERROR([bad value ${enableval} for --disable-data-download]) ;;
              esac], [DOWNLOAD_DATA=true])

AC_ARG_VAR(MODEL, [Option to use alternative data models. Currently available is "senzing" (MODEL=senzing). If this option is not set the default libpostal data model is used.])
AS_VAR_IF([MODEL], [], [],
  [AS_VAR_IF([MODEL], [senzing], [], [AC_MSG_FAILURE([Invalid MODEL value set])])])

AM_CONDITIONAL([DOWNLOAD_DATA], [test "x$DOWNLOAD_DATA" = "xtrue"])

AC_ARG_WITH(cflags-scanner-extra, [AS_HELP_STRING([--with-cflags-scanner-extra@<:@=VALUE@:>@], [Extra compilation options for scanner.c])],
[
 if test "x$withval" = "xno"; then
    CFLAGS_SCANNER_EXTRA=""
 else
   CFLAGS_SCANNER_EXTRA="$withval"
 fi
],
[ CFLAGS_SCANNER_EXTRA="" ]
)

AC_MSG_NOTICE([extra cflags for scanner.c: $CFLAGS_SCANNER_EXTRA])
AC_SUBST(CFLAGS_SCANNER_EXTRA)
AC_SUBST(LIBPOSTAL_SO_VERSION, LIBPOSTAL_MAJOR_VERSION:LIBPOSTAL_MINOR_VERSION:LIBPOSTAL_PATCH_VERSION)

AC_OUTPUT
