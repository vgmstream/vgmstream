if(NOT WIN32 AND USE_MPEG)
	if(NOT MPEG_PATH AND NOT BUILD_STATIC)
		find_package(MPG123 QUIET)
		
		if(MPG123_FOUND)
			set(MPEG_SOURCE "(system)")
		endif()
	endif()
	if(MPEG_PATH OR BUILD_STATIC OR NOT MPG123_FOUND)
		if(${CMAKE_VERSION} VERSION_LESS "3.18.0")
			# no ARCHIVE_EXTRACT until 3.18
			FetchDependency(MPEG
				DIR mpg123
				# "official" git repo: https://www.mpg123.de/trunk/.git/ but *very* slow (HTTP emulation)
				# (original is svn://scm.orgis.org/mpg123/trunk, FetchDependency would need SVN_REPOSITORY and stuff)
				# "official" git mirror (default branch is not master), works too
				# not sure how to set branch + commit fedb989a4d300199f09757815409d3a89b8bc63 (v1.28.2)
				GIT_REPOSITORY https://github.com/madebr/mpg123
				GIT_TAG master
			)
			set(MPEG_BUILD_COMMAND autoreconf -iv && ./configure --enable-static=yes --enable-shared=no && make && make install DESTDIR="${MPEG_BIN}/bin" && make clean)
		else()
			FetchDependency(MPEG
				DIR mpg123
				DOWNLOAD https://downloads.sourceforge.net/mpg123/mpg123-1.28.2.tar.bz2
				SUBDIR mpg123-1.28.2
			)
			set(MPEG_BUILD_COMMAND ./configure --enable-static=yes --enable-shared=no && make && make install DESTDIR="${MPEG_BIN}/bin" && make clean)
		endif()

		if(MPEG_PATH)
			set(MPEG_LINK_PATH ${MPEG_BIN}/bin/usr/local/lib/libmpg123.a)
			set(MPG123_INCLUDE_DIR ${MPEG_BIN}/bin/usr/local/include)
			
			if(NOT EXISTS ${MPEG_LINK_PATH})
				add_custom_target(MPEG_MAKE ALL
					COMMAND ${MPEG_BUILD_COMMAND}
					WORKING_DIRECTORY ${MPEG_PATH}
				)
			endif()
			
			add_library(mpg123 STATIC IMPORTED)
			set_target_properties(mpg123 PROPERTIES
				IMPORTED_LOCATION ${MPEG_LINK_PATH}
			)
		endif()
	endif()
endif()
if(NOT USE_MPEG)
	unset(MPEG_SOURCE)
endif()
