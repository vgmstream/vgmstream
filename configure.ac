dnl automake-vgmstream m4 script 
dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.53)
AC_INIT(automake-vgmstream,1.3.0)
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

have_vorbis=no
PKG_CHECK_MODULES(VORBIS, [vorbis], have_vorbis=yes,
        [AC_MSG_WARN([Cannot find libvorbis - will not enable Vorbis formats])]
)
AM_CONDITIONAL(HAVE_VORBIS, test "$have_vorbis" = yes)

have_vorbisfile=no
PKG_CHECK_MODULES(VORBISFILE, [vorbisfile], have_vorbisfile=yes,
        [AC_MSG_WARN([Cannot find libvorbisfile - will not enable Vorbis formats])]
)
AM_CONDITIONAL(HAVE_VORBISFILE, test "$have_vorbisfile" = yes)

have_libmpg123=no
PKG_CHECK_MODULES(MPG123, [libmpg123], have_libmpg123=yes,
        [AC_MSG_WARN([Cannot find libmpg123 - will not enable MPEG formats])]
)
AM_CONDITIONAL(HAVE_LIBMPG123, test "$have_libmpg123" = yes)

have_libspeex=no
PKG_CHECK_MODULES(SPEEX, [speex], have_libspeex=yes,
        [AC_MSG_WARN([Cannot find libspeex - will not enable Speex formats])]
)
AM_CONDITIONAL(HAVE_LIBSPEEX, test "$have_libspeex" = yes)

have_ffmpeg=no
PKG_CHECK_MODULES(FFMPEG, [libavformat libavcodec libavutil libswresample], have_ffmpeg=yes,
        [AC_MSG_WARN([Cannot find ffmpeg - will not enable FFmpeg formats])]
)
AM_CONDITIONAL(HAVE_FFMPEG, test "$have_ffmpeg" = yes)

have_audacious=no
PKG_CHECK_MODULES(AUDACIOUS, [audacious >= 3.5.0],  have_audacious=yes,
        [AC_MSG_WARN([Cannot find audacious >= 3.5.0 correctly installed - will not build Audacious plugin])]
)
AM_CONDITIONAL(HAVE_AUDACIOUS, test "$have_audacious" = yes)

PKG_CHECK_MODULES(GTK, [glib-2.0 >= 2.6.0 gtk+-2.0 >= 2.6.0 gthread-2.0 pango],
    , [AC_MSG_ERROR([Cannot find glib2/gtk2/pango])]
)

have_libao=no
PKG_CHECK_MODULES(AO, [ao >= 1.1.0], have_libao=yes,
        [AC_MSG_WARN([Cannot find libao - will not build vgmstream123])])
AM_CONDITIONAL(HAVE_LIBAO, test "$have_libao" = yes)

have_jansson=no
PKG_CHECK_MODULES(JANSSON, [jansson >= 2.3], have_jansson=yes,
        [AC_MSG_WARN([Cannot find jansson - will disable JSON dumping in CLI])])
AM_CONDITIONAL(HAVE_JANSSON, test "$have_jansson" = yes)

if test "_$GCC" = _yes
then
  CFLAGS="$CFLAGS -Wall -Wextra -Wno-sign-compare -Wno-unused-parameter -Wno-unused-but-set-variable"
fi

#plugindir=`pkg-config audacious --define-variable=prefix="$prefix" --variable=plugin_dir`
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
  audacious/Makefile.autotools
  cli/Makefile.autotools
])
