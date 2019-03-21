# - Find mpg123
# Find the native mpg123 includes and library
#
#  MPG123_INCLUDE_DIR - where to find mpg123.h
#  MPG123_LIBRARIES   - List of libraries when using mpg123.
#  MPG123_FOUND       - True if mpg123 found.

# Comes from https://github.com/coelckers/gzdoom/blob/master/cmake/FindMPG123.cmake

if(MPG123_INCLUDE_DIR AND MPG123_LIBRARIES)
	# Already in cache, be silent
	set(MPG123_FIND_QUIETLY TRUE)
endif()

find_path(MPG123_INCLUDE_DIR mpg123.h PATHS "${MPG123_DIR}" PATH_SUFFIXES include)

find_library(MPG123_LIBRARIES NAMES mpg123 mpg123-0 PATHS "${MPG123_DIR}" PATH_SUFFIXES lib)

# MARK_AS_ADVANCED(MPG123_LIBRARIES MPG123_INCLUDE_DIR)

# handle the QUIETLY and REQUIRED arguments and set MPG123_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MPG123 DEFAULT_MSG MPG123_LIBRARIES MPG123_INCLUDE_DIR)
