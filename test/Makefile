export SHELL = /bin/sh
export CFLAGS=-Wall -ggdb
export LDFLAGS=-lm -L../src -lvgmstream -lvorbisfile -lmpg123
export STRIP=strip

.PHONY: libvgmstream.a

test: libvgmstream.a test.o
	$(CC) test.o $(LDFLAGS) $(CFLAGS) -o test

test.o: test.c
	$(CC) $(CFLAGS) -c "-DVERSION=\"`../version.sh`\"" test.c -o test.o

libvgmstream.a:
	$(MAKE) -C ../src libvgmstream.a

clean:
	rm -f test test.o
