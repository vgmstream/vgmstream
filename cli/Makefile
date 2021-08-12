###############################
# CLI tools
###############################

###############################################################################
### main defs

LIBAO_IPATH = ../../libao/include
LIBAO_LPATH = ../../libao/bin


CFLAGS += $(DEF_CFLAGS) -DVAR_ARRAYS $(EXTRA_CFLAGS)
LDFLAGS += -L../src -lvgmstream -lm $(EXTRA_LDFLAGS)
TARGET_EXT_LIBS = 

ifeq ($(TARGET_OS),Windows_NT)
  OUTPUT_CLI = test.exe
  OUTPUT_123 = vgmstream123.exe

  # -DUSE_ALLOCA
  CFLAGS += -DWIN32 -I../ext_includes -I../ext_libs/Getopt -I../ext_includes/ffmpeg
  LDFLAGS += -L../ext_libs

  LIBAO_INC = -I$(LIBAO_IPATH)
  LIBAO_LIB = -L$(LIBAO_LPATH) -lao
else
  OUTPUT_CLI = vgmstream-cli
  OUTPUT_123 = vgmstream123

  #todo move clhca/etc and remove
  CFLAGS += -I../ext_includes

  LIBAO_LIB = -lao
endif

CFLAGS += $(LIBS_CFLAGS)
LDFLAGS += $(LIBS_LDFLAGS)
TARGET_EXT_LIBS += $(LIBS_TARGET_EXT_LIBS)


export CFLAGS LDFLAGS


###############################################################################
### targets

vgmstream_cli: libvgmstream.a $(TARGET_EXT_LIBS)
	$(CC) $(CFLAGS) "-DVGMSTREAM_VERSION=\"$(VGMSTREAM_VERSION_PREV)\"" vgmstream_cli.c $(LDFLAGS) -o $(OUTPUT_CLI)
	$(STRIP) $(OUTPUT_CLI)

vgmstream123: libvgmstream.a $(TARGET_EXT_LIBS)
	$(CC) $(CFLAGS) $(LIBAO_INC) "-DVGMSTREAM_VERSION=\"$(VGMSTREAM_VERSION_PREV)\"" vgmstream123.c $(LDFLAGS) $(LIBAO_LIB) -o $(OUTPUT_123)
	$(STRIP) $(OUTPUT_123)

libvgmstream.a:
	$(MAKE) -C ../src $@

$(TARGET_EXT_LIBS):
	$(MAKE) -C ../ext_libs $@

clean:
	$(RMF) $(OUTPUT_CLI)

.PHONY: clean vgmstream_cli libvgmstream.a $(TARGET_EXT_LIBS)
