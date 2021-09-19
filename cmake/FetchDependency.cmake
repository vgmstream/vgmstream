if(NOT WIN32)
	find_package(Git QUIET)
	if(Git_FOUND)
		include(FetchContent)
	endif()
endif()

function(FetchDependency name)
	set(value_args DIR GIT_REPOSITORY GIT_TAG)
	cmake_parse_arguments(ARGS "" "${value_args}" "" ${ARGN})
	
	if(NOT ARGS_DIR)
		set(ARGS_DIR ${name})
	endif()
	
	set(${name}_BIN ${VGM_BINARY_DIR}/dependencies/${ARGS_DIR})
	set(${name}_BIN ${${name}_BIN} PARENT_SCOPE)
	
	if(${name}_PATH AND IS_DIRECTORY "${${name}_PATH}")
		set(${name}_SOURCE "(local path)" PARENT_SCOPE)
	elseif(Git_FOUND AND ARGS_GIT_REPOSITORY)
		set(${name}_PATH ${VGM_SOURCE_DIR}/dependencies/${ARGS_DIR})
		set(${name}_PATH ${${name}_PATH} PARENT_SCOPE)
		set(${name}_SOURCE "(download)" PARENT_SCOPE)
		
		FetchContent_Declare(${name}_FETCH
			GIT_REPOSITORY ${ARGS_GIT_REPOSITORY}
			SOURCE_DIR ${${name}_PATH}
			BINARY_DIR ${${name}_BIN}
			SUBBUILD_DIR ${${name}_BIN}-sub
			GIT_TAG ${ARGS_GIT_TAG}
			GIT_SHALLOW ON
		)
		FetchContent_GetProperties(${name}_FETCH)
		if(NOT ${name}_FETCH_POPULATED)
			message("Downloading ${ARGS_DIR}...")
			FetchContent_Populate(${name}_FETCH)
		endif()
	else()
		set(${name}_PATH "" PARENT_SCOPE)
		set(USE_${name} OFF PARENT_SCOPE)
	endif()
endfunction()
