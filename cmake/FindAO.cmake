if(AO_INCLUDE_DIR)
	# Already in cache, be silent
	set(AO_FIND_QUIETLY TRUE)
endif(AO_INCLUDE_DIR)

find_path(AO_INCLUDE_DIR ao/ao.h
	/opt/local/include
	/usr/local/include
	/usr/include
)

set(AO_NAMES ao)
find_library(AO_LIBRARY
	NAMES ${AO_NAMES}
	PATHS /usr/lib /usr/local/lib /opt/local/lib
)

if(AO_INCLUDE_DIR AND AO_LIBRARY)
	set(AO_FOUND TRUE)
	set(AO_LIBRARIES ${AO_LIBRARY})
else(AO_INCLUDE_DIR AND AO_LIBRARY)
	set(AO_FOUND FALSE)
	set(AO_LIBRARIES)
endif(AO_INCLUDE_DIR AND AO_LIBRARY)

if(AO_FOUND)
	if(NOT AO_FIND_QUIETLY)
		message(STATUS "Found AO: ${AO_LIBRARY}")
	endif(NOT AO_FIND_QUIETLY)
else(AO_FOUND)
	if(AO_FIND_REQUIRED)
		message(STATUS "Looked for ao libraries named ${AO_NAMES}.")
		message(STATUS "Include file detected: [${AO_INCLUDE_DIR}].")
		message(STATUS "Lib file detected: [${AO_LIBRARY}].")
		message(FATAL_ERROR "=========> Could NOT find ao library")
	endif(AO_FIND_REQUIRED)
endif(AO_FOUND)

mark_as_advanced(
	AO_LIBRARY
	AO_INCLUDE_DIR
)
