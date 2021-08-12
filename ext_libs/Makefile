#
# external libs
#

# don't barf in case it's target "clean"
#ifneq ($(TARGET_OS),Windows_NT)
#$(error option must be built with TARGET_OS = Windows_NT)
#endif


libvorbis.a: libvorbis.def
	$(DLLTOOL) -d libvorbis.def -l libvorbis.a

libmpg123-0.a: libmpg123-0.def
	$(DLLTOOL) -d libmpg123-0.def -l libmpg123-0.a

libg719_decode.a: libg719_decode.def
	$(DLLTOOL) -d libg719_decode.def -l libg719_decode.a

libavcodec.a: avcodec-vgmstream-58.dll avcodec-vgmstream-58.def
	$(DLLTOOL) -D avcodec-vgmstream-58.dll -d avcodec-vgmstream-58.def -l libavcodec.a

libavformat.a: avformat-vgmstream-58.dll avformat-vgmstream-58.def
	$(DLLTOOL) -D avformat-vgmstream-58.dll -d avformat-vgmstream-58.def -l libavformat.a

libavutil.a: avutil-vgmstream-56.dll avutil-vgmstream-56.def
	$(DLLTOOL) -D avutil-vgmstream-56.dll -d avutil-vgmstream-56.def -l libavutil.a

libswresample.a: swresample-vgmstream-3.dll swresample-vgmstream-3.def
	$(DLLTOOL) -D swresample-vgmstream-3.dll -d swresample-vgmstream-3.def -l libswresample.a

libatrac9.a: libatrac9.dll libatrac9.def
	$(DLLTOOL) -D libatrac9.dll -d libatrac9.def -l libatrac9.a

libcelt-0061.a: libcelt-0061.dll libcelt-0061.def
	$(DLLTOOL) -D libcelt-0061.dll -d libcelt-0061.def -l libcelt-0061.a

libcelt-0110.a: libcelt-0110.dll libcelt-0110.def
	$(DLLTOOL) -D libcelt-0110.dll -d libcelt-0110.def -l libcelt-0110.a

libspeex/libspeex.a: libspeex/libspeex.dll libspeex/libspeex.def
	$(DLLTOOL) -D libspeex.dll -d libspeex/libspeex.def -l libspeex/libspeex.a

clean:
	$(RMF) libvorbis.a libmpg123-0.a libg719_decode.a libavcodec.a libavformat.a libavutil.a libswresample.a libatrac9.a libcelt-0061.a libcelt-0110.a

.PHONY: clean
