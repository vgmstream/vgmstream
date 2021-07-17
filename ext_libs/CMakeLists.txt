if(MSVC)
	if(USE_MPEG)
		add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/libmpg123-0.lib ${CMAKE_CURRENT_BINARY_DIR}/libmpg123-0.exp
			COMMAND lib
			ARGS /def:${CMAKE_CURRENT_SOURCE_DIR}/libmpg123-0.def /machine:x86 /out:${CMAKE_CURRENT_BINARY_DIR}/libmpg123-0.lib
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/libmpg123-0.def)
		add_custom_target(libmpg123
			DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/libmpg123-0.lib
			SOURCES
				${VGM_SOURCE_DIR}/ext_includes/fmt123.h
				${VGM_SOURCE_DIR}/ext_includes/mpg123.h)
	endif()

	if(USE_VORBIS)
		add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/libvorbis.lib ${CMAKE_CURRENT_BINARY_DIR}/libvorbis.exp
			COMMAND lib
			ARGS /def:${CMAKE_CURRENT_SOURCE_DIR}/libvorbis.def /machine:x86 /out:${CMAKE_CURRENT_BINARY_DIR}/libvorbis.lib
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/libvorbis.def)
		file(GLOB OGG_HEADERS "${VGM_SOURCE_DIR}/ext_includes/ogg/*.h")
		file(GLOB VORBIS_HEADERS "${VGM_SOURCE_DIR}/ext_includes/vorbis/*h")
		add_custom_target(libvorbis
			DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/libvorbis.lib
			SOURCES
				${OGG_HEADERS}
				${VORBIS_HEADERS})
		source_group("Header Files\\ogg" FILES ${OGG_HEADERS})
		source_group("Header Files\\vorbis" FILES ${VORBIS_HEADERS})
	endif()

	if(USE_G719)
		add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/libg719_decode.lib ${CMAKE_CURRENT_BINARY_DIR}/libg719_decode.exp
			COMMAND lib
			ARGS /def:${CMAKE_CURRENT_SOURCE_DIR}/libg719_decode.def /machine:x86 /out:${CMAKE_CURRENT_BINARY_DIR}/libg719_decode.lib
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/libg719_decode.def)
		add_custom_target(libg719_decode
			DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/libg719_decode.lib
			SOURCES ${VGM_SOURCE_DIR}/ext_includes/g719.h)
	endif()

	if(USE_FFMPEG AND NOT FFMPEG_LIBRARIES)
		add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/avcodec.lib ${CMAKE_CURRENT_BINARY_DIR}/avcodec.exp
			COMMAND lib
			ARGS /def:${CMAKE_CURRENT_SOURCE_DIR}/avcodec-vgmstream-58.def /machine:x86 /out:${CMAKE_CURRENT_BINARY_DIR}/avcodec.lib
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/avcodec-vgmstream-58.def)
		add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/avformat.lib ${CMAKE_CURRENT_BINARY_DIR}/avformat.exp
			COMMAND lib
			ARGS /def:${CMAKE_CURRENT_SOURCE_DIR}/avformat-vgmstream-58.def /machine:x86 /out:${CMAKE_CURRENT_BINARY_DIR}/avformat.lib
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/avformat-vgmstream-58.def)
		add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/avutil.lib ${CMAKE_CURRENT_BINARY_DIR}/avutil.exp
			COMMAND lib
			ARGS /def:${CMAKE_CURRENT_SOURCE_DIR}/avutil-vgmstream-56.def /machine:x86 /out:${CMAKE_CURRENT_BINARY_DIR}/avutil.lib
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/avutil-vgmstream-56.def)
		add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/swresample.lib ${CMAKE_CURRENT_BINARY_DIR}/swresample.exp
			COMMAND lib
			ARGS /def:${CMAKE_CURRENT_SOURCE_DIR}/swresample-vgmstream-3.def /machine:x86 /out:${CMAKE_CURRENT_BINARY_DIR}/swresample.lib
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/swresample-vgmstream-3.def)
		file(GLOB AVCODEC_HEADERS "${VGM_SOURCE_DIR}/ext_includes/ffmpeg/libavcodec/*.h")
		file(GLOB AVFORMAT_HEADERS "${VGM_SOURCE_DIR}/ext_includes/ffmpeg/libavformat/*.h")
		file(GLOB AVUTIL_HEADERS "${VGM_SOURCE_DIR}/ext_includes/ffmpeg/libavutil/*.h")
		file(GLOB SWRESAMPLE_HEADERS "${VGM_SOURCE_DIR}/ext_includes/ffmpeg/libswresample/*.h")
		add_custom_target(ffmpeg
			DEPENDS
				${CMAKE_CURRENT_BINARY_DIR}/avcodec.lib
				${CMAKE_CURRENT_BINARY_DIR}/avformat.lib
				${CMAKE_CURRENT_BINARY_DIR}/avutil.lib
				${CMAKE_CURRENT_BINARY_DIR}/swresample.lib
			SOURCES
				${AVCODEC_HEADERS}
				${AVFORMAT_HEADERS}
				${AVUTIL_HEADERS}
				${SWRESAMPLE_HEADERS})
		source_group("Header Files\\ffmpeg\\libavcodec" FILES ${AVCODEC_HEADERS})
		source_group("Header Files\\ffmpeg\\libavformat" FILES ${AVFORMAT_HEADERS})
		source_group("Header Files\\ffmpeg\\libavutil" FILES ${AVUTIL_HEADERS})
		source_group("Header Files\\ffmpeg\\libswresample" FILES ${SWRESAMPLE_HEADERS})
	endif()

	if(USE_ATRAC9)
		add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/libatrac9.lib ${CMAKE_CURRENT_BINARY_DIR}/libatrac9.exp
			COMMAND lib
			ARGS /def:${CMAKE_CURRENT_SOURCE_DIR}/libatrac9.def /machine:x86 /out:${CMAKE_CURRENT_BINARY_DIR}/libatrac9.lib
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/libatrac9.def)
		add_custom_target(libatrac9
			DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/libatrac9.lib
			SOURCES ${VGM_SOURCE_DIR}/ext_includes/libatrac9.h)
	endif()

	if(USE_CELT)
		add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/libcelt-0061.lib ${CMAKE_CURRENT_BINARY_DIR}/libcelt-0061.exp
			COMMAND lib
			ARGS /def:${CMAKE_CURRENT_SOURCE_DIR}/libcelt-0061.def /machine:x86 /out:${CMAKE_CURRENT_BINARY_DIR}/libcelt-0061.lib
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/libcelt-0061.def)
		add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/libcelt-0110.lib ${CMAKE_CURRENT_BINARY_DIR}/libcelt-0110.exp
			COMMAND lib
			ARGS /def:${CMAKE_CURRENT_SOURCE_DIR}/libcelt-0110.def /machine:x86 /out:${CMAKE_CURRENT_BINARY_DIR}/libcelt-0110.lib
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/libcelt-0110.def)
		file(GLOB CELT_HEADERS "${VGM_SOURCE_DIR}/ext_includes/celt/*.h")
		add_custom_target(libcelt
			DEPENDS
				${CMAKE_CURRENT_BINARY_DIR}/libcelt-0061.lib
				${CMAKE_CURRENT_BINARY_DIR}/libcelt-0110.lib
			SOURCES ${CELT_HEADERS})
	endif()

    if(USE_SPEEX)
        add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/libspeex/libspeex.lib ${CMAKE_CURRENT_BINARY_DIR}/libspeex/libspeex.exp
            COMMAND lib
            ARGS /def:${CMAKE_CURRENT_SOURCE_DIR}/libspeex/libspeex.def /machine:x86 /out:${CMAKE_CURRENT_BINARY_DIR}/libspeex/libspeex.lib
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/libspeex/libspeex.def)
        file(GLOB SPEEX_HEADERS "${VGM_SOURCE_DIR}/ext_includes/speex/*.h")
        add_custom_target(libspeex
            DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/libspeex/libspeex.lib
            SOURCES ${SPEEX_HEADERS})
    endif()
elseif(MINGW)
	if(NOT DLLTOOL)
		set(DLLTOOL dlltool)
	endif()

	if(USE_MPEG)
		add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/libmpg123-0.lib
			COMMAND ${DLLTOOL}
			ARGS -d ${CMAKE_CURRENT_SOURCE_DIR}/libmpg123-0.def -l ${CMAKE_CURRENT_BINARY_DIR}/libmpg123-0.lib
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/libmpg123-0.def)
		add_custom_target(libmpg123
			DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/libmpg123-0.lib)
	endif()

	if(USE_VORBIS)
		add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/libvorbis.lib
			COMMAND ${DLLTOOL}
			ARGS -d ${CMAKE_CURRENT_SOURCE_DIR}/libvorbis.def -l ${CMAKE_CURRENT_BINARY_DIR}/libvorbis.lib
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/libvorbis.def)
		add_custom_target(libvorbis
			DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/libvorbis.lib)
	endif()

	if(USE_G719)
		add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/libg719_decode.lib
			COMMAND ${DLLTOOL}
			ARGS -d ${CMAKE_CURRENT_SOURCE_DIR}/libg719_decode.def -l ${CMAKE_CURRENT_BINARY_DIR}/libg719_decode.lib
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/libg719_decode.def)
		add_custom_target(libg719_decode
			DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/libg719_decode.lib)
	endif()

	if(USE_FFMPEG AND NOT FFMPEG_LIBRARIES)
		add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/avcodec.lib
			COMMAND ${DLLTOOL}
			ARGS -d ${CMAKE_CURRENT_SOURCE_DIR}/avcodec-vgmstream-58.def -l ${CMAKE_CURRENT_BINARY_DIR}/avcodec.lib -D avcodec-vgmstream-58.dll
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/avcodec-vgmstream-58.def)
		add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/avformat.lib
			COMMAND ${DLLTOOL}
			ARGS -d ${CMAKE_CURRENT_SOURCE_DIR}/avformat-vgmstream-58.def -l ${CMAKE_CURRENT_BINARY_DIR}/avformat.lib -D avformat-vgmstream-58.dll
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/avformat-vgmstream-58.def)
		add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/avutil.lib
			COMMAND ${DLLTOOL}
			ARGS -d ${CMAKE_CURRENT_SOURCE_DIR}/avutil-vgmstream-56.def -l ${CMAKE_CURRENT_BINARY_DIR}/avutil.lib -D avutil-vgmstream-56.dll
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/avutil-vgmstream-56.def)
		add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/swresample.lib
			COMMAND ${DLLTOOL}
			ARGS -d ${CMAKE_CURRENT_SOURCE_DIR}/swresample-vgmstream-3.def -l ${CMAKE_CURRENT_BINARY_DIR}/swresample.lib -D swresample-vgmstream-3.dll
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/swresample-vgmstream-3.def)
		add_custom_target(ffmpeg
			DEPENDS
				${CMAKE_CURRENT_BINARY_DIR}/avcodec.lib
				${CMAKE_CURRENT_BINARY_DIR}/avformat.lib
				${CMAKE_CURRENT_BINARY_DIR}/avutil.lib
				${CMAKE_CURRENT_BINARY_DIR}/swresample.lib)
	endif()

	if(USE_ATRAC9)
		add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/libatrac9.lib
			COMMAND ${DLLTOOL}
			ARGS -d ${CMAKE_CURRENT_SOURCE_DIR}/libatrac9.def -l ${CMAKE_CURRENT_BINARY_DIR}/libatrac9.lib
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/libatrac9.def)
		add_custom_target(libatrac9
			DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/libatrac9.lib)
	endif()

	if(USE_CELT)
		add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/libcelt-0061.lib
			COMMAND ${DLLTOOL}
			ARGS -d ${CMAKE_CURRENT_SOURCE_DIR}/libcelt-0061.def -l ${CMAKE_CURRENT_BINARY_DIR}/libcelt-0061.lib
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/libcelt-0061.def)
		add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/libcelt-0110.lib
			COMMAND ${DLLTOOL}
			ARGS -d ${CMAKE_CURRENT_SOURCE_DIR}/libcelt-0110.def -l ${CMAKE_CURRENT_BINARY_DIR}/libcelt-0110.lib
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/libcelt-0110.def)
		add_custom_target(libcelt
			DEPENDS
				${CMAKE_CURRENT_BINARY_DIR}/libcelt-0061.lib
				${CMAKE_CURRENT_BINARY_DIR}/libcelt-0110.lib)
	endif()

    if(USE_SPEEX)
        add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/libspeex/libspeex.lib
            COMMAND ${DLLTOOL}
            ARGS -d ${CMAKE_CURRENT_SOURCE_DIR}/libspeex/libspeex.def -l ${CMAKE_CURRENT_BINARY_DIR}/libspeex/libspeex.lib
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/libspeex/libspeex.def)
        add_custom_target(libspeex
            DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/libspeex/libspeex.lib)
    endif()
endif()
