export SHELL = /bin/sh
export CFLAGS=-Wall -O3
export LDFLAGS=-lm -L../src -lvgmstream -lvorbisfile
export STRIP=strip

.PHONY: libvgmstream.a libvorbisfile.a

test: libvgmstream.a test.o
	$(CC) test.o $(LDFLAGS) $(CFLAGS) -o test
	$(STRIP) test

test.o: test.c
	$(CC) $(CFLAGS) -c "-DVERSION=\"`../version.sh`\"" test.c -o test.o

libvgmstream.a:
	$(MAKE) -C ../src libvgmstream.a

clean:
	rm -f test test.o
