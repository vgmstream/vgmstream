#
# main vgmstream code
#

# automatically get all possible .o by finding all .c
OBJECTS =

SRC_SRCS = $(wildcard *.c)
SRC_OBJS = $(patsubst %.c,%.o,$(SRC_SRCS))
OBJECTS += $(SRC_OBJS)

CODING_SRCS = $(wildcard coding/*.c)
CODING_OBJS = $(patsubst %.c,%.o,$(CODING_SRCS))
OBJECTS += $(CODING_OBJS)

LAYOUT_SRCS = $(wildcard layout/*.c)
LAYOUT_OBJS = $(patsubst %.c,%.o,$(LAYOUT_SRCS))
OBJECTS += $(LAYOUT_OBJS)

META_SRCS = $(wildcard meta/*.c)
META_OBJS = $(patsubst %.c,%.o,$(META_SRCS))
OBJECTS += $(META_OBJS)

EXT_LIBS_SRCS = $(wildcard ../ext_libs/*.c)
EXT_LIBS_OBJS = $(patsubst %.c,%.o,$(EXT_LIBS_SRCS))
OBJECTS += $(EXT_LIBS_OBJS)


libvgmstream.a: $(OBJECTS)
	$(AR) crs libvgmstream.a $(OBJECTS)

#vgmstream-deps:
#	$(CC) $(CFLAGS) -M -o vgmstream-deps

clean:
	$(RMF) $(OBJECTS) libvgmstream.a

.PHONY: clean
