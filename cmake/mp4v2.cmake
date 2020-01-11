# qaac doesn't have a Visual Studio project for Windows, so this not only allows things to build for Visual Studio, but also covers other compilers and platforms

configure_file(${QAAC_PATH}/mp4v2/libplatform/config.h.in ${VGM_BINARY_DIR}/mp4v2/include/libplatform/config.h COPYONLY)

file(GLOB INCLUDE_HEADERS "${QAAC_PATH}/mp4v2/include/mp4v2/*.h")
file(GLOB LIBPLATFORM_HEADERS "${QAAC_PATH}/mp4v2/libplatform/*.h")
file(GLOB LIBPLATFORM_IO_HEADERS "${QAAC_PATH}/mp4v2/libplatform/io/*.h")
file(GLOB LIBPLATFORM_IO_SOURCES "${QAAC_PATH}/mp4v2/libplatform/io/*.cpp")
file(GLOB LIBPLATFORM_NUMBER_HEADERS "${QAAC_PATH}/mp4v2/libplatform/number/*.h")
file(GLOB LIBPLATFORM_PROCESS_HEADERS "${QAAC_PATH}/mp4v2/libplatform/process/*.h")
file(GLOB LIBPLATFORM_PROG_HEADERS "${QAAC_PATH}/mp4v2/libplatform/prog/*.h")
list(FILTER LIBPLATFORM_PROG_HEADERS EXCLUDE REGEX ".*commandline.*")
file(GLOB LIBPLATFORM_PROG_SOURCES "${QAAC_PATH}/mp4v2/libplatform/prog/*.cpp")
list(FILTER LIBPLATFORM_PROG_SOURCES EXCLUDE REGEX ".*commandline.*")
file(GLOB LIBPLATFORM_SYS_HEADERS "${QAAC_PATH}/mp4v2/libplatform/sys/*.h")
file(GLOB LIBPLATFORM_SYS_SOURCES "${QAAC_PATH}/mp4v2/libplatform/sys/*.cpp")
file(GLOB LIBPLATFORM_TIME_HEADERS "${QAAC_PATH}/mp4v2/libplatform/time/*.h")
file(GLOB LIBPLATFORM_TIME_SOURCES "${QAAC_PATH}/mp4v2/libplatform/time/*.cpp")
file(GLOB SRC_HEADERS "${QAAC_PATH}/mp4v2/src/*.h")
file(GLOB SRC_SOURCES "${QAAC_PATH}/mp4v2/src/*.cpp")
file(GLOB SRC_BMFF_HEADERS "${QAAC_PATH}/mp4v2/src/bmff/*.h")
file(GLOB SRC_BMFF_SOURCES "${QAAC_PATH}/mp4v2/src/bmff/*.cpp")
file(GLOB SRC_ITMF_HEADERS "${QAAC_PATH}/mp4v2/src/itmf/*.h")
file(GLOB SRC_ITMF_SOURCES "${QAAC_PATH}/mp4v2/src/itmf/*.cpp")
file(GLOB SRC_QTFF_HEADERS "${QAAC_PATH}/mp4v2/src/qtff/*.h")
file(GLOB SRC_QTFF_SOURCES "${QAAC_PATH}/mp4v2/src/qtff/*.cpp")

if(WIN32)
	list(FILTER LIBPLATFORM_HEADERS EXCLUDE REGEX ".*posix.*")
	set(LIBPLATFORM_SOURCES
		${QAAC_PATH}/mp4v2/libplatform/platform_win32.cpp)
	list(FILTER LIBPLATFORM_IO_SOURCES EXCLUDE REGEX ".*posix.*")
	set(LIBPLATFORM_NUMBER_SOURCES
		${QAAC_PATH}/mp4v2/libplatform/number/random_win32.cpp)
	set(LIBPLATFORM_PROCESS_SOURCES
		${QAAC_PATH}/mp4v2/libplatform/process/process_win32.cpp)
	list(FILTER LIBPLATFORM_TIME_SOURCES EXCLUDE REGEX ".*posix.*")
else()
	list(FILTER LIBPLATFORM_HEADERS EXCLUDE REGEX ".*win32.*")
	list(FILTER LIBPLATFORM_IO_SOURCES EXCLUDE REGEX ".*win32.*")
	set(LIBPLATFORM_NUMBER_SOURCES
		${QAAC_PATH}/mp4v2/libplatform/number/random_posix.cpp)
	set(LIBPLATFORM_PROCESS_SOURCES
		${QAAC_PATH}/mp4v2/libplatform/process/process_posix.cpp)
	list(FILTER LIBPLATFORM_TIME_SOURCES EXCLUDE REGEX ".*win32.*")
endif()

# Setup source groups, mainly for Visual Studio
source_group("Header Files\\libplatform" FILES ${LIBPLATFORM_HEADERS})
source_group("Header Files\\libplatform\\io" FILES ${LIBPLATFORM_IO_HEADERS})
source_group("Header Files\\libplatform\\number" FILES ${LIBPLATFORM_NUMBER_HEADERS})
source_group("Header Files\\libplatform\\process" FILES ${LIBPLATFORM_PROCESS_HEADERS})
source_group("Header Files\\libplatform\\prog" FILES ${LIBPLATFORM_PROG_HEADERS})
source_group("Header Files\\libplatform\\sys" FILES ${LIBPLATFORM_SYS_HEADERS})
source_group("Header Files\\libplatform\\time" FILES ${LIBPLATFORM_TIME_HEADERS})
source_group("Header Files\\src" FILES ${SRC_HEADERS})
source_group("Header Files\\src\\bmff" FILES ${SRC_BMFF_HEADERS})
source_group("Header Files\\src\\itmf" FILES ${SRC_ITMF_HEADERS})
source_group("Header Files\\src\\qtff" FILES ${SRC_QTFF_HEADERS})
source_group("Source Files\\libplatform" FILES ${LIBPLATFORM_SOURCES})
source_group("Source Files\\libplatform\\io" FILES ${LIBPLATFORM_IO_SOURCES})
source_group("Source Files\\libplatform\\number" FILES ${LIBPLATFORM_NUMBER_SOURCES})
source_group("Source Files\\libplatform\\process" FILES ${LIBPLATFORM_PROCESS_SOURCES})
source_group("Source Files\\libplatform\\prog" FILES ${LIBPLATFORM_PROG_SOURCES})
source_group("Source Files\\libplatform\\sys" FILES ${LIBPLATFORM_SYS_SOURCES})
source_group("Source Files\\libplatform\\time" FILES ${LIBPLATFORM_TIME_SOURCES})
source_group("Source Files\\src" FILES ${SRC_SOURCES})
source_group("Source Files\\src\\bmff" FILES ${SRC_BMFF_SOURCES})
source_group("Source Files\\src\\itmf" FILES ${SRC_ITMF_SOURCES})
source_group("Source Files\\src\\qtff" FILES ${SRC_QTFF_SOURCES})

add_library(mp4v2 STATIC
	${INCLUDE_HEADERS}
	${LIBPLATFORM_HEADERS}
	${LIBPLATFORM_SOURCES}
	${LIBPLATFORM_IO_HEADERS}
	${LIBPLATFORM_IO_SOURCES}
	${LIBPLATFORM_NUMBER_HEADERS}
	${LIBPLATFORM_NUMBER_SOURCES}
	${LIBPLATFORM_PROCESS_HEADERS}
	${LIBPLATFORM_PROCESS_SOURCES}
	${LIBPLATFORM_PROG_HEADERS}
	${LIBPLATFORM_PROG_SOURCES}
	${LIBPLATFORM_SYS_HEADERS}
	${LIBPLATFORM_SYS_SOURCES}
	${LIBPLATFORM_TIME_HEADERS}
	${LIBPLATFORM_TIME_SOURCES}
	${SRC_HEADERS}
	${SRC_SOURCES}
	${SRC_BMFF_HEADERS}
	${SRC_BMFF_SOURCES}
	${SRC_ITMF_HEADERS}
	${SRC_ITMF_SOURCES}
	${SRC_QTFF_HEADERS}
	${SRC_QTFF_SOURCES})

# Add the preprocessor definitions
target_compile_definitions(mp4v2 PRIVATE
	UNICODE
	_UNICODE)

# Set up the proper include directories
target_include_directories(mp4v2 PRIVATE
	${QAAC_PATH}/mp4v2
	${QAAC_PATH}/mp4v2/include
	${VGM_BINARY_DIR}/mp4v2/include)

# Make sure that whatever compiler we use can handle these features
target_compile_features(mp4v2 PRIVATE
	cxx_long_long_type
	cxx_variadic_macros)

# Set up position-independent code
set_target_properties(mp4v2 PROPERTIES
	POSITION_INDEPENDENT_CODE TRUE)
