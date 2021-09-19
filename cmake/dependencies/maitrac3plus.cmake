if(USE_MAIATRAC3PLUS)
	FetchDependency(MAIATRAC3PLUS
		DIR maiatrac3plus
		GIT_REPOSITORY https://github.com/emulibraries/maiatrac3plus
		GIT_TAG 95c34f2651b0988a8ed762692925eb22700e9b42
	)
	
	if(MAIATRAC3PLUS_PATH)
		configure_file(
			${VGM_SOURCE_DIR}/ext_libs/maiatrac3plus/CMakeLists.txt
			${MAIATRAC3PLUS_PATH}/MaiAT3PlusDecoder/CMakeLists.txt
		COPYONLY)
		configure_file(
			${VGM_SOURCE_DIR}/ext_libs/maiatrac3plus/maiatrac3plus.h
			${MAIATRAC3PLUS_PATH}/MaiAT3PlusDecoder/maiatrac3plus.h
		COPYONLY)
		configure_file(
			${MAIATRAC3PLUS_PATH}/MaiAT3PlusDecoder/src/base/Mai_mem.cc
			${MAIATRAC3PLUS_PATH}/MaiAT3PlusDecoder/src/base/Mai_Mem.cc
		COPYONLY)
		add_subdirectory(${MAIATRAC3PLUS_PATH}/MaiAT3PlusDecoder ${MAIATRAC3PLUS_BIN})
	endif()
endif()
if(NOT USE_MAIATRAC3PLUS)
	unset(MAIATRAC3PLUS_SOURCE)
endif()
