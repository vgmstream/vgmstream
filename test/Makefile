SHELL = /bin/sh
CFLAGS=-lm -Wall -O3

VGMSTREAMFILES = ../src/streamfile.c ../src/vgmstream.c ../src/util.c ../src/meta/adx_header.c ../src/coding/adx_decoder.c ../src/coding/ngc_dsp_decoder.c ../src/meta/brstm.c ../src/layout/interleave.c ../src/layout/nolayout.c ../src/coding/pcm_decoder.c ../src/meta/nds_strm.c ../src/coding/ima_decoder.c ../src/meta/agsc.c ../src/meta/ngc_adpdtk.c ../src/coding/ngc_dtk_decoder.c ../src/coding/g721_decoder.c ../src/meta/rsf.c ../src/meta/afc_header.c ../src/coding/ngc_afc_decoder.c ../src/meta/ast.c ../src/layout/ast_blocked.c

test: test.c $(VGMSTREAMFILES)
	gcc $(CFLAGS) test.c $(VGMSTREAMFILES) -o test
	strip test

test.exe: test.c $(VGMSTREAMFILES)
	i586-mingw32msvc-gcc $(CFLAGS) test.c $(VGMSTREAMFILES) -o test.exe
	i586-mingw32msvc-strip test.exe
clean:
	rm -f test test.exe
