if(NOT WIN32 AND USE_JANSSON)
	if(NOT JANSSON_PATH AND NOT EMSCRIPTEN)
		find_package(PkgConfig REQUIRED)
		pkg_check_modules(JANSSON jansson>=2.3)
		
		if(JANSSON_FOUND)
			set(JANSSON_SOURCE "(system)")
			set(JANSSON_PKG libjansson)
			link_directories(${JANSSON_LIBRARY_DIRS})
		endif()
	endif()
	if(JANSSON_PATH OR EMSCRIPTEN OR NOT JANSSON_FOUND)
		FetchDependency(JANSSON
			DIR jansson
			GIT_REPOSITORY https://github.com/akheron/jansson
			GIT_TAG 684e18c927e89615c2d501737e90018f4930d6c5
		)
		
		if(JANSSON_PATH)
			set(JANSSON_LINK_PATH ${JANSSON_BIN}/src/.libs/libjansson.a)
			set(JANSSON_INCLUDE_DIRS ${JANSSON_PATH}/src)
			
			if(NOT EXISTS ${JANSSON_PATH}/configure)
				add_custom_target(JANSSON_AUTORECONF
					COMMAND autoreconf -iv
					BYPRODUCTS ${JANSSON_PATH}/configure
					WORKING_DIRECTORY ${JANSSON_PATH}
				)
			endif()
			
			file(MAKE_DIRECTORY ${JANSSON_BIN})
			add_custom_target(JANSSON_CONFIGURE
				COMMAND "${JANSSON_PATH}/configure" --enable-static --disable-shared CC="${CMAKE_C_COMPILER}" AR="${CMAKE_AR}" RANLIB="${CMAKE_RANLIB}"
				DEPENDS ${JANSSON_PATH}/configure
				BYPRODUCTS ${JANSSON_BIN}/Makefile
				WORKING_DIRECTORY ${JANSSON_BIN}
			)
			add_custom_target(JANSSON_MAKE
				COMMAND make
				DEPENDS ${JANSSON_BIN}/Makefile
				BYPRODUCTS ${JANSSON_LINK_PATH} ${JANSSON_BIN}
				WORKING_DIRECTORY ${JANSSON_BIN}
			)
			
			add_library(jansson STATIC IMPORTED)
			if(NOT EXISTS ${JANSSON_LINK_PATH})
				add_dependencies(jansson JANSSON_MAKE)
			endif()
			set_target_properties(jansson PROPERTIES
				IMPORTED_LOCATION ${JANSSON_LINK_PATH}
			)
		endif()
	endif()
endif()
if(NOT USE_JANSSON)
	unset(JANSSON_SOURCE)
endif()
