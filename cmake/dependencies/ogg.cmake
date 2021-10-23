if(NOT WIN32 AND USE_VORBIS)
	if(NOT OGG_PATH)
		find_package(Ogg QUIET)
		
		if(OGG_FOUND)
			set(OGG_SOURCE "(system)")
		endif()
	endif()
	if(OGG_PATH OR NOT OGG_FOUND)
		FetchDependency(OGG
			DIR ogg
			GIT_REPOSITORY https://gitlab.xiph.org/xiph/ogg
			GIT_TAG v1.3.5
		)
		
		if(OGG_PATH)
			set(OGG_LINK_PATH ${OGG_BIN}/libogg.a)
			
			if(EXISTS ${OGG_LINK_PATH} AND EXISTS ${OGG_BIN}/include)
				add_library(ogg STATIC IMPORTED)
				set_target_properties(ogg PROPERTIES
					IMPORTED_LOCATION ${OGG_LINK_PATH}
				)
			else()
				add_subdirectory(${OGG_PATH} ${OGG_BIN} EXCLUDE_FROM_ALL)
			endif()
			
			set(OGG_INCLUDE_DIR ${OGG_PATH}/include ${OGG_BIN}/include)
			set(OGG_LIBRARY ogg)
		endif()
	endif()
endif()
if(NOT OGG_PATH)
	unset(OGG_SOURCE)
endif()
