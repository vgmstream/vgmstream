#
# main vgmstream code
#

#SRCS = $(wildcard **/*.c) #GNUMake 3.81?
SRCS = $(wildcard *.c) $(wildcard */*.c) $(wildcard */*/*.c)

# get all possible .o by finding all .c
OBJECTS = $(patsubst %.c,%.o,$(SRCS))

# in case of renamed files
OBJECTS_CLEAN = $(wildcard *.o) $(wildcard */*.o) $(wildcard */*/*.o)


libvgmstream.a: $(OBJECTS)
	$(AR) crs libvgmstream.a $(OBJECTS)

libvgmstream.so: $(OBJECTS)
	$(LD) -shared -o libvgmstream.so $(OBJECTS)

#vgmstream-deps:
#	$(CC) $(CFLAGS) -M -o vgmstream-deps

clean:
	$(RMF) $(OBJECTS_CLEAN) libvgmstream.a

.PHONY: clean
