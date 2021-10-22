# Setup a target to have the proper preprocessor definitions, include directories, dependencies and link libraries
# Can take either 1 argument, the target, or 2 arguments, the target and a boolean that, if set to TRUE, will add the link libraries
macro(setup_target TARGET)
	set(LINK FALSE)
	if(${ARGC} GREATER 1)
		if(${ARGV1})
			set(LINK TRUE)
		endif()
	endif()

	# Common preprocessor definitions and include directories
	if(WIN32)
		target_compile_definitions(${TARGET} PRIVATE
			_WIN32_WINNT=0x501
			_CRT_SECURE_NO_WARNINGS)
	endif()
	target_include_directories(${TARGET} PRIVATE ${VGM_SOURCE_DIR}/src)
	target_include_directories(${TARGET} PRIVATE ${VGM_SOURCE_DIR}/ext_includes)
	# Set up position-independent code for all targets
	set_target_properties(${TARGET} PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
	if(BUILD_STATIC)
		set_target_properties(${TARGET} PROPERTIES LINK_SEARCH_END_STATIC 1)
	endif()
	if(NOT WIN32 AND LINK)
		# Include libm on non-Windows systems
		target_link_libraries(${TARGET} m)
	endif()

	target_compile_definitions(${TARGET} PRIVATE VGM_LOG_OUTPUT)

	if(USE_MPEG)
		target_compile_definitions(${TARGET} PRIVATE VGM_USE_MPEG)
		if(WIN32)
			if(LINK)
				add_dependencies(${TARGET} libmpg123)
				target_link_libraries(${TARGET} ${VGM_BINARY_DIR}/ext_libs/libmpg123-0.lib)
			endif()
		else()
			target_include_directories(${TARGET} PRIVATE ${MPG123_INCLUDE_DIR})
			if(LINK)
				target_link_libraries(${TARGET} mpg123 m)
			endif()
		endif()
	endif()

	if(USE_VORBIS)
		target_compile_definitions(${TARGET} PRIVATE VGM_USE_VORBIS)
		if(WIN32)
			if(LINK)
				add_dependencies(${TARGET} libvorbis)
				target_link_libraries(${TARGET} ${VGM_BINARY_DIR}/ext_libs/libvorbis.lib)
			endif()
		else()
			target_include_directories(${TARGET} PRIVATE ${VORBISFILE_INCLUDE_DIRS})
			if(LINK)
				target_link_libraries(${TARGET} vorbisfile vorbis ogg m)
			endif()
		endif()
	endif()

	if(USE_FFMPEG)
		target_compile_definitions(${TARGET} PRIVATE VGM_USE_FFMPEG)
		if(WIN32 AND NOT FFMPEG_LIBRARIES)
			target_include_directories(${TARGET} PRIVATE ${VGM_SOURCE_DIR}/ext_includes/ffmpeg)
			if(LINK)
				add_dependencies(${TARGET} ffmpeg)
				target_link_libraries(${TARGET}
					${VGM_BINARY_DIR}/ext_libs/avcodec.lib
					${VGM_BINARY_DIR}/ext_libs/avformat.lib
					${VGM_BINARY_DIR}/ext_libs/avutil.lib)
			endif()
		else()
			target_include_directories(${TARGET} PRIVATE ${FFMPEG_INCLUDE_DIRS})
			if(LINK)
				if(FFMPEG_COMPILE)
					if(USE_FFMPEG_LIBOPUS)
						target_link_libraries(${TARGET} avformat avcodec avutil swresample opus pthread m)
					else()
						target_link_libraries(${TARGET} avformat avcodec avutil swresample pthread m)
					endif()
				else()
					target_link_libraries(${TARGET} ${FFMPEG_LIBRARIES})
				endif()
			endif()
		endif()
	endif()

	if(USE_G7221)
		target_compile_definitions(${TARGET} PRIVATE VGM_USE_G7221)
	endif()

	if(USE_G719)
		target_compile_definitions(${TARGET} PRIVATE VGM_USE_G719)
		if(WIN32 AND LINK)
			add_dependencies(${TARGET} libg719_decode)
			target_link_libraries(${TARGET} ${VGM_BINARY_DIR}/ext_libs/libg719_decode.lib)
		endif()
		if(NOT WIN32 AND LINK)
			target_link_libraries(${TARGET} g719_decode)
		endif()
	endif()

	if(USE_MAIATRAC3PLUS)
		target_compile_definitions(${TARGET} PRIVATE VGM_USE_MAIATRAC3PLUS)
		target_include_directories(${TARGET} PRIVATE ${MAIATRAC3PLUS_PATH}/MaiAT3PlusDecoder)
		if(LINK)
			target_link_libraries(${TARGET} at3plusdecoder)
		endif()
	endif()

	if(USE_ATRAC9)
		target_compile_definitions(${TARGET} PRIVATE VGM_USE_ATRAC9)
		if(WIN32 AND LINK)
			add_dependencies(${TARGET} libatrac9)
			target_link_libraries(${TARGET} ${VGM_BINARY_DIR}/ext_libs/libatrac9.lib)
		endif()
		if(NOT WIN32 AND LINK)
			target_link_libraries(${TARGET} atrac9 m)
		endif()
	endif()

	if(USE_CELT)
		target_compile_definitions(${TARGET} PRIVATE VGM_USE_CELT)
		if(WIN32 AND LINK)
			add_dependencies(${TARGET} libcelt)
			target_link_libraries(${TARGET}
				${VGM_BINARY_DIR}/ext_libs/libcelt-0061.lib
				${VGM_BINARY_DIR}/ext_libs/libcelt-0110.lib)
		endif()
		if(NOT WIN32 AND LINK)
			target_link_libraries(${TARGET} celt0061 celt0110 m)
		endif()
	endif()

    if(USE_SPEEX)
        target_compile_definitions(${TARGET} PRIVATE VGM_USE_SPEEX)
        if(WIN32 AND LINK)
            add_dependencies(${TARGET} libspeex)
            target_link_libraries(${TARGET} ${VGM_BINARY_DIR}/ext_libs/libspeex/libspeex.lib)
        endif()
		if(NOT WIN32 AND LINK)
			target_link_libraries(${TARGET} speex m)
		endif()
    endif()
endmacro()

# Installs the DLLs to the given install prefix
macro(install_dlls INSTALL_PREFIX)
	# Paths to the DLLs
	set(MPEG_DLL ${VGM_SOURCE_DIR}/ext_libs/libmpg123-0.dll)
	set(VORBIS_DLL ${VGM_SOURCE_DIR}/ext_libs/libvorbis.dll)
	set(G719_DLL ${VGM_SOURCE_DIR}/ext_libs/libg719_decode.dll)
	set(FFMPEG_DLL
		${VGM_SOURCE_DIR}/ext_libs/avcodec-vgmstream-58.dll
		${VGM_SOURCE_DIR}/ext_libs/avformat-vgmstream-58.dll
		${VGM_SOURCE_DIR}/ext_libs/avutil-vgmstream-56.dll
		${VGM_SOURCE_DIR}/ext_libs/swresample-vgmstream-3.dll)
	set(ATRAC9_DLL ${VGM_SOURCE_DIR}/ext_libs/libatrac9.dll)
	set(CELT_DLL
		${VGM_SOURCE_DIR}/ext_libs/libcelt-0061.dll
		${VGM_SOURCE_DIR}/ext_libs/libcelt-0110.dll)
    set(SPEEX_DLL ${VGM_SOURCE_DIR}/ext_libs/libspeex/libspeex.dll)

	# List of DLLs to check for install
	set(DLLS
		MPEG
		VORBIS
		G7221
		G719
		FFMPEG
		ATRAC9
		CELT
		SPEEX)

	# Loop over DLLs and only install if the USE_* is set for that DLL
	foreach(DLL ${DLLS})
		if(${USE_${DLL}})
			install(FILES ${${DLL}_DLL}
				DESTINATION ${INSTALL_PREFIX})
		endif()
	endforeach()
endmacro()
