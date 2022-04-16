#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

m4_define(LIBPOSTAL_MAJOR_VERSION, [1])
m4_define(LIBPOSTAL_MINOR_VERSION, [1])
m4_define(LIBPOSTAL_PATCH_VERSION, [0])

AC_INIT([libpostal], LIBPOSTAL_MAJOR_VERSION.LIBPOSTAL_MINOR_VERSION.LIBPOSTAL_PATCH_VERSION)

AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_CONFIG_SRCDIR([src])
LT_INIT([win32-dll])

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

AC_CONFIG_FILES([Makefile
                 libpostal.pc
                 src/Makefile
                 src/libpostal_data
                 test/Makefile],  [chmod +x src/libpostal_data])

AC_CHECK_PROG([FOUND_SHUF], [shuf], [yes])
AC_CHECK_PROG([FOUND_GSHUF], [gshuf], [yes])

AS_IF([test "x$FOUND_SHUF" = xyes],  [AC_DEFINE([HAVE_SHUF], [1], [shuf available])])
AS_IF([test "x$FOUND_GSHUF" = xyes],  [AC_DEFINE([HAVE_GSHUF], [1], [gshuf available])])

# ------------------------------------------------------------------
# Architecture-specific options
# ------------------------------------------------------------------

# allow enabling hardware optimization on any system:
case "$host_cpu" in
   arm*|aarch64*)
     enable_arm_neon=yes
     enable_intel_sse=no
     AC_DEFINE([ARM_NEON], [1],
       [Enable ARM_NEON optimizations])
     ;;
   i?86|x86_64)
     enable_intel_sse=yes
     enable_arm_neon=no
     AC_DEFINE([INTEL_SSE], [1],
       [Enable Intel SSE optimizations])
     ;;
esac

AC_ARG_ENABLE([hardware-optimizations],
   AS_HELP_STRING([[[--disable-hardware-optimizations]]],
      [Disable hardware optimizations (Intel SSE2 / ARM NEON)]),
   [
     # disable hardware optimization on all systems:
     enable_arm_neon=no
     AC_DEFINE([ARM_NEON], [0],
       [Disable ARM_NEON optimizations])
     enable_intel_sse=no
     AC_DEFINE([INTEL_SSE], [0],
       [Disable INTEL_SSE optimizations])
   ])

# INTEL
# =====
#
# INTEL SSE (SIMD) support.

AC_ARG_ENABLE([intel-sse],
  AS_HELP_STRING([[[--enable-intel-sse]]],
     [Enable Intel SSE optimizations: =no/off, yes/on:]
     [no/off: disable the optimizations;]
     [yes/on: enable the optimizations.]
     [If not specified: determined by the compiler.]),
  [case "$enableval" in
     no|off)
        # disable the default enabling:
        AC_DEFINE([INTEL_SSE], [0],
                  [Disable Intel SSE optimizations])
        # Prevent inclusion of the assembler files below:
        enable_intel_sse=no;;
     yes|on)
        enable_intel_sse=yes
        AC_DEFINE([INTEL_SSE], [1],
                  [Enable Intel SSE optimizations]);;
     *)
        AC_MSG_ERROR([--enable-intel-sse=${enable_intel_sse}: invalid value])
  esac])

# Add Intel specific files to all builds where the host_cpu is Intel ('x86*')
# or where Intel optimizations were explicitly requested (this allows a
# fallback if a future host CPU does not match 'x86*')
AM_CONDITIONAL([INTEL_SSE],
  [test "$enable_intel_sse" != 'no' &&
   case "$host_cpu" in
     i?86|x86_64) :;;
     *)    test "$enable_intel_sse" != '';;
   esac])

# ARM
# ===
#
# ARM NEON (SIMD) support.

AC_ARG_ENABLE([arm-neon],
  AS_HELP_STRING([[[--enable-arm-neon]]],
     [Enable ARM NEON optimizations: =no/off, check, api, yes/on:]
     [no/off: disable the optimizations; check: use internal checking code]
     [(deprecated and poorly supported); api: disable by default, enable by]
     [a call to png_set_option; yes/on: turn on unconditionally.]
     [If not specified: determined by the compiler.]),
  [case "$enableval" in
     no|off)
        # disable the default enabling on __ARM_NEON__ systems:
        AC_DEFINE([ARM_NEON], [0],
                  [Disable ARM Neon optimizations])
        # Prevent inclusion of the assembler files below:
        enable_arm_neon=no;;
     yes|on)
        enable_arm_neon=yes
        AC_DEFINE([ARM_NEON], [1],
                  [Enable ARM Neon optimizations]);;
     *)
        AC_MSG_ERROR([--enable-arm-neon=${enable_arm_neon}: invalid value])
  esac])

# Add ARM specific files to all builds where the host_cpu is arm ('arm*') or
# where ARM optimizations were explicitly requested (this allows a fallback if a
# future host CPU does not match 'arm*')

AM_CONDITIONAL([ARM_NEON],
  [test "$enable_arm_neon" != 'no' &&
   case "$host_cpu" in
     arm*|aarch64*) :;;
     *)    test "$enable_arm_neon" != '';;
   esac])

SIMDFLAGS=""

AS_IF([test "x$enable_intel_sse" != "xno"], [
  SIMDFLAGS="-mfpmath=sse -msse2 -DINTEL_SSE"
])

AS_IF([test "x$enable_arm_neon" != "xno"], [
  SIMDFLAGS="-march=armv8-a+fp+simd+crypto+crc -DARM_NEON"
])

CFLAGS="${SIMDFLAGS} ${CFLAGS}"

AC_SUBST([SIMDFLAGS], [$SIMDFLAGS])

AC_CHECK_HEADER(cblas.h, [AX_CBLAS])

AC_ARG_ENABLE([data-download],
              [  --disable-data-download    Disable downloading data],
              [case "${enableval}" in
                yes)  DOWNLOAD_DATA=true ;;
                no) DOWNLOAD_DATA=false ;;
                *) AC_MSG_ERROR([bad value ${enableval} for --disable-data-download]) ;;
              esac], [DOWNLOAD_DATA=true])

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
