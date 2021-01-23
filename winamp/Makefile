###############################
# Winamp plugin
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
OUTPUT_WINAMP = in_vgmstream.dll

CFLAGS += $(DEF_CFLAGS) -DUSE_ALLOCA -DWIN32 -I../ext_includes $(EXTRA_CFLAGS)
LDFLAGS += -L../src -L../ext_libs -lm -lvgmstream $(EXTRA_LDFLAGS)
TARGET_EXT_LIBS = 

CFLAGS += $(LIBS_CFLAGS)
LDFLAGS += $(LIBS_LDFLAGS)
TARGET_EXT_LIBS += $(LIBS_TARGET_EXT_LIBS)


export CFLAGS LDFLAGS


###############################################################################
### targets

in_vgmstream: libvgmstream.a $(TARGET_EXT_LIBS) resource.o
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
