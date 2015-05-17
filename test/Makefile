export SHELL = /bin/sh
export CFLAGS=-Wall -O3 -I../ext_includes -DUSE_ALLOCA -DVGM_USE_MAIATRAC3PLUS
# -DVGM_USE_G7221 -DVGM_USE_G719 
export LDFLAGS=-L../src -L ../ext_libs -lvgmstream -lvorbisfile -lmpg123 -lat3plusdecoder -lpthread -lm
#-lg719_decode -lg7221_decode -lsiren_decode
export CC=gcc
export AR=ar
export STRIP=strip

.PHONY: libvgmstream.a

test.exe: libvgmstream.a
	$(CC) $(CFLAGS) $(LDFLAGS) "-DVERSION=\"`../version.sh`\"" test.c $(LDFLAGS) -o vgmstream
	$(STRIP) vgmstream

libvgmstream.a:
	$(MAKE) -C ../src $@

clean:
	rm -f test test.o
