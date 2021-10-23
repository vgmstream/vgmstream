if(NOT WIN32 AND USE_VORBIS)
	if(NOT VORBIS_PATH)
		find_package(VorbisFile QUIET)
		
		if(VORBISFILE_FOUND)
			set(VORBIS_SOURCE "(system)")
		endif()
	endif()
	if(VORBIS_PATH AND (OGG_PATH OR OGG_FOUND) OR NOT VORBISFILE_FOUND)
		FetchDependency(VORBIS
			DIR vorbis
			GIT_REPOSITORY https://gitlab.xiph.org/xiph/vorbis
			GIT_TAG v1.3.7
		)
		
		if(VORBIS_PATH)
			set(VORBIS_LINK_PATH ${VORBIS_BIN}/lib/libvorbis.a)
			set(VORBISFILE_LINK_PATH ${VORBIS_BIN}/lib/libvorbisfile.a)
			
			if(EXISTS ${VORBIS_LINK_PATH} AND EXISTS ${VORBISFILE_LINK_PATH})
				add_library(vorbis STATIC IMPORTED)
				set_target_properties(vorbis PROPERTIES
					IMPORTED_LOCATION ${VORBIS_LINK_PATH}
				)
				add_library(vorbisfile STATIC IMPORTED)
				set_target_properties(vorbisfile PROPERTIES
					IMPORTED_LOCATION ${VORBISFILE_LINK_PATH}
				)
			else()
				add_subdirectory(${VORBIS_PATH} ${VORBIS_BIN} EXCLUDE_FROM_ALL)
			endif()
			
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
