#
# CLI tools
#


### main defs

ifeq ($(TARGET_OS),WIN32)
  OUTPUT_CLI = test.exe
  OUTPUT_123 = vgmstream123.exe
else
  OUTPUT_CLI = vgmstream-cli
  OUTPUT_123 = vgmstream123
endif

# -DUSE_ALLOCA
ifeq ($(TARGET_OS),WIN32)
  CFLAGS += -DWIN32
endif

CFLAGS += -Wall -O3 -DVAR_ARRAYS -I../ext_includes $(EXTRA_CFLAGS)
LDFLAGS += -L../src -L../ext_libs -lm -lvgmstream $(EXTRA_LDFLAGS)
TARGET_EXT_LIBS = 

LIBAO_INC_PATH = ../../libao/include
LIBAO_LIB_PATH = ../../libao/bin

#ifdef VGM_DEBUG
#  CFLAGS += -DVGM_DEBUG_OUTPUT -O0 -Werror=format-security -Wdeclaration-after-statement -Wvla
#  CFLAGS += -Wold-style-definition -Woverflow -Wpointer-arith -Wstrict-prototypes -pedantic -std=gnu90 -fstack-protector -Wformat
#endif


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

vgmstream_cli: libvgmstream.a $(TARGET_EXT_LIBS)
	$(CC) $(CFLAGS) "-DVERSION=\"`../version.sh`\"" test.c $(LDFLAGS) -o $(OUTPUT_CLI)
	$(STRIP) $(OUTPUT_CLI)

vgmstream123: libvgmstream.a $(TARGET_EXT_LIBS)
	$(CC) $(CFLAGS) -I$(LIBAO_INC_PATH) "-DVERSION=\"`../version.sh`\"" vgmstream123.c $(LDFLAGS) -L$(LIBAO_LIB_PATH) -lao -o $(OUTPUT_123)
	$(STRIP) $(OUTPUT_123)

libvgmstream.a:
	$(MAKE) -C ../src $@

$(TARGET_EXT_LIBS):
	$(MAKE) -C ../ext_libs $@

clean:
	$(RMF) $(OUTPUT_CLI)

.PHONY: clean vgmstream_cli libvgmstream.a $(TARGET_EXT_LIBS)
