###############################
# CLI tools
###############################

###############################################################################
### main defs

ifeq ($(TARGET_OS),Windows_NT)
  OUTPUT_CLI = test.exe
  OUTPUT_123 = vgmstream123.exe
else
  OUTPUT_CLI = vgmstream-cli
  OUTPUT_123 = vgmstream123
endif

# -DUSE_ALLOCA
ifeq ($(TARGET_OS),Windows_NT)
  CFLAGS += -DWIN32
endif

CFLAGS += $(DEF_CFLAGS) -DVAR_ARRAYS -I../ext_includes $(EXTRA_CFLAGS)
LDFLAGS += -L../src -L../ext_libs -lm -lvgmstream $(EXTRA_LDFLAGS)
TARGET_EXT_LIBS = 

CFLAGS += $(LIBS_CFLAGS)
LDFLAGS += $(LIBS_LDFLAGS)
TARGET_EXT_LIBS += $(LIBS_TARGET_EXT_LIBS)

LIBAO_INC_PATH = ../../libao/include
LIBAO_LIB_PATH = ../../libao/bin


export CFLAGS LDFLAGS


###############################################################################
### targets

vgmstream_cli: libvgmstream.a $(TARGET_EXT_LIBS)
	$(CC) $(CFLAGS) "-DVERSION=\"`../version.sh`\"" vgmstream_cli.c $(LDFLAGS) -o $(OUTPUT_CLI)
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
