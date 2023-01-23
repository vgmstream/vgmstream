if(NOT WIN32 AND USE_ATRAC9)
	FetchDependency(ATRAC9
		DIR LibAtrac9
		GIT_REPOSITORY https://github.com/Thealexbarney/LibAtrac9
		GIT_TAG 6a9e00f6c7abd74d037fd210b6670d3cdb313049
		GIT_UNSHALLOW ON
	)
	
	if(ATRAC9_PATH)
		set(ATRAC9_LINK_PATH ${ATRAC9_BIN}/bin/libatrac9.a)
		
		add_custom_target(ATRAC9_MAKE
			COMMAND make static CFLAGS="-fPIC" OBJDIR="${ATRAC9_BIN}/obj" BINDIR="${ATRAC9_BIN}/bin" CC="${CMAKE_C_COMPILER}" AR="${CMAKE_AR}"
			WORKING_DIRECTORY ${ATRAC9_PATH}/C
			BYPRODUCTS ${ATRAC9_LINK_PATH} ${ATRAC9_BIN}
		)
		
		add_library(atrac9 STATIC IMPORTED)
		if(NOT EXISTS ${ATRAC9_LINK_PATH})
			add_dependencies(atrac9 ATRAC9_MAKE)
		endif()
		set_target_properties(atrac9 PROPERTIES
			IMPORTED_LOCATION ${ATRAC9_LINK_PATH}
		)
	endif()
endif()
if(NOT USE_ATRAC9)
	unset(ATRAC9_SOURCE)
endif()
