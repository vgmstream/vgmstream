if(NOT WIN32 AND USE_G719)
	FetchDependency(G719
		DIR libg719_decode
		GIT_REPOSITORY https://github.com/kode54/libg719_decode
		GIT_TAG da90ad8a676876c6c47889bcea6a753f9bbf7a73
		GIT_UNSHALLOW ON
	)
	
	if(G719_PATH)
		set(G719_LINK_PATH ${G719_BIN}/libg719_decode.a)
		
		configure_file(
			${VGM_SOURCE_DIR}/ext_libs/extra/libg719_decode/CMakeLists.txt
			${G719_PATH}/CMakeLists.txt
		COPYONLY)
		
		if(EXISTS ${G719_LINK_PATH})
			add_library(g719_decode STATIC IMPORTED)
			set_target_properties(g719_decode PROPERTIES
				IMPORTED_LOCATION ${G719_LINK_PATH}
			)
		else()
			add_subdirectory(${G719_PATH} ${G719_BIN} EXCLUDE_FROM_ALL)
		endif()
	endif()
endif()
if(NOT USE_G719)
	unset(G719_SOURCE)
endif()
