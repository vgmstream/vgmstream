if(NOT WIN32 AND USE_MPEG)
	if(NOT MPEG_PATH AND NOT BUILD_STATIC)
		find_package(MPG123 QUIET)
		
		if(MPG123_FOUND)
			set(MPEG_SOURCE "(system)")
		endif()
	endif()
	if(MPEG_PATH OR BUILD_STATIC OR NOT MPG123_FOUND)
		FetchDependency(MPEG
			DIR mpg123
			FETCH_PRIORITY file svn git
			
			FILE_DOWNLOAD https://downloads.sourceforge.net/mpg123/mpg123-1.31.1.tar.bz2
			FILE_SUBDIR mpg123-1.31.1
			
			# unknown current version, to be fixed
			#SVN_REPOSITORY svn://scm.orgis.org/mpg123/trunk
			#SVN_REVISION -r4968 ?
			
			# "official" git repo: https://www.mpg123.de/trunk/.git/ but *very* slow (HTTP emulation)
			# "official" git mirror (default branch is not master), works too
			GIT_REPOSITORY https://github.com/madebr/mpg123
			GIT_TAG aec901b7a636b6eb61e03a87ff3547c787e8c693
			GIT_UNSHALLOW ON
		)
		
		if(MPEG_PATH)
			set(MPEG_LINK_PATH ${MPEG_BIN}/src/libmpg123/.libs/libmpg123.a)
			set(MPG123_INCLUDE_DIR ${MPEG_PATH}/src)
			
			if(NOT EXISTS ${MPEG_PATH}/configure)
				add_custom_target(MPEG_AUTORECONF
					COMMAND autoreconf -iv
					BYPRODUCTS ${MPEG_PATH}/configure
					WORKING_DIRECTORY ${MPEG_PATH}
				)
			endif()
			
			set(MPEG_CONFIGURE
				--enable-static
				--disable-shared
				CC="${CMAKE_C_COMPILER}"
				AR="${CMAKE_AR}"
				RANLIB="${CMAKE_RANLIB}"
			)
			if(EMSCRIPTEN)
				list(APPEND MPEG_CONFIGURE
					--with-cpu=generic_fpu
				)
			endif()
			
			file(MAKE_DIRECTORY ${MPEG_BIN})
			add_custom_target(MPEG_CONFIGURE
				COMMAND "${MPEG_PATH}/configure" ${MPEG_CONFIGURE}
				DEPENDS ${MPEG_PATH}/configure
				BYPRODUCTS ${MPEG_BIN}/Makefile
				WORKING_DIRECTORY ${MPEG_BIN}
			)
			add_custom_target(MPEG_MAKE
				COMMAND make src/libmpg123/libmpg123.la
				DEPENDS ${MPEG_BIN}/Makefile
				BYPRODUCTS ${MPEG_LINK_PATH} ${MPEG_BIN}
				WORKING_DIRECTORY ${MPEG_BIN}
			)
			
			add_library(mpg123 STATIC IMPORTED)
			if(NOT EXISTS ${MPEG_LINK_PATH})
				add_dependencies(mpg123 MPEG_MAKE)
			endif()
			set_target_properties(mpg123 PROPERTIES
				IMPORTED_LOCATION ${MPEG_LINK_PATH}
			)
		endif()
	endif()
endif()
if(NOT USE_MPEG)
	unset(MPEG_SOURCE)
endif()
