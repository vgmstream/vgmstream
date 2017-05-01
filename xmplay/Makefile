# optional parts
VGM_ENABLE_FFMPEG=1
ifeq ($(VGM_ENABLE_FFMPEG),1)
FFMPEG_CC=-DVGM_USE_FFMPEG
FFMPEG_LD=-lavcodec -lavformat -lavutil
endif

VGM_ENABLE_MAIATRAC3PLUS=0
ifeq ($(VGM_ENABLE_MAIATRAC3PLUS),1) 
MAT3P_CC=-DVGM_USE_MAIATRAC3PLUS
MAT3P_LD=-lat3plusdecoder
endif

#MPEG_CC=-DVGM_USE_MPEG
MPEG_LD=-lmpg123-0
VGM_DISABLE_MPEG=0
ifeq ($(VGM_DISABLE_MPEG),1)
MPEG_CC=-DVGM_DISABLE_MPEG
MPEG_LD=
endif


# config
export SHELL = /bin/sh
export CFLAGS=-Wall -O3 $(MPEG_CC) -DVGM_USE_G7221 -DVGM_USE_G719 $(MAT3P_CC) $(FFMPEG_CC) -DUSE_ALLOCA -DWIN32 -I../ext_includes $(EXTRA_CFLAGS)
export LDFLAGS=-L../src -L../ext_libs -lvgmstream -lvorbis $(MPEG_LD) -lg7221_decode -lg719_decode $(MAT3P_LD) $(FFMPEG_LD) -lm $(EXTRA_LDFLAGS)
export CC=gcc
export AR=ar
export STRIP=strip


.PHONY: libvgmstream.a libvorbis.a libmpg123-0.a libg7221_decode.a libg719_decode.a libat3plusdecoder.a libavcodec.a libavformat.a libavutil.a

xmp-vgmstream.dll: libvgmstream.a libvorbis.a libmpg123-0.a libg7221_decode.a libg719_decode.a libat3plusdecoder.a libavcodec.a libavformat.a libavutil.a
	$(CC) -shared -static-libgcc $(CFLAGS) "-DVERSION=\"`../version.sh`\"" xmp_vgmstream.c $(LDFLAGS) -o xmp-vgmstream.dll xmpin.def
	$(STRIP) xmp-vgmstream.dll

libvgmstream.a:
	$(MAKE) -C ../src $@

libvorbis.a:
	$(MAKE) -C ../ext_libs -f Makefile.mingw $@

libmpg123-0.a:
	$(MAKE) -C ../ext_libs -f Makefile.mingw $@

libg7221_decode.a:
	$(MAKE) -C ../ext_libs -f Makefile.mingw $@

libg719_decode.a:
	$(MAKE) -C ../ext_libs -f Makefile.mingw $@

libat3plusdecoder.a:
	$(MAKE) -C ../ext_libs -f Makefile.mingw $@

libavcodec.a:
	$(MAKE) -C ../ext_libs -f Makefile.mingw $@

libavformat.a:
	$(MAKE) -C ../ext_libs -f Makefile.mingw $@

libavutil.a:
	$(MAKE) -C ../ext_libs -f Makefile.mingw $@

clean:
	rm -f xmp-vgmstream.dll
