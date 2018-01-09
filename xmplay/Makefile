#
# XMPlay plugin
#

ifneq ($(TARGET_OS),WIN32)
$(error option must be built with TARGET_OS = WIN32)
endif


### main defs
OUTPUT_XMPLAY = xmp-vgmstream.dll

CFLAGS += -Wall -O3 -DUSE_ALLOCA -DWIN32 -I../ext_includes $(EXTRA_CFLAGS)
LDFLAGS += -L../src -L../ext_libs -lm -lvgmstream $(EXTRA_LDFLAGS)
TARGET_EXT_LIBS = 


### external libs
ifeq ($(TARGET_OS),WIN32)

VGM_ENABLE_VORBIS = 1
ifeq ($(VGM_ENABLE_VORBIS),1)
  #CFLAGS  += -DVGM_USE_VORBIS
  LDFLAGS += -lvorbis
  TARGET_EXT_LIBS += libvorbis.a
else
  CFLAGS  += -DVGM_DISABLE_VORBIS
endif

VGM_ENABLE_MPEG = 1
ifeq ($(VGM_ENABLE_MPEG),1)
  #CFLAGS  += -DVGM_USE_MPEG
  LDFLAGS += -lmpg123-0
  TARGET_EXT_LIBS += libmpg123-0.a
else
  CFLAGS  += -DVGM_DISABLE_MPEG
endif

VGM_ENABLE_G7221 = 1
ifeq ($(VGM_ENABLE_G7221),1) 
  CFLAGS  += -DVGM_USE_G7221
  LDFLAGS += -lg7221_decode
  TARGET_EXT_LIBS += libg7221_decode.a
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

endif #if WIN32

export CFLAGS LDFLAGS


### targets

xmp_vgmstream: libvgmstream.a $(TARGET_EXT_LIBS)
	$(CC) -shared -static-libgcc $(CFLAGS) "-DVERSION=\"`../version.sh`\"" xmp_vgmstream.c $(LDFLAGS) -o $(OUTPUT_XMPLAY) xmpin.def
	$(STRIP) $(OUTPUT_XMPLAY)

libvgmstream.a:
	$(MAKE) -C ../src $@

$(TARGET_EXT_LIBS):
	$(MAKE) -C ../ext_libs $@

clean:
	$(RMF) $(OUTPUT_XMPLAY)

.PHONY: clean xmp_vgmstream libvgmstream.a $(TARGET_EXT_LIBS)
