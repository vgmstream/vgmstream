export SHELL = /bin/sh
export CFLAGS=-Wall -O3 "-DVGM_USE_G7221" -I../ext_includes
export LDFLAGS=-L../src -L../ext_libs -lvgmstream -lvorbis -lmpg123-0 -lg7221_decode -lm
export CC=i586-mingw32msvc-gcc
export AR=i586-mingw32msvc-ar
export STRIP=i586-mingw32msvc-strip
export WINDRES=i586-mingw32msvc-windres

.PHONY: libvgmstream.a libvorbis.a libmpg123-0.a libg7221_decode.a

in_vgmstream.dll: libvgmstream.a libvorbis.a libmpg123-0.a libg7221_decode.a in_vgmstream.c resource.o
	$(CC) -shared $(CFLAGS) "-DVERSION=\"`../version.sh`\"" in_vgmstream.c resource.o $(LDFLAGS) -o in_vgmstream.dll
	$(STRIP) in_vgmstream.dll

resource.o: resource.rc resource.h
	$(WINDRES) -o resource.o resource.rc

libvgmstream.a:
	$(MAKE) -C ../src libvgmstream.a

libvorbis.a:
	$(MAKE) -C ../ext_libs -f Makefile.mingw $@

libmpg123-0.a:
	$(MAKE) -C ../ext_libs -f Makefile.mingw $@

libg7221_decode.a:
	$(MAKE) -C ../ext_libs -f Makefile.mingw $@

clean:
	rm -f in_vgmstream.dll resource.o
