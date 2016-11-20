# optional parts
export ENABLE_FFMPEG=0
ifeq ($(ENABLE_FFMPEG),1)
export FFMPEG_CC=-DVGM_USE_FFMPEG -I../../ffmpeg-dev/include
export FFMPEG_LD=-L../../ffmpeg-dev/lib -lavcodec -lavformat -lavutil
endif

export ENABLE_MAIATRAC3PLUS=1
ifeq ($(ENABLE_MAIATRAC3PLUS),1) 
export MAT3P_CC=-DVGM_USE_MAIATRAC3PLUS
export MAT3P_LD=-lat3plusdecoder
endif

# config
export SHELL = /bin/sh
export CFLAGS=-Wall -O3 -DVGM_USE_G7221 -DVGM_USE_G719 $(MAT3P_CC) -DWIN32 -DUSE_ALLOCA -I../ext_includes $(FFMPEG_CC)
export LDFLAGS=-L../src -L../ext_libs -lvgmstream -lvorbis -lmpg123-0 -lg7221_decode -lg719_decode $(MAT3P_LD) -lm $(FFMPEG_LD)
export CC=i586-mingw32msvc-gcc
export AR=i586-mingw32msvc-ar
export STRIP=i586-mingw32msvc-strip
export WINDRES=i586-mingw32msvc-windres

#export CC=i686-w64-mingw32-gcc
#export AR=i686-w64-mingw32-ar
#export STRIP=i686-w64-mingw32-strip
#export WINDRES=i686-w64-mingw32-windres

.PHONY: libvgmstream.a libvorbis.a libmpg123-0.a libg7221_decode.a libg719_decode.a at3plusdecoder.a

in_vgmstream.dll: libvgmstream.a libvorbis.a libmpg123-0.a libg7221_decode.a libg719_decode.a libat3plusdecoder.a in_vgmstream.c resource.o
	$(CC) -shared -static-libgcc $(CFLAGS) "-DVERSION=\"`../version.sh`\"" in_vgmstream.c resource.o $(LDFLAGS) -o in_vgmstream.dll
	$(STRIP) in_vgmstream.dll

resource.o: resource.rc resource.h
	$(WINDRES) -o resource.o resource.rc

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

clean:
	rm -f in_vgmstream.dll resource.o
