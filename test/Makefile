export SHELL = /bin/sh
export CFLAGS=-Wall -O3 -I../ext_includes
export LDFLAGS=-L../src -L ../ext_libs -lvgmstream -lvorbisfile -lmpg123 -lm
export CC=gcc
export AR=ar
export STRIP=strip

.PHONY: libvgmstream.a

test.exe: libvgmstream.a
	$(CC) $(CFLAGS) $(LDFLAGS) "-DVERSION=\"`../version.sh`\"" test.c $(LDFLAGS) -o test
	$(STRIP) test

libvgmstream.a:
	$(MAKE) -C ../src $@

clean:
	rm -f test test.o
