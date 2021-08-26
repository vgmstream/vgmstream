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

UTIL_SRCS = $(wildcard util/*.c)
UTIL_OBJS = $(patsubst %.c,%.o,$(UTIL_SRCS))
OBJECTS += $(UTIL_OBJS)


libvgmstream.a: $(OBJECTS)
	$(AR) crs libvgmstream.a $(OBJECTS)

libvgmstream.so: $(OBJECTS)
	$(LD) -shared -o libvgmstream.so $(OBJECTS)

#vgmstream-deps:
#	$(CC) $(CFLAGS) -M -o vgmstream-deps

clean:
	$(RMF) $(OBJECTS) libvgmstream.a

.PHONY: clean
