if(NOT WIN32 AND USE_VORBIS)
	if(NOT VORBIS_PATH OR NOT OGG_PATH)
		find_package(VorbisFile QUIET)
		
		if(VORBISFILE_FOUND)
			set(VORBIS_SOURCE "(system)")
		endif()
	endif()
	if(VORBIS_PATH AND OGG_PATH OR NOT VORBISFILE_FOUND)
		FetchDependency(OGG
			DIR ogg
			GIT_REPOSITORY https://gitlab.xiph.org/xiph/ogg
			GIT_TAG v1.3.5
		)
		FetchDependency(VORBIS
			DIR vorbis
			GIT_REPOSITORY https://gitlab.xiph.org/xiph/vorbis
			GIT_TAG v1.3.7
		)
		
		if(OGG_PATH AND VORBIS_PATH)
			add_subdirectory(${OGG_PATH} ${OGG_BIN})
			set(OGG_INCLUDE_DIR ${OGG_PATH}/include ${OGG_BIN}/include)
			set(OGG_LIBRARY ogg)
			
			add_subdirectory(${VORBIS_PATH} ${VORBIS_BIN})
			set(OGG_VORBIS_INCLUDE_DIR ${VORBIS_PATH}/include)
			set(OGG_VORBIS_LIBRARY vorbis)
			
			set(VORBISFILE_INCLUDE_DIRS ${OGG_INCLUDE_DIR} ${OGG_VORBIS_INCLUDE_DIR})
		else()
			set_vorbis(OFF TRUE)
		endif()
	endif()
endif()
if(NOT USE_VORBIS)
	unset(VORBIS_SOURCE)
endif()
