export SHELL = /bin/sh
export CFLAGS=-Wall -O3
export LDFLAGS=-lm -L../src -lvgmstream
export CC=i586-mingw32msvc-gcc
export AR=i586-mingw32msvc-ar
export STRIP=i586-mingw32msvc-strip

.PHONY: libvgmstream.a

in_vgmstream.dll: libvgmstream.a
	$(CC) -shared $(CFLAGS) in_vgmstream.c $(LDFLAGS) -o in_vgmstream.dll
	$(STRIP) in_vgmstream.dll

libvgmstream.a:
	$(MAKE) -C ../src libvgmstream.a

clean:
	rm -f in_vgmstream.dll
