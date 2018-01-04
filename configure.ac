dnl audacious-vgmstream m4 script 
dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.53)
AC_INIT(audacious-vgmstream,1.3.0)
AM_INIT_AUTOMAKE([subdir-objects])
AC_CONFIG_HEADERS(audacious/config.h)
AM_DISABLE_STATIC

AC_PROG_CC
AC_PROG_CXX
AM_PROG_CC_STDC
AC_HEADER_STDC
AC_PROG_INSTALL
AM_PROG_LIBTOOL

AC_PATH_X
AC_PATH_XTRA

PKG_CHECK_MODULES(AUDACIOUS, [audacious >= 3.5.0],,
        [AC_MSG_ERROR([Cannot find audacious >= 3.5.0 correctly installed])]
)

PKG_CHECK_MODULES(VORBIS, [vorbis],,
        [AC_MSG_ERROR([Cannot find libvorbis])]
)

PKG_CHECK_MODULES(VORBISFILE, [vorbisfile],,
        [AC_MSG_ERROR([Cannot find libvorbisfile])]
)

PKG_CHECK_MODULES(MPG123, [libmpg123],,
        [AC_MSG_ERROR([Cannot find libmpg123])]
)

PKG_CHECK_MODULES(GTK, [glib-2.0 >= 2.6.0 gtk+-2.0 >= 2.6.0 gthread-2.0 pango],
    , [AC_MSG_ERROR([Cannot find glib2/gtk2/pango])]
)

have_libao=no
PKG_CHECK_MODULES(AO, [ao >= 1.1.0], have_libao=yes,
        [AC_MSG_WARN([Cannot find libao - will not build vgmstream123])])
AM_CONDITIONAL(HAVE_LIBAO, test "$have_libao" = yes)

if test "_$GCC" = _yes
then
  CFLAGS="$CFLAGS -Wall -Wextra -Wno-sign-compare -Wno-unused-parameter -Wno-unused-but-set-variable"
fi

plugindir=`pkg-config audacious --variable=plugin_dir`
AC_SUBST(plugindir)

INPUT_PLUGIN_DIR="Input"
AC_SUBST(INPUT_PLUGIN_DIR)

#AC_CHECK_HEADERS(regex.h,,AC_MSG_ERROR(regex.h missing))
#AC_CHECK_FUNCS(regcomp regexec regfree)

AC_PATH_X
AC_PATH_XTRA

AC_OUTPUT([
  Makefile.autotools
  src/Makefile.autotools
  src/coding/Makefile.autotools
  src/layout/Makefile.autotools
  src/meta/Makefile.autotools
  audacious/Makefile.autotools
  test/Makefile.autotools
])
