#
# Winamp plugin
#


# cross-compile
ifneq ($(TARGET_OS),Windows_NT)
export CC=i586-mingw32msvc-gcc
export AR=i586-mingw32msvc-ar
export STRIP=i586-mingw32msvc-strip
export WINDRES=i586-mingw32msvc-windres
endif


### main defs
OUTPUT_WINAMP = in_vgmstream.dll

CFLAGS += -ffast-math -O3 -Wall -Werror=format-security -Wdeclaration-after-statement -Wvla -DUSE_ALLOCA -DWIN32 -I../ext_includes $(EXTRA_CFLAGS)
LDFLAGS += -L../src -L../ext_libs -lm -lvgmstream $(EXTRA_LDFLAGS)
TARGET_EXT_LIBS = 


### external libs

VGM_ENABLE_VORBIS = 1
ifeq ($(VGM_ENABLE_VORBIS),1)
  CFLAGS  += -DVGM_USE_VORBIS
  LDFLAGS += -lvorbis
  TARGET_EXT_LIBS += libvorbis.a
endif

VGM_ENABLE_MPEG = 1
ifeq ($(VGM_ENABLE_MPEG),1)
  CFLAGS  += -DVGM_USE_MPEG
  LDFLAGS += -lmpg123-0
  TARGET_EXT_LIBS += libmpg123-0.a
endif

VGM_ENABLE_G7221 = 1
ifeq ($(VGM_ENABLE_G7221),1) 
  CFLAGS  += -DVGM_USE_G7221
endif

VGM_ENABLE_G719 = 1
ifeq ($(VGM_ENABLE_G719),1) 
  CFLAGS  += -DVGM_USE_G719
  LDFLAGS += -lg719_decode
  TARGET_EXT_LIBS += libg719_decode.a
endif

VGM_ENABLE_MAIATRAC3PLUS = 0
ifeq ($(VGM_ENABLE_MAIATRAC3PLUS),1) 
  CFLAGS  += -DVGM_USE_MAIATRAC3PLUS
  LDFLAGS += -lat3plusdecoder
  TARGET_EXT_LIBS += libat3plusdecoder.a
endif

VGM_ENABLE_FFMPEG = 1
ifeq ($(VGM_ENABLE_FFMPEG),1)
  CFLAGS  += -DVGM_USE_FFMPEG
  LDFLAGS += -lavcodec -lavformat -lavutil -lswresample
  TARGET_EXT_LIBS += libavcodec.a libavformat.a libavutil.a libswresample.a
endif

VGM_ENABLE_ATRAC9 = 1
ifeq ($(VGM_ENABLE_ATRAC9),1) 
  CFLAGS  += -DVGM_USE_ATRAC9
  LDFLAGS += -latrac9
  TARGET_EXT_LIBS += libatrac9.a
endif

VGM_ENABLE_CELT = 1
ifeq ($(VGM_ENABLE_CELT),1) 
  CFLAGS  += -DVGM_USE_CELT
  LDFLAGS += -lcelt-0061 -lcelt-0110
  TARGET_EXT_LIBS += libcelt-0061.a libcelt-0110.a
endif


export CFLAGS LDFLAGS


### targets

in_vgmstream: libvgmstream.a  $(TARGET_EXT_LIBS) resource.o
	$(CC) -shared -static-libgcc $(CFLAGS) "-DVERSION=\"`../version.sh`\"" in_vgmstream.c resource.o $(LDFLAGS) -o $(OUTPUT_WINAMP)
	$(STRIP) $(OUTPUT_WINAMP)

resource.o: resource.rc resource.h
	$(WINDRES) -o resource.o resource.rc

libvgmstream.a:
	$(MAKE) -C ../src $@

$(TARGET_EXT_LIBS):
	$(MAKE) -C ../ext_libs $@

clean:
	$(RMF) $(OUTPUT_WINAMP) resource.o

.PHONY: clean in_vgmstream libvgmstream.a $(TARGET_EXT_LIBS)
