if(NOT WIN32 AND USE_G719)
	FetchDependency(G719
		DIR libg719_decode
		GIT_REPOSITORY https://github.com/kode54/libg719_decode
		GIT_TAG 9bd89f89df4a5c0e9f178c173fc55d373f039bcf
	)
	
	if(G719_PATH)
		set(G719_LINK_PATH ${G719_BIN}/libg719_decode.a)
		
		configure_file(
			${VGM_SOURCE_DIR}/ext_libs/libg719_decode/CMakeLists.txt
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
