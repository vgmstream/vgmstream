###############################
# XMPlay plugin
###############################


# cross-compile
ifneq ($(TARGET_OS),Windows_NT)
export CC=i586-mingw32msvc-gcc
export AR=i586-mingw32msvc-ar
export STRIP=i586-mingw32msvc-strip
export WINDRES=i586-mingw32msvc-windres
endif


###############################################################################
### main defs
OUTPUT_XMPLAY = xmp-vgmstream.dll

ifeq ($(TARGET_OS),Windows_NT)
  CFLAGS += -m32 -DWIN32
endif

CFLAGS += $(DEF_CFLAGS) -I../ext_includes $(EXTRA_CFLAGS)
LDFLAGS += -L../src -L../ext_libs/$(DLL_DIR) -lvgmstream -lm $(EXTRA_LDFLAGS)
TARGET_EXT_LIBS = 

CFLAGS += $(LIBS_CFLAGS)
LDFLAGS += $(LIBS_LDFLAGS)
TARGET_EXT_LIBS += $(LIBS_TARGET_EXT_LIBS)


export CFLAGS LDFLAGS


###############################################################################
### targets

xmp_vgmstream: libvgmstream.a $(TARGET_EXT_LIBS)
	$(CC) -shared -static-libgcc $(CFLAGS) xmp_vgmstream.c $(LDFLAGS) -o $(OUTPUT_XMPLAY) xmpin.def
	$(STRIP) $(OUTPUT_XMPLAY)

libvgmstream.a:
	$(MAKE) -C ../src $@

$(TARGET_EXT_LIBS):
	$(MAKE) -C ../ext_libs $@

clean:
	$(RMF) $(OUTPUT_XMPLAY)

.PHONY: clean xmp_vgmstream libvgmstream.a $(TARGET_EXT_LIBS)
