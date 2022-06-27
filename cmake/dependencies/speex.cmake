if(NOT WIN32 AND USE_SPEEX)
	if(NOT SPEEX_PATH)
		find_package(Speex QUIET)
		
		if(Speex_FOUND)
			set(SPEEX_SOURCE "(system)")
		endif()
	endif()
	if(SPEEX_PATH OR NOT Speex_FOUND)
		FetchDependency(SPEEX
			DIR speex
			GIT_REPOSITORY https://gitlab.xiph.org/xiph/speex
			GIT_TAG Speex-1.2.0
		)
		
		if(SPEEX_PATH)
			set(SPEEX_LINK_PATH ${SPEEX_BIN}/libspeex/.libs/libspeex.a)
			
			if(NOT EXISTS ${SPEEX_PATH}/configure)
				add_custom_target(SPEEX_AUTORECONF
					COMMAND ./autogen.sh
					BYPRODUCTS ${SPEEX_PATH}/configure
					WORKING_DIRECTORY ${SPEEX_PATH}
				)
			endif()
			
			file(MAKE_DIRECTORY ${SPEEX_BIN})
			add_custom_target(SPEEX_CONFIGURE
				COMMAND "${SPEEX_PATH}/configure" --enable-static --disable-shared --disable-binaries --with-pic CC="${CMAKE_C_COMPILER}" AR="${CMAKE_AR}" RANLIB="${CMAKE_RANLIB}"
				DEPENDS ${SPEEX_PATH}/configure
				BYPRODUCTS ${SPEEX_BIN}/Makefile
				WORKING_DIRECTORY ${SPEEX_BIN}
			)
			add_custom_target(SPEEX_MAKE
				COMMAND make
				DEPENDS ${SPEEX_BIN}/Makefile
				BYPRODUCTS ${SPEEX_LINK_PATH} ${SPEEX_BIN}
				WORKING_DIRECTORY ${SPEEX_BIN}
			)
			
			add_library(speex STATIC IMPORTED)
			if(NOT EXISTS ${SPEEX_LINK_PATH})
				add_dependencies(speex SPEEX_MAKE)
			endif()
			set_target_properties(speex PROPERTIES
				IMPORTED_LOCATION ${SPEEX_LINK_PATH}
			)
		endif()
	endif()
endif()
if(NOT USE_SPEEX)
	unset(SPEEX_SOURCE)
endif()
