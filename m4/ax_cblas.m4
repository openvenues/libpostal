# ===========================================================================
#                http://autoconf-archive.cryp.to/acx_blas.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_CBLAS([ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]])
#
# DESCRIPTION
#
#   This macro looks for a library that implements the CBLAS linear-algebra
#   interface (see http://www.netlib.org/blas/). On success, it sets the
#   CBLAS_LIBS output variable to hold the requisite library linkages.
#
#   To link with CBLAS, you should link with:
#
#       $CBLAS_LIBS $LIBS
#
#   in that order.
#
#   Many libraries are searched for, from ATLAS to CXML to ESSL. The user
#   may also use --with-cblas=<lib> in order to use some specific CBLAS
#   library <lib>.
#
#   ACTION-IF-FOUND is a list of shell commands to run if a BLAS library is
#   found, and ACTION-IF-NOT-FOUND is a list of commands to run it if it is
#   not found. If ACTION-IF-FOUND is not specified, the default action will
#   define HAVE_BLAS.
#
#   This macro requires autoconf 2.50 or later.
#
# LAST MODIFICATION
#
#   2008-12-29
#
# COPYLEFT
#
#   Copyright (c) 2008 Patrick O. Perry <patperry@stanfordalumni.org>
#   Copyright (c) 2008 Steven G. Johnson <stevenj@alum.mit.edu>
#
#   This program is free software: you can redistribute it and/or modify it
#   under the terms of the GNU General Public License as published by the
#   Free Software Foundation, either version 3 of the License, or (at your
#   option) any later version.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
#   Public License for more details.
#
#   You should have received a copy of the GNU General Public License along
#   with this program. If not, see <http://www.gnu.org/licenses/>.
#
#   As a special exception, the respective Autoconf Macro's copyright owner
#   gives unlimited permission to copy, distribute and modify the configure
#   scripts that are the output of Autoconf when processing the Macro. You
#   need not follow the terms of the GNU General Public License when using
#   or distributing such scripts, even though portions of the text of the
#   Macro appear in them. The GNU General Public License (GPL) does govern
#   all other use of the material that constitutes the Autoconf Macro.
#
#   This special exception to the GPL applies to versions of the Autoconf
#   Macro released by the Autoconf Macro Archive. When you make and
#   distribute a modified version of the Autoconf Macro, you may extend this
#   special exception to the GPL to apply to your modified version as well.

AC_DEFUN([AX_CBLAS], [
AC_PREREQ(2.50)
ax_cblas_ok=no

AC_ARG_WITH(cblas,
    [AC_HELP_STRING([--with-cblas=<lib>], [use CBLAS library <lib>])])
case $with_cblas in
    yes | "") ;;
    no) ax_cblas_ok=disable ;;
    -* | */* | *.a | *.so | *.so.* | *.o) CBLAS_LIBS="$with_cblas" ;;
    *) CBLAS_LIBS="-l$with_cblas" ;;
esac

ax_cblas_save_LIBS="$LIBS"

# First, check CBLAS_LIBS environment variable
if test $ax_cblas_ok = no; then
if test "x$CBLAS_LIBS" != x; then
    save_LIBS="$LIBS"; LIBS="$CBLAS_LIBS $LIBS"
    AC_MSG_CHECKING([for cblas_dgemm in $CBLAS_LIBS])
    AC_TRY_LINK_FUNC(cblas_dgemm, [ax_cblas_ok=yes], [CBLAS_LIBS=""])
    AC_MSG_RESULT($ax_cblas_ok)
    LIBS="$save_LIBS"
fi
fi

# CBLAS linked to by default?  (happens on some supercomputers)
if test $ax_cblas_ok = no; then
    save_LIBS="$LIBS"; LIBS="$LIBS"
    AC_CHECK_FUNC(cblas_dgemm, [ax_cblas_ok=yes])
    LIBS="$save_LIBS"
fi

# BLAS in ATLAS library? (http://math-atlas.sourceforge.net/)
if test $ax_cblas_ok = no; then
    AC_CHECK_LIB(atlas, ATL_xerbla,
        [AC_CHECK_LIB(cblas, cblas_dgemm,
            [ax_cblas_ok=yes
             CBLAS_LIBS="-lcblas -latlas"],
            [], [-latlas])])
fi

# BLAS in Intel MKL library?
if test $ax_cblas_ok = no; then
    AC_CHECK_LIB(mkl, cblas_dgemm, [ax_cblas_ok=yes;CBLAS_LIBS="-lmkl"])
fi

# BLAS in Apple vecLib library?
if test $ax_cblas_ok = no; then
    save_LIBS="$LIBS"; LIBS="-framework vecLib $LIBS"
    AC_CHECK_FUNC(cblas_dgemm, [ax_cblas_ok=yes;CBLAS_LIBS="-framework vecLib"])
    LIBS="$save_LIBS"
fi

# BLAS in Alpha DXML library? (now called CXML, see above)
if test $ax_cblas_ok = no; then
    AC_CHECK_LIB(dxml, cblas_dgemm, [ax_cblas_ok=yes;CBLAS_LIBS="-ldxml"])
fi

# BLAS in Sun Performance library?
if test $ax_cblas_ok = no; then
    if test "x$GCC" != xyes; then # only works with Sun CC
        AC_CHECK_LIB(sunmath, acosp,
            [AC_CHECK_LIB(sunperf, cblas_dgemm,
                   [CBLAS_LIBS="-xlic_lib=sunperf -lsunmath"
                                 ax_cblas_ok=yes],[],[-lsunmath])])
    fi
fi

# BLAS in SCSL library?  (SGI/Cray Scientific Library)
if test $ax_cblas_ok = no; then
    AC_CHECK_LIB(scs, cblas_dgemm, [ax_cblas_ok=yes; CBLAS_LIBS="-lscs"])
fi

# BLAS in SGIMATH library?
if test $ax_cblas_ok = no; then
    AC_CHECK_LIB(complib.sgimath, cblas_dgemm,
             [ax_cblas_ok=yes; CBLAS_LIBS="-lcomplib.sgimath"])
fi

# BLAS in IBM ESSL library? (requires generic BLAS lib, too)
if test $ax_cblas_ok = no; then
    AC_CHECK_LIB(blas, cblas_dgemm,
        [AC_CHECK_LIB(essl, cblas_dgemm,
            [ax_cblas_ok=yes; CBLAS_LIBS="-lessl -lblas"],
            [], [-lblas])])
fi

# BLAS in OpenBLAS library?
if test $ax_cblas_ok = no; then
    AC_CHECK_LIB(openblas, cblas_dgemm, [ax_cblas_ok=yes; CBLAS_LIBS="-lopenblas"])
fi

# Generic CBLAS library?
if test $ax_cblas_ok = no; then
    AC_CHECK_LIB(cblas, cblas_dgemm, [ax_cblas_ok=yes; CBLAS_LIBS="-lcblas"])
fi

# Generic BLAS library?
if test $ax_cblas_ok = no; then
    AC_CHECK_LIB(blas, cblas_dgemm, [ax_cblas_ok=yes; CBLAS_LIBS="-lblas"])
fi

AC_SUBST(CBLAS_LIBS)

LIBS="$ax_cblas_save_LIBS"

# Finally, execute ACTION-IF-FOUND/ACTION-IF-NOT-FOUND:
if test x"$ax_cblas_ok" = xyes; then
        ifelse([$1],,AC_DEFINE(HAVE_CBLAS,1,[Define if you have a CBLAS library.]),[$1])
        :
else
        ax_cblas_ok=no
        $2
fi
])dnl AX_CBLAS
