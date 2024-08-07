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

CFLAGS += $(DEF_CFLAGS) $(EXTRA_CFLAGS)
LDFLAGS += -L../src -lvgmstream -lm $(EXTRA_LDFLAGS)
TARGET_EXT_LIBS = 

OUTPUT_WINAMP = in_vgmstream.dll

ifeq ($(TARGET_OS),Windows_NT)
  CFLAGS += -DWIN32 -m32 -I../ext_includes
  LDFLAGS += -L../ext_libs/$(DLL_DIR)
endif

CFLAGS += $(LIBS_CFLAGS)
LDFLAGS += $(LIBS_LDFLAGS)
TARGET_EXT_LIBS += $(LIBS_TARGET_EXT_LIBS)


export CFLAGS LDFLAGS

SRCS = $(wildcard *.c)
OBJECTS = $(wildcard *.o)

###############################################################################
### targets

in_vgmstream: libvgmstream.a $(TARGET_EXT_LIBS) resource.o
	$(CC) -shared -static-libgcc $(CFLAGS) $(SRCS) resource.o $(LDFLAGS) -o $(OUTPUT_WINAMP)
	$(STRIP) $(OUTPUT_WINAMP)

resource.o: resource.rc resource.h
	$(WINDRES) -o resource.o resource.rc

libvgmstream.a:
	$(MAKE) -C ../src $@

$(TARGET_EXT_LIBS):
	$(MAKE) -C ../ext_libs $@

clean:
	$(RMF) $(OUTPUT_WINAMP) $(OBJECTS) resource.o

.PHONY: clean in_vgmstream libvgmstream.a $(TARGET_EXT_LIBS)
