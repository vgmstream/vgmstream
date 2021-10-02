if(NOT WIN32 AND BUILD_V123)
	if(NOT LIBAO_PATH AND NOT BUILD_STATIC)
		find_package(AO QUIET)
		
		if(AO_FOUND)
			set(LIBAO_SOURCE "(system)")
			set(LIBAO_INCLUDE ${AO_INCLUDE_DIR})
		endif()
	endif()
	
	if(LIBAO_PATH OR BUILD_STATIC OR NOT AO_FOUND)
		FetchDependency(LIBAO
			DIR libao
		)
		
		if(LIBAO_PATH)
			if(BUILD_STATIC)
				set(LIBAO_CONF_ARGS --enable-static --disable-shared)
				set(LINK_EXT .a)
			else()
				set(LIBAO_CONF_ARGS --disable-static --enable-shared)
				set(LINK_EXT .so)
			endif()
			set(LIBAO_LINK_PATH ${LIBAO_BIN}/bin/usr/local/lib/libao${LINK_EXT})
			set(LIBAO_INCLUDE ${LIBAO_BIN}/bin/usr/local/include)
			
			if(NOT EXISTS ${LIBAO_PATH}/configure)
				add_custom_target(LIBAO_AUTORECONF
					COMMAND ./autogen.sh
					BYPRODUCTS ${LIBAO_PATH}/configure
					WORKING_DIRECTORY ${LIBAO_PATH}
				)
			endif()
			
			file(MAKE_DIRECTORY ${LIBAO_BIN})
			add_custom_target(LIBAO_CONFIGURE
				COMMAND "${LIBAO_PATH}/configure" ${LIBAO_CONF_ARGS} CC="${CMAKE_C_COMPILER}" AR="${CMAKE_AR}"
				DEPENDS ${LIBAO_PATH}/configure
				BYPRODUCTS ${LIBAO_BIN}/Makefile
				WORKING_DIRECTORY ${LIBAO_BIN}
			)
			add_custom_target(LIBAO_MAKE
				COMMAND make && make install DESTDIR=${LIBAO_BIN}/bin
				DEPENDS ${LIBAO_BIN}/Makefile
				BYPRODUCTS ${LIBAO_LINK_PATH} ${LIBAO_BIN}
				WORKING_DIRECTORY ${LIBAO_BIN}
			)
			
			if(BUILD_STATIC)
				add_library(ao STATIC IMPORTED)
			else()
				add_library(ao SHARED IMPORTED)
			endif()
			if(NOT EXISTS ${LIBAO_LINK_PATH})
				add_dependencies(ao LIBAO_MAKE)
			endif()
			set_target_properties(ao PROPERTIES
				IMPORTED_LOCATION ${LIBAO_LINK_PATH}
			)
		elseif(BUILD_STATIC)
			message(FATAL_ERROR "Path to libao must be set. (Use LIBAO_PATH or turn off BUILD_V123)")
		else()
			set(BUILD_V123 OFF)
		endif()
	endif()
endif()
if(NOT BUILD_V123)
	unset(LIBAO_SOURCE)
endif()
