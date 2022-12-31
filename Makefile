###############################
# vgmstream makefile
###############################

ifeq ($(VGMSTREAM_VERSION),)
  # for current dir (expanded later)
  VGMSTREAM_VERSION=`sh ./version-get.sh`
else
  VGMSTREAM_VERSION=$(VGMSTREAM_VERSION)
endif
DEF_CFLAGS += -DVGMSTREAM_VERSION_AUTO -DVGM_LOG_OUTPUT

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

DEF_CFLAGS += -ffast-math -O3 -Wall -Werror=format-security -Wvla -Wimplicit-function-declaration -Wignored-qualifiers

VGM_DEBUG_FLAGS = 0
ifeq ($(VGM_DEBUG_FLAGS),1)
  #DEF_CFLAGS += -O0
  DEF_CFLAGS += -g -DVGM_DEBUG_OUTPUT
  DEF_CFLAGS += -Wall
  DEF_CFLAGS += -Wextra
  DEF_CFLAGS += -Wno-sign-compare
  DEF_CFLAGS += -Wlogical-op
  #DEF_CFLAGS += -pedantic -Wconversion -std=gnu90
  #DEF_CFLAGS += -Wfloat-equal
  DEF_CFLAGS += -Wdisabled-optimization -Wunsafe-loop-optimizations -Wswitch-default
  DEF_CFLAGS +=  -Wcast-qual -Wpointer-arith
  DEF_CFLAGS += -Wcast-align=strict -Wduplicated-cond -Wjump-misses-init -Wnull-dereference
  DEF_CFLAGS += -Wold-style-definition -Wstrict-prototypes
  DEF_CFLAGS += -Wmultistatement-macros -Wstringop-truncation
  DEF_CFLAGS += -Wredundant-decls -Wmissing-include-dirs -Wmissing-declarations
  #DEF_CFLAGS += -Wshadow
  #DEF_CFLAGS += -Wstack-protector -fstack-protector
  STRIP = echo
endif

LIBS_CFLAGS=
LIBS_LDFLAGS=
LIBS_TARGET_EXT_LIBS=

# config libs
VGM_G7221 = 1
ifneq ($(VGM_G7221),0)
  LIBS_CFLAGS  += -DVGM_USE_G7221
endif


### external libs
# (call "make VGM_xxx = 0/1" to override 0/1 defaults, as Make does)
ifeq ($(TARGET_OS),Windows_NT)

  # enabled by default on Windows
  VGM_VORBIS = 1
  ifneq ($(VGM_VORBIS),0)
    LIBS_CFLAGS  += -DVGM_USE_VORBIS
    LIBS_LDFLAGS += -lvorbis
    LIBS_TARGET_EXT_LIBS += libvorbis.a
  endif

  VGM_MPEG = 1
  ifneq ($(VGM_MPEG),0)
    LIBS_CFLAGS  += -DVGM_USE_MPEG
    LIBS_LDFLAGS += -lmpg123-0
    LIBS_TARGET_EXT_LIBS += libmpg123-0.a
  endif

  VGM_G719 = 1
  ifneq ($(VGM_G719),0)
    LIBS_CFLAGS  += -DVGM_USE_G719
    LIBS_LDFLAGS += -lg719_decode
    LIBS_TARGET_EXT_LIBS += libg719_decode.a
  endif

  VGM_MAT3P = 0
  ifneq ($(VGM_MAT3P),0)
    LIBS_CFLAGS  += -DVGM_USE_MAIATRAC3PLUS
    LIBS_LDFLAGS += -lat3plusdecoder
    LIBS_TARGET_EXT_LIBS += libat3plusdecoder.a
  endif

  VGM_FFMPEG = 1
  ifneq ($(VGM_FFMPEG),0)
    LIBS_CFLAGS  += -DVGM_USE_FFMPEG -I../ext_includes/ffmpeg
    LIBS_LDFLAGS += -lavcodec -lavformat -lavutil -lswresample
    LIBS_TARGET_EXT_LIBS += libavcodec.a libavformat.a libavutil.a libswresample.a
  endif

  VGM_ATRAC9 = 1
  ifneq ($(VGM_ATRAC9),0)
    LIBS_CFLAGS  += -DVGM_USE_ATRAC9
    LIBS_LDFLAGS += -latrac9
    LIBS_TARGET_EXT_LIBS += libatrac9.a
  endif

  VGM_CELT = 1
  ifneq ($(VGM_CELT),0)
    LIBS_CFLAGS  += -DVGM_USE_CELT
    LIBS_LDFLAGS += -lcelt-0061 -lcelt-0110
    LIBS_TARGET_EXT_LIBS += libcelt-0061.a libcelt-0110.a
  endif

  VGM_SPEEX = 1
  ifneq ($(VGM_SPEEX),0)
    LIBS_CFLAGS  += -DVGM_USE_SPEEX
    LIBS_LDFLAGS +=  -L../ext_libs/libspeex -lspeex
    LIBS_TARGET_EXT_LIBS += libspeex/libspeex.a
  endif

else

  # must install system libs and enable manually on Linux
  VGM_VORBIS = 0
  ifneq ($(VGM_VORBIS),0)
    LIBS_CFLAGS  += -DVGM_USE_VORBIS
    LIBS_LDFLAGS += -lvorbis -lvorbisfile
  endif

  VGM_MPEG = 0
  ifneq ($(VGM_MPEG),0)
    LIBS_CFLAGS  += -DVGM_USE_MPEG
    LIBS_LDFLAGS += -lmpg123
  endif

  VGM_G719 = 0
  ifneq ($(VGM_G719),0)
    LIBS_CFLAGS  += -DVGM_USE_G719
    LIBS_LDFLAGS += -lg719_decode
  endif

  VGM_MAT3P = 0
  ifneq ($(VGM_MAT3P),0)
    LIBS_CFLAGS  += -DVGM_USE_MAIATRAC3PLUS
    LIBS_LDFLAGS += -lat3plusdecoder
  endif

  VGM_FFMPEG = 0
  ifneq ($(VGM_FFMPEG),0)
    LIBS_CFLAGS  += -DVGM_USE_FFMPEG
    LIBS_LDFLAGS += -lavcodec -lavformat -lavutil -lswresample
  endif

  VGM_ATRAC9 = 0
  ifneq ($(VGM_ATRAC9),0)
    LIBS_CFLAGS  += -DVGM_USE_ATRAC9
    ifeq ($(VGM_ATRAC9),1)
      LIBS_LDFLAGS += -latrac9
    endif
    ifeq ($(VGM_ATRAC9),2)
      LIBS_LDFLAGS += -l:libatrac9.a
    endif
  endif

  VGM_CELT = 0
  ifneq ($(VGM_CELT),0)
    LIBS_CFLAGS  += -DVGM_USE_CELT
    ifeq ($(VGM_CELT),1)
      LIBS_LDFLAGS += -lcelt-0061 -lcelt-0110
    endif
    ifeq ($(VGM_CELT),2)
      LIBS_LDFLAGS += -l:libcelt-0061.a -l:libcelt-0110.a
    endif
  endif

  VGM_SPEEX = 0
  ifneq ($(VGM_SPEEX),0)
    LIBS_CFLAGS  += -DVGM_USE_SPEEX
    LIBS_LDFLAGS +=  -lspeex
  endif
endif

export DEF_CFLAGS LIBS_CFLAGS LIBS_LDFLAGS LIBS_TARGET_EXT_LIBS


###############################################################################
### internal defs
ifeq ($(TARGET_OS),Windows_NT)
  BIN_FILE = vgmstream-$(VGMSTREAM_VERSION)-win.zip
  ZIP_FILES  = COPYING
  ZIP_FILES += README.md
  ZIP_FILES += doc/USAGE.md
  ZIP_FILES += cli/test.exe
  ZIP_FILES += winamp/in_vgmstream.dll
  ZIP_FILES += xmplay/xmp-vgmstream.dll
  ZIP_FILES += ext_libs/*.dll
  ZIP_FILES += ext_libs/libspeex/*.dll
  ZIP_FILES_AO  = cli/vgmstream123.exe
  ZIP_FILES_AO += $(LIBAO_DLL_PATH)/*.dll
else
  BIN_FILE = vgmstream-$(VGMSTREAM_VERSION)-bin.zip
  ZIP_FILES  = COPYING
  ZIP_FILES += README.md
  ZIP_FILES += doc/USAGE.md
  ZIP_FILES += cli/vgmstream-cli
  ZIP_FILES_AO  = cli/vgmstream123
endif

###############################################################################
### targets
buildrelease: clean bin

buildrelease-ex: clean bin-ex

buildfullrelease: clean sourceball bin

# make a tmp copy of git's index to avoid including dev stuff
sourceball:
	rm -rf vgmstream-$(VGMSTREAM_VERSION)
	git checkout-index -f -a --prefix=vgmstream-$(VGMSTREAM_VERSION)/
#	echo "#!/bin/sh" > vgmstream-$(VGMSTREAM_VERSION)/version-get.sh
#	echo "echo \"$(VGMSTREAM_VERSION)\"" >> vgmstream-$(VGMSTREAM_VERSION)/version-get.sh
	tar cvzf "bin/vgmstream-$(VGMSTREAM_VERSION)-src.tar.gz" vgmstream-$(VGMSTREAM_VERSION)/*
#	git archive --format zip --output bin/vgmstream-$(VGMSTREAM_VERSION)-src.zip master
	rm -rf vgmstream-$(VGMSTREAM_VERSION)

bin: vgmstream-cli winamp xmplay
	mkdir -p bin
	zip -FS -j "bin/$(BIN_FILE)" $(ZIP_FILES)

#separate since vgmstream123 is kinda untested
bin-ex: vgmstream-cli winamp xmplay vgmstream123
	mkdir -p bin
	zip -FS -j "bin/$(BIN_FILE)" $(ZIP_FILES) $(ZIP_FILES_AO)

vgmstream_cli: vgmstream-cli

vgmstream-cli: version
	$(MAKE) -C cli vgmstream_cli

vgmstream123: version
	$(MAKE) -C cli vgmstream123

winamp: version
	$(MAKE) -C winamp in_vgmstream

xmplay: version
	$(MAKE) -C xmplay xmp_vgmstream

version:
	sh version-make.sh

clean:
	$(RMF) vgmstream-*.zip
	$(MAKE) -C src clean
	$(MAKE) -C cli clean
	$(MAKE) -C winamp clean
	$(MAKE) -C xmplay clean
	$(MAKE) -C ext_libs clean

.PHONY: clean buildfullrelease buildrelease sourceball bin vgmstream-cli vgmstream_cli vgmstream123 winamp xmplay version
