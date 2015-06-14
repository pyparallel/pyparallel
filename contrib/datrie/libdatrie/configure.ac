#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(libdatrie, 0.2.8, thep@linux.thai.net)
AC_CONFIG_SRCDIR([datrie/trie.h])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE(dist-xz no-dist-gzip)

# Library versioning
# Library code modified:                    REVISION++
# Interfaces added:              CURRENT++  REVISION=0  AGE++
# Interfaces changed/removed:    CURRENT++  REVISION=0  AGE=0
LT_CURRENT=4
LT_REVISION=1
LT_AGE=3
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)

DOXYGEN_REQ_VER=1.8.4

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL

# Whether linker support --version-script option
echo '{global:hello; local:*;};' > conftest.ver
AC_LIBTOOL_LINKER_OPTION(
  [whether linker supports -version-script],
  datrie_cv_have_version_script,
  [-Wl,-version-script -Wl,conftest.ver])

AM_CONDITIONAL(LD_HAS_VERSION_SCRIPT,
               test "$datrie_cv_have_version_script" = "yes")

# Checks for libraries.

#
# Checks for iconv
#
found_iconv=no
# Check in the C library first
AC_CHECK_FUNC(iconv_open, [with_libiconv=no; found_iconv=yes])
# Check if we have GNU libiconv
if test $found_iconv = "no"; then
  AC_CHECK_LIB(iconv, libiconv_open, [with_libiconv=gnu; found_iconv=yes])
fi
# Check if we have a iconv in -liconv, possibly from vendor
if test $found_iconv = "no"; then
  AC_CHECK_LIB(iconv, iconv_open, [with_libiconv=native; found_iconv=yes])
fi
if test $found_iconv = "no"; then
  AC_MSG_ERROR([*** No usable iconv() implementation found])
fi

case $with_libiconv in
  gnu|native)
    ICONV_LIBS="-liconv"
    ;;
esac
AC_SUBST(ICONV_LIBS)

#
# Checks for locale_charset() and nl_langinfo(CODESET)
#
AC_CHECK_LIB(iconv, locale_charset,
             [have_locale_charset=yes], [have_locale_charset=no])
if test x$have_locale_charset = xyes; then
  AC_DEFINE(HAVE_LOCALE_CHARSET,1,[Have locale_charset()])
fi
AC_CACHE_CHECK(
  [for nl_langinfo (CODESET)], datrie_cv_langinfo_codeset,
  [AC_TRY_COMPILE([#include <langinfo.h>],
    [char *codeset = nl_langinfo (CODESET);],
    [datrie_cv_langinfo_codeset=yes],
    [datrie_cv_langinfo_codeset=no])]
)
if test x$datrie_cv_langinfo_codeset = xyes; then
  AC_DEFINE(HAVE_LANGINFO_CODESET,1,[Have nl_langinfo (CODESET)])
fi
if test x$have_locale_charset = xno && test x$datrie_cv_langinfo_codeset = xno
then
  AC_MSG_ERROR([*** No locale_charset() nor nl_langinfo(CODESET) found.
Please consider installing GNU libiconv.])
fi


# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([limits.h stdlib.h stdio.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T

dnl Disable doc generation with doxygen option
AC_ARG_ENABLE(doxygen-doc,
              [AC_HELP_STRING([--disable-doxygen-doc],
                              [disable document generation with doxygen])],
              , enable_doxygen_doc="yes")

if test "x$enable_doxygen_doc" = "xyes"; then
  AC_CHECK_PROG(DOXYGEN,doxygen,doxygen,no)
  if test "x$DOXYGEN" = "xno"; then
    enable_doxygen_doc="no"
  else
    AC_MSG_CHECKING([doxygen >= $DOXYGEN_REQ_VER])
    DOXYGEN_VER=$($DOXYGEN --version)
    if expr $DOXYGEN_VER \< $DOXYGEN_REQ_VER > /dev/null; then
      AC_MSG_RESULT([$DOXYGEN_VER, no, documentation disabled])
      enable_doxygen_doc="no"
    else
      AC_MSG_RESULT([$DOXYGEN_VER, yes])
    fi
  fi
fi

dnl where to install the doxygen-generated HTML doc
AC_ARG_WITH(html-docdir,
            [AC_HELP_STRING([--with-html-docdir=DIR],
                            [where to install the doxyten-generated HTML doc [PREFIX/share/doc/datrie/html]])],
            [htmldocdir="$withval"], [htmldocdir=\$\{prefix\}/share/doc/datrie/html])
AC_SUBST(htmldocdir)

AM_CONDITIONAL(ENABLE_DOXYGEN_DOC,test "x$enable_doxygen_doc" = "xyes")

# Checks for library functions.
AC_FUNC_MALLOC

AC_CONFIG_FILES([Makefile
                 datrie-0.2.pc
                 datrie/Makefile
                 tools/Makefile
                 man/Makefile
                 doc/Makefile
                 doc/Doxyfile
                 tests/Makefile])
AC_OUTPUT
