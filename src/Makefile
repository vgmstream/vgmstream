#
# main vgmstream code
#

# automatically get all possible .o by finding all .c
OBJECTS =

#SRCS = $(wildcard **/*.c) #GNUMake 3.81?
SRCS = $(wildcard *.c) $(wildcard */*.c)
OBJECTS = $(patsubst %.c,%.o,$(SRCS))


libvgmstream.a: $(OBJECTS)
	$(AR) crs libvgmstream.a $(OBJECTS)

libvgmstream.so: $(OBJECTS)
	$(LD) -shared -o libvgmstream.so $(OBJECTS)

#vgmstream-deps:
#	$(CC) $(CFLAGS) -M -o vgmstream-deps

clean:
	$(RMF) $(OBJECTS) libvgmstream.a

.PHONY: clean
