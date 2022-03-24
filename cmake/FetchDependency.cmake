set(FetchContent_INCLUDED FALSE)
if(NOT WIN32)
	find_package(Git QUIET)
	find_package(Subversion QUIET)
	# no FetchContent until 3.11
	if((NOT ${CMAKE_VERSION} VERSION_LESS "3.11.0") AND (Git_FOUND OR Subversion_FOUND))
		include(FetchContent)
		set(FetchContent_INCLUDED TRUE)
	endif()
endif()

function(FetchDependency name)
	set(value_args
		DIR
		
		GIT_REPOSITORY
		GIT_TAG
		GIT_UNSHALLOW
		
		SVN_REPOSITORY
		SVN_REVISION
		
		FILE_DOWNLOAD
		FILE_SUBDIR
	)
	set(multi_value_args
		FETCH_PRIORITY
	)
	cmake_parse_arguments(ARGS "" "${value_args}" "${multi_value_args}" ${ARGN})
	
	if(NOT ARGS_DIR)
		set(ARGS_DIR ${name})
	endif()
	if(NOT ARGS_FETCH_PRIORITY)
		set(ARGS_FETCH_PRIORITY git svn file)
	endif()
	list(APPEND ARGS_FETCH_PRIORITY none)
	
	set(${name}_BIN ${VGM_BINARY_DIR}/dependencies/${ARGS_DIR})
	set(${name}_BIN ${${name}_BIN} PARENT_SCOPE)
	
	if(${name}_PATH)
		if(IS_DIRECTORY "${${name}_PATH}")
			set(${name}_SOURCE "(local path)" PARENT_SCOPE)
		else()
			message(FATAL_ERROR "The provided path to ${ARGS_DIR} does not exist (Use ${name}_PATH)")
		endif()
	elseif(FetchContent_INCLUDED AND ((Subversion_FOUND AND ARGS_SVN_REPOSITORY) OR (Git_FOUND AND ARGS_GIT_REPOSITORY) OR ((NOT ${CMAKE_VERSION} VERSION_LESS "3.18.0") AND ARGS_FILE_DOWNLOAD)))
		set(${name}_PATH ${VGM_SOURCE_DIR}/dependencies/${ARGS_DIR})
		set(${name}_PATH ${${name}_PATH} PARENT_SCOPE)
		set(${name}_SOURCE "(download)" PARENT_SCOPE)
		
		foreach(CURRENT_FETCH ${ARGS_FETCH_PRIORITY})
			if(CURRENT_FETCH STREQUAL "git" AND Git_FOUND AND ARGS_GIT_REPOSITORY)
				if(ARGS_GIT_UNSHALLOW)
					set(shallow OFF)
				else()
					set(shallow ON)
				endif()
				FetchContent_Declare(${name}_FETCH
					GIT_REPOSITORY ${ARGS_GIT_REPOSITORY}
					SOURCE_DIR ${${name}_PATH}
					BINARY_DIR ${${name}_BIN}
					SUBBUILD_DIR ${${name}_BIN}-sub
					GIT_TAG ${ARGS_GIT_TAG}
					GIT_SHALLOW ${shallow}
				)
				FetchContent_GetProperties(${name}_FETCH)
				if(NOT ${name}_FETCH_POPULATED)
					message("Downloading ${ARGS_DIR} (git)...")
					FetchContent_Populate(${name}_FETCH)
				endif()
				break()
				
			elseif(CURRENT_FETCH STREQUAL "svn" AND Subversion_FOUND AND ARGS_SVN_REPOSITORY)
				FetchContent_Declare(${name}_FETCH
					SVN_REPOSITORY ${ARGS_SVN_REPOSITORY}
					SOURCE_DIR ${${name}_PATH}
					BINARY_DIR ${${name}_BIN}
					SUBBUILD_DIR ${${name}_BIN}-sub
					SVN_REVISION ${ARGS_SVN_REVISION}
				)
				FetchContent_GetProperties(${name}_FETCH)
				if(NOT ${name}_FETCH_POPULATED)
					message("Downloading ${ARGS_DIR} (svn)...")
					FetchContent_Populate(${name}_FETCH)
				endif()
				break()
				
			elseif(CURRENT_FETCH STREQUAL "file" AND (NOT ${CMAKE_VERSION} VERSION_LESS "3.18.0") AND ARGS_FILE_DOWNLOAD)
				# no ARCHIVE_EXTRACT until 3.18
				string(REGEX REPLACE ".*/" "" FILE ${ARGS_FILE_DOWNLOAD})
				if(NOT EXISTS ${${name}_PATH}/${FILE})
					message("Downloading ${ARGS_DIR} (file)...")
					file(DOWNLOAD
						${ARGS_FILE_DOWNLOAD}
						${${name}_PATH}/${FILE}
					)
					file(SIZE ${${name}_PATH}/${FILE} FILE_SIZE)
					if(FILE_SIZE EQUAL 0)
						message("The download of ${ARGS_DIR} (file) failed")
						continue()
					endif()
					file(ARCHIVE_EXTRACT
						INPUT ${${name}_PATH}/${FILE}
						DESTINATION ${${name}_PATH}
					)
				endif()
				if(ARGS_FILE_SUBDIR)
					set(${name}_PATH ${${name}_PATH}/${ARGS_FILE_SUBDIR} PARENT_SCOPE)
				endif()
				break()
			elseif(CURRENT_FETCH STREQUAL "none")
				set(${name}_PATH "" PARENT_SCOPE)
				set(USE_${name} OFF PARENT_SCOPE)
			endif()
		endforeach()
	else()
		set(${name}_PATH "" PARENT_SCOPE)
		set(USE_${name} OFF PARENT_SCOPE)
	endif()
endfunction()
