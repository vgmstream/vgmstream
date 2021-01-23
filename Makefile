###############################
# vgmstream makefile
###############################

###############################################################################
### external defs
# currently aimed to WIN32 builds but vgmstream_cli should work for others (or use autotools instead)
export TARGET_OS = $(OS)

#for Win builds with vgmstream123
LIBAO_DLL_PATH = ../libao/bin

### tools
RMF = rm -f

ifeq ($(TARGET_OS),Windows_NT)
  SHELL = sh
  CC = gcc
  AR = ar
  STRIP = strip
  WINDRES = windres
  DLLTOOL = dlltool

  # same thing, the above should be available
  #CC = i686-w64-mingw32-gcc
  #AR = i686-w64-mingw32-ar
  #STRIP = i686-w64-mingw32-strip
  #WINDRES = i686-w64-mingw32-windres
  #DLLTOOL = i686-w64-mingw32-dlltool

else
  SHELL = /bin/sh
  CC = gcc
  AR = ar
  STRIP = strip
  WINDRES =
  DLLTOOL =

  # (old crosscompile, not used anymore?)
  #CC = i586-mingw32msvc-gcc
  #AR = i586-mingw32msvc-ar
  #STRIP = i586-mingw32msvc-strip
  #WINDRES = i586-mingw32msvc-windres
  #DLLTOOL = i586-mingw32msvc-dlltool

endif
export RMF SHELL CC AR STRIP WINDRES DLLTOOL


###############################################################################
### build defs

DEF_CFLAGS = -ffast-math -O3 -Wall -Werror=format-security -Wdeclaration-after-statement -Wvla -Wimplicit-function-declaration -Wignored-qualifiers
#ifdef VGM_DEBUG
#  CFLAGS += -DVGM_DEBUG_OUTPUT -O0
#  CFLAGS += -Wold-style-definition -Woverflow -Wpointer-arith -Wstrict-prototypes -pedantic -std=gnu90 -fstack-protector -Wformat
#endif

LIBS_CFLAGS=
LIBS_LDFLAGS=
LIBS_TARGET_EXT_LIBS=

# config libs
VGM_ENABLE_G7221 = 1
ifeq ($(VGM_ENABLE_G7221),1) 
  LIBS_CFLAGS  += -DVGM_USE_G7221
endif


### external libs
ifeq ($(TARGET_OS),Windows_NT)

VGM_ENABLE_VORBIS = 1
ifeq ($(VGM_ENABLE_VORBIS),1)
  LIBS_CFLAGS  += -DVGM_USE_VORBIS
  LIBS_LDFLAGS += -lvorbis
  LIBS_TARGET_EXT_LIBS += libvorbis.a
endif

VGM_ENABLE_MPEG = 1
ifeq ($(VGM_ENABLE_MPEG),1)
  LIBS_CFLAGS  += -DVGM_USE_MPEG
  LIBS_LDFLAGS += -lmpg123-0
  LIBS_TARGET_EXT_LIBS += libmpg123-0.a
endif

VGM_ENABLE_G719 = 1
ifeq ($(VGM_ENABLE_G719),1) 
  LIBS_CFLAGS  += -DVGM_USE_G719
  LIBS_LDFLAGS += -lg719_decode
  LIBS_TARGET_EXT_LIBS += libg719_decode.a
endif

VGM_ENABLE_MAIATRAC3PLUS = 0
ifeq ($(VGM_ENABLE_MAIATRAC3PLUS),1) 
  LIBS_CFLAGS  += -DVGM_USE_MAIATRAC3PLUS
  LIBS_LDFLAGS += -lat3plusdecoder
  LIBS_TARGET_EXT_LIBS += libat3plusdecoder.a
endif

VGM_ENABLE_FFMPEG = 1
ifeq ($(VGM_ENABLE_FFMPEG),1)
  LIBS_CFLAGS  += -DVGM_USE_FFMPEG
  LIBS_LDFLAGS += -lavcodec -lavformat -lavutil -lswresample
  LIBS_TARGET_EXT_LIBS += libavcodec.a libavformat.a libavutil.a libswresample.a
endif

VGM_ENABLE_ATRAC9 = 1
ifeq ($(VGM_ENABLE_ATRAC9),1) 
  LIBS_CFLAGS  += -DVGM_USE_ATRAC9
  LIBS_LDFLAGS += -latrac9
  LIBS_TARGET_EXT_LIBS += libatrac9.a
endif

VGM_ENABLE_CELT = 1
ifeq ($(VGM_ENABLE_CELT),1) 
  LIBS_CFLAGS  += -DVGM_USE_CELT
  LIBS_LDFLAGS += -lcelt-0061 -lcelt-0110
  LIBS_TARGET_EXT_LIBS += libcelt-0061.a libcelt-0110.a
endif

VGM_ENABLE_SPEEX = 1
ifeq ($(VGM_ENABLE_SPEEX),1) 
  LIBS_CFLAGS  += -DVGM_USE_SPEEX
  LIBS_LDFLAGS +=  -L../ext_libs/libspeex -lspeex
  LIBS_TARGET_EXT_LIBS += libspeex/libspeex.a
endif

endif #if WIN32


export DEF_CFLAGS LIBS_CFLAGS LIBS_LDFLAGS LIBS_TARGET_EXT_LIBS


###############################################################################
### internal defs
ZIP_FILES = COPYING 
ZIP_FILES+= README.md 
ZIP_FILES+= cli/test.exe
ZIP_FILES+= winamp/in_vgmstream.dll
ZIP_FILES+= xmplay/xmp-vgmstream.dll
ZIP_FILES+= ext_libs/*.dll
ZIP_FILES+= ext_libs/libspeex/*.dll
ZIP_FILES_AO = $(LIBAO_DLL_PATH)/*.dll

###############################################################################
### targets
buildrelease: clean bin

buildrelease-ex: clean bin-ex

buildfullrelease: clean sourceball bin

sourceball:
	rm -rf vgmstream-`./version.sh`
	git checkout-index -f -a --prefix=vgmstream-`./version.sh`/
#	git archive --format zip --output vgmstream-`./version.sh`.zip master
	echo "#!/bin/sh" > vgmstream-`./version.sh`/version.sh
	echo "echo \"`./version.sh`\"" >> vgmstream-`./version.sh`/version.sh
	tar cvzf "vgmstream-`./version.sh`.tar.gz" vgmstream-`./version.sh`/*
	rm -rf vgmstream-`./version.sh`

bin mingwbin: vgmstream_cli winamp xmplay
	zip -FS -j "vgmstream-`./version.sh`-test.zip" $(ZIP_FILES)

#separate since vgmstream123 is kinda untested
bin-ex mingwbin-ex: vgmstream_cli winamp xmplay vgmstream123
	zip -FS -j "vgmstream-`./version.sh`-test.zip" $(ZIP_FILES) $(ZIP_FILES_AO)

vgmstream_cli mingw_test:
	$(MAKE) -C cli vgmstream_cli

vgmstream123:
	$(MAKE) -C cli vgmstream123

winamp mingw_winamp:
	$(MAKE) -C winamp in_vgmstream

xmplay mingw_xmplay:
	$(MAKE) -C xmplay xmp_vgmstream

clean:
	$(RMF) vgmstream-*.zip
	$(MAKE) -C src clean
	$(MAKE) -C cli clean
	$(MAKE) -C winamp clean
	$(MAKE) -C xmplay clean
	$(MAKE) -C ext_libs clean

.PHONY: clean buildfullrelease buildrelease sourceball bin vgmstream_cli winamp xmplay mingwbin mingw_test mingw_winamp mingw_xmplay

#deprecated: buildfullrelease sourceball mingwbin mingw_test mingw_winamp mingw_xmplay
