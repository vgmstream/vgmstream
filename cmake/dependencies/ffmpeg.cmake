if(USE_FFMPEG)
	if(NOT FFMPEG_PATH AND NOT BUILD_STATIC)
		# FFmpeg detection
		if(WIN32)
			find_package(FFmpeg COMPONENTS AVFORMAT AVUTIL AVCODEC SWRESAMPLE)
		else()
			find_package(FFmpeg QUIET COMPONENTS AVFORMAT AVUTIL AVCODEC SWRESAMPLE)
		endif()

		if(NOT FFMPEG_LIBRARIES)
			if(WIN32)
				set_ffmpeg(OFF TRUE)
			else()
				set(FFmpeg_FOUND NO)
			endif()
		else()
			if(${AVCODEC_VERSION} VERSION_LESS 57)
				message("libavcodec version mismatch ${AVCODEC_VERSION} expected >=57")
				if(WIN32)
					set_ffmpeg(OFF TRUE)
				else()
					set(FFmpeg_FOUND NO)
				endif()
			elseif(${AVUTIL_VERSION} VERSION_LESS 55)
				message("libavutil version mismatch ${AVUTIL_VERSION} expected >=55")
				if(WIN32)
					set_ffmpeg(OFF TRUE)
				else()
					set(FFmpeg_FOUND NO)
				endif()
			elseif(${SWRESAMPLE_VERSION} VERSION_LESS 2)
				message("libswresample version mismatch ${SWRESAMPLE_VERSION} expected >=2")
				if(WIN32)
					set_ffmpeg(OFF TRUE)
				else()
					set(FFmpeg_FOUND NO)
				endif()
			elseif(${AVFORMAT_VERSION} VERSION_LESS 57)
				message("libavformat version mismatch ${AVFORMAT_VERSION} expected >=57")
				if(WIN32)
					set_ffmpeg(OFF TRUE)
				else()
					set(FFmpeg_FOUND NO)
				endif()
			endif()
		endif()

		if(FFmpeg_FOUND)
			set(FFMPEG_SOURCE "(system)")
		endif()
	endif()
	if(USE_FFMPEG AND NOT WIN32 AND (NOT FFmpeg_FOUND OR FFMPEG_PATH OR BUILD_STATIC))
		set(USE_FFMPEG_LIBOPUS OFF)
		if(NOT EMSCRIPTEN)
			find_package(PkgConfig REQUIRED)
			pkg_check_modules(PC_OPUS QUIET opus>=1.1)
			if(PC_OPUS_FOUND)
				set(USE_FFMPEG_LIBOPUS ON)
			endif()
		endif()

		FetchDependency(FFMPEG
			DIR ffmpeg
			GIT_REPOSITORY https://git.ffmpeg.org/ffmpeg.git
			GIT_TAG n5.1.2
		)

		if(FFMPEG_PATH)
			set(FFMPEG_COMPILE YES)

			set(FFMPEG_CONF_DEMUXER
				ac3 eac3 spdif asf xwma mov oma xmv ogg flac wav aac mp3 smacker bink binka caf mpc mpc8 tak ape
			)
			set(FFMPEG_CONF_PARSER
				ac3 mpegaudio xma vorbis opus
			)
			set(FFMPEG_CONF_DECODER
			ac3 eac3 wmapro wmav1 wmav2 xma1 xma2 aac atrac3 atrac3p mp2float mp3float smackaud binkaudio_dct binkaudio_rdft pcm_s16be pcm_s16be_planar pcm_s16le pcm_s16le_planar pcm_s8 pcm_s8_planar flac vorbis mpc7 mpc8 alac adpcm_ima_qt adpcm_ima_dk3 adpcm_ima_dk4 tak ape
			)
			if(USE_FFMPEG_LIBOPUS)
				list(APPEND FFMPEG_CONF_DECODER libopus)
			else()
				list(APPEND FFMPEG_CONF_DECODER opus)
			endif()
			string(REPLACE ";" "," FFMPEG_CONF_PARSER "${FFMPEG_CONF_PARSER}")
			string(REPLACE ";" "," FFMPEG_CONF_DEMUXER "${FFMPEG_CONF_DEMUXER}")
			string(REPLACE ";" "," FFMPEG_CONF_DECODER "${FFMPEG_CONF_DECODER}")
			set(FFMPEG_CONF_ARGS
				--enable-static
				--disable-shared
				--enable-gpl
				--disable-version3
				--disable-programs
				--disable-doc
				--disable-avdevice
				--disable-swscale
				--disable-postproc
				--disable-avfilter
				--disable-network
				--disable-everything
				--disable-iconv
				--disable-mediafoundation
				--disable-schannel
				--disable-sdl2
				--disable-zlib
				--disable-swscale-alpha
				--disable-amf
				--disable-cuda
				--disable-cuvid
				--disable-dxva2
				--disable-d3d11va
				--disable-ffnvcodec
				--disable-nvenc
				--disable-nvdec
				--disable-vdpau
				--disable-vulkan
				--enable-parser=${FFMPEG_CONF_PARSER}
				--enable-demuxer=${FFMPEG_CONF_DEMUXER}
				--enable-decoder=${FFMPEG_CONF_DECODER}
				--enable-swresample
				--extra-libs=-static
				--extra-cflags=--static
				--pkg-config-flags=--static
			)
			if(USE_FFMPEG_LIBOPUS)
				list(APPEND FFMPEG_CONF_ARGS
					--enable-libopus
				)
			endif()
			if(EMSCRIPTEN)
				list(APPEND FFMPEG_CONF_ARGS
					--cc=${CMAKE_C_COMPILER}
					--ranlib=${CMAKE_RANLIB}
					--enable-cross-compile
					--target-os=none
					--arch=x86
					--disable-runtime-cpudetect
					--disable-asm
					--disable-fast-unaligned
					--disable-pthreads
					--disable-w32threads
					--disable-os2threads
					--disable-debug
					--disable-stripping
					--disable-safe-bitstream-reader
				)
			endif()

			set(FFMPEG_LINK_PATH ${FFMPEG_BIN}/bin/usr/local/lib)
			set(FFMPEG_INCLUDE_DIRS ${FFMPEG_BIN}/bin/usr/local/include)

			file(MAKE_DIRECTORY ${FFMPEG_BIN})
			add_custom_target(FFMPEG_CONFIGURE
				COMMAND "${FFMPEG_PATH}/configure" ${FFMPEG_CONF_ARGS}
				BYPRODUCTS ${FFMPEG_BIN}/Makefile
				WORKING_DIRECTORY ${FFMPEG_BIN}
			)
			add_custom_target(FFMPEG_MAKE
				COMMAND make && make install DESTDIR="${FFMPEG_BIN}/bin"
				DEPENDS ${FFMPEG_BIN}/Makefile
				BYPRODUCTS ${FFMPEG_BIN}
				WORKING_DIRECTORY ${FFMPEG_BIN}
			)

			foreach(LIB avutil avformat swresample avcodec)
				add_library(${LIB} STATIC IMPORTED)
				if(NOT EXISTS ${FFMPEG_LINK_PATH}/lib${LIB}.a)
					add_dependencies(${LIB} FFMPEG_MAKE)
				endif()
				set_target_properties(${LIB} PROPERTIES
					IMPORTED_LOCATION ${FFMPEG_LINK_PATH}/lib${LIB}.a
				)
			endforeach()
		endif()
	endif()
endif()
if(NOT USE_FFMPEG)
	unset(FFMPEG_SOURCE)
endif()
