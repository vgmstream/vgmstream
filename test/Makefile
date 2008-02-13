SHELL = /bin/sh
CFLAGS=-lm -O3

VGMSTREAMFILES = ../src/streamfile.c ../src/vgmstream.c ../src/util.c ../src/meta/adx_header.c ../src/coding/adx_decoder.c ../src/coding/gcdsp_decoder.c ../src/meta/brstm.c ../src/layout/interleave.c ../src/layout/nolayout.c ../src/coding/pcm_decoder.c ../src/meta/nds_strm.c ../src/coding/ima_decoder.c ../src/meta/agsc.c

test: test.c $(VGMSTREAMFILES)
	gcc $(CFLAGS) test.c $(VGMSTREAMFILES) -o test
	strip test

test.exe: test.c $(VGMSTREAMFILES)
	i586-mingw32msvc-gcc $(CFLAGS) test.c $(VGMSTREAMFILES) -o test.exe
	i586-mingw32msvc-strip test.exe
clean:
	rm -f test test.exe
