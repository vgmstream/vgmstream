if(NOT WIN32 AND USE_G719)
	FetchDependency(G719
		DIR libg719_decode
		GIT_REPOSITORY https://github.com/kode54/libg719_decode
		GIT_TAG 9bd89f89df4a5c0e9f178c173fc55d373f039bcf
	)
	
	if(G719_PATH)
		configure_file(
			${VGM_SOURCE_DIR}/ext_libs/libg719_decode/CMakeLists.txt
			${G719_PATH}/CMakeLists.txt
		COPYONLY)
		add_subdirectory(${G719_PATH} ${G719_BIN})
	endif()
endif()
if(NOT USE_G719)
	unset(G719_SOURCE)
endif()
