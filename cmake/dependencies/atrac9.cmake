if(NOT WIN32 AND USE_ATRAC9)
	FetchDependency(ATRAC9
		DIR LibAtrac9
		GIT_REPOSITORY https://github.com/Thealexbarney/LibAtrac9
		GIT_TAG 6a9e00f6c7abd74d037fd210b6670d3cdb313049
	)
	
	if(ATRAC9_PATH)
		if(EMSCRIPTEN)
			set(ATRAC9_LINK_PATH ${ATRAC9_BIN}/embin/libatrac9.a)
		else()
			set(ATRAC9_LINK_PATH ${ATRAC9_BIN}/bin/libatrac9.a)
		endif()
		if(NOT EXISTS ${ATRAC9_LINK_PATH})
			if(EMSCRIPTEN)
				add_custom_target(ATRAC9_MAKE ALL
					COMMAND emmake make static CFLAGS="-fPIC" CC=emcc AR=emar BINDIR="${ATRAC9_BIN}/embin" && make clean
					WORKING_DIRECTORY ${ATRAC9_PATH}/C
				)
			else()
				add_custom_target(ATRAC9_MAKE ALL
					COMMAND make static CFLAGS="-fPIC" BINDIR="${ATRAC9_BIN}/bin" && make clean
					WORKING_DIRECTORY ${ATRAC9_PATH}/C
				)
			endif()
		endif()
		
		add_library(atrac9 STATIC IMPORTED)
		set_target_properties(atrac9 PROPERTIES
			IMPORTED_LOCATION ${ATRAC9_LINK_PATH}
		)
	endif()
endif()
if(NOT USE_ATRAC9)
	unset(ATRAC9_SOURCE)
endif()
