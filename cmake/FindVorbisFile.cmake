# - Find vorbisfile
# Find the native vorbisfile includes and libraries
#
#  VORBISFILE_INCLUDE_DIRS - where to find vorbisfile.h, etc.
#  VORBISFILE_LIBRARIES    - List of libraries when using vorbisfile.
#  VORBISFILE_FOUND        - True if vorbisfile found.

# Adapted from https://github.com/erikd/libsndfile/blob/master/cmake/FindVorbisEnc.cmake

if(VORBISFILE_INCLUDE_DIR)
	# Already in cache, be silent
	set(VORBISFILE_FIND_QUIETLY TRUE)
endif()

find_package(Vorbis QUIET)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_VORBISFILE QUIET vorbisfile)

set(VORBISFILE_VERSION ${PC_VORBISFILE_VERSION})

find_path(VORBISFILE_INCLUDE_DIR vorbis/vorbisfile.h
	HINTS
		${PC_VORBISFILE_INCLUDEDIR}
		${PC_VORBISFILE_INCLUDE_DIRS}
		${VORBISFILE_ROOT}
	)

find_library(VORBISFILE_LIBRARY
	NAMES
		vorbisfile
		vorbisfile_static
		libvorbisfile
		libvorbisfile_static
	HINTS
		${PC_VORBISFILE_LIBDIR}
		${PC_VORBISFILE_LIBRARY_DIRS}
		${VORBISFILE_ROOT}
	)

# Handle the QUIETLY and REQUIRED arguments and set VORBISFILE_FOUND
# to TRUE if all listed variables are TRUE.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(VorbisFile
	REQUIRED_VARS
		VORBISFILE_LIBRARY
		VORBISFILE_INCLUDE_DIR
		VORBIS_FOUND
	VERSION_VAR
		VORBISFILE_VERSION
	)

if(VORBISFILE_FOUND)
	set(VORBISFILE_INCLUDE_DIRS ${VORBISFILE_INCLUDE_DIR})
	set(VORBISFILE_LIBRARIES ${VORBISFILE_LIBRARY} ${VORBIS_LIBRARIES})
		if(NOT TARGET Vorbis::VorbisFile)
		add_library(Vorbis::VorbisFile UNKNOWN IMPORTED)
		set_target_properties(Vorbis::VorbisFile PROPERTIES
			INTERFACE_INCLUDE_DIRECTORIES "${VORBISFILE_INCLUDE_DIR}"
			IMPORTED_LOCATION "${VORBISFILE_LIBRARY}"
			INTERFACE_LINK_LIBRARIES Vorbis::Vorbis
		)
	endif()
endif()

mark_as_advanced(VORBISFILE_INCLUDE_DIR VORBISFILE_LIBRARY)
