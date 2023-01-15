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

libavcodec.a: avcodec-vgmstream-59.dll avcodec-vgmstream-59.def
	$(DLLTOOL) -D avcodec-vgmstream-59.dll -d avcodec-vgmstream-59.def -l libavcodec.a

libavformat.a: avformat-vgmstream-59.dll avformat-vgmstream-59.def
	$(DLLTOOL) -D avformat-vgmstream-59.dll -d avformat-vgmstream-59.def -l libavformat.a

libavutil.a: avutil-vgmstream-57.dll avutil-vgmstream-57.def
	$(DLLTOOL) -D avutil-vgmstream-57.dll -d avutil-vgmstream-57.def -l libavutil.a

libswresample.a: swresample-vgmstream-4.dll swresample-vgmstream-4.def
	$(DLLTOOL) -D swresample-vgmstream-4.dll -d swresample-vgmstream-4.def -l libswresample.a

libatrac9.a: libatrac9.dll libatrac9.def
	$(DLLTOOL) -D libatrac9.dll -d libatrac9.def -l libatrac9.a

libcelt-0061.a: libcelt-0061.dll libcelt-0061.def
	$(DLLTOOL) -D libcelt-0061.dll -d libcelt-0061.def -l libcelt-0061.a

libcelt-0110.a: libcelt-0110.dll libcelt-0110.def
	$(DLLTOOL) -D libcelt-0110.dll -d libcelt-0110.def -l libcelt-0110.a

libspeex-1.a: libspeex-1.dll libspeex-1.def
	$(DLLTOOL) -D libspeex-1.dll -d libspeex-1.def -l libspeex-1.a

clean:
	$(RMF) libvorbis.a libmpg123-0.a libg719_decode.a libavcodec.a libavformat.a libavutil.a libswresample.a libatrac9.a libcelt-0061.a libcelt-0110.a libspeex-1.a

.PHONY: clean
