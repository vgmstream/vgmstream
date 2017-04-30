# get all .c to find all possible .o
SRC_SRCS = $(wildcard *.c)
SRC_OBJS = $(patsubst %.c,%.o,$(SRC_SRCS))

CODING_SRCS = $(wildcard coding/*.c)
CODING_OBJS = $(patsubst %.c,%.o,$(CODING_SRCS))

LAYOUT_SRCS = $(wildcard layout/*.c)
LAYOUT_OBJS = $(patsubst %.c,%.o,$(LAYOUT_SRCS))

META_SRCS = $(wildcard meta/*.c)
META_OBJS = $(patsubst %.c,%.o,$(META_SRCS))

EXT_LIBS_SRCS = $(wildcard ../ext_libs/*.c)
EXT_LIBS_OBJS = $(patsubst %.c,%.o,$(EXT_LIBS_SRCS))

OBJECTS = $(SRC_OBJS) $(CODING_OBJS) $(LAYOUT_OBJS) $(META_OBJS) $(EXT_LIBS_OBJS)

libvgmstream.a: $(OBJECTS)
	$(AR) crs libvgmstream.a $(OBJECTS)

vgmstream-deps:
	$(CC) $(CFLAGS) -M -o vgmstream-deps

clean:
	rm -f $(OBJECTS) libvgmstream.a
