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
			GIT_TAG 870ff845b32f314aec0036641ffe18aba4916887
		)
		
		if(SPEEX_PATH)
			if(EMSCRIPTEN)
				set(SPEEX_LINK_PATH ${SPEEX_BIN}/embin/usr/local/lib/libspeex.a)
			else()
				set(SPEEX_LINK_PATH ${SPEEX_BIN}/bin/usr/local/lib/libspeex.a)
			endif()
			if(NOT EXISTS ${SPEEX_LINK_PATH})
				if(EMSCRIPTEN)
					add_custom_target(SPEEX_MAKE ALL
						COMMAND emconfigure ./autogen.sh && emconfigure ./configure --enable-static=yes --enable-shared=no && emmake make && make install DESTDIR="${SPEEX_BIN}/embin" && make clean
						WORKING_DIRECTORY ${SPEEX_PATH}
					)
				else()
					add_custom_target(SPEEX_MAKE ALL
						COMMAND ./autogen.sh && ./configure --enable-static=yes --enable-shared=no && make && make install DESTDIR="${SPEEX_BIN}/bin" && make clean
						WORKING_DIRECTORY ${SPEEX_PATH}
					)
				endif()
			endif()
			
			add_library(speex STATIC IMPORTED)
			set_target_properties(speex PROPERTIES
				IMPORTED_LOCATION ${SPEEX_LINK_PATH}
			)
		endif()
	endif()
endif()
if(NOT USE_SPEEX)
	unset(SPEEX_SOURCE)
endif()
