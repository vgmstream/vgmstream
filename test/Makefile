export SHELL = /bin/sh
export CFLAGS=-Wall -O3
export LDFLAGS=-lm -L../src -lvgmstream
export STRIP=strip

.PHONY: libvgmstream.a

test: libvgmstream.a
	$(CC) $(CFLAGS) "-DVERSION=\"`../version.sh`\"" test.c $(LDFLAGS) -o test
	$(STRIP) test

libvgmstream.a:
	$(MAKE) -C ../src libvgmstream.a

clean:
	rm -f test
