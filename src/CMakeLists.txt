file(GLOB CODING_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/coding/*.h")
file(GLOB CODING_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/coding/*.c")
file(GLOB LAYOUT_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/layout/*.h")
file(GLOB LAYOUT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/layout/*.c")
file(GLOB META_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/meta/*.h")
file(GLOB META_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/meta/*.c")
file(GLOB UTIL_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/util/*.h")
file(GLOB UTIL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/util/*.c")
set(EXT_HEADERS
	${VGM_SOURCE_DIR}/ext_includes/pstdint.h)
file(GLOB MAIN_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
file(GLOB MAIN_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

# Setup source groups, mainly for Visual Studio
source_group("Header Files\\coding" FILES ${CODING_HEADERS})
source_group("Header Files\\layout" FILES ${LAYOUT_HEADERS})
source_group("Header Files\\meta" FILES ${META_HEADERS})
source_group("Header Files\\util" FILES ${UTIL_HEADERS})
source_group("Header Files\\ext" FILES ${EXT_HEADERS})
source_group("Source Files\\coding" FILES ${CODING_SOURCES})
source_group("Source Files\\layout" FILES ${LAYOUT_SOURCES})
source_group("Source Files\\meta" FILES ${META_SOURCES})
source_group("Source Files\\util" FILES ${UTIL_SOURCES})

set(libvgmstream_sources
	${CODING_HEADERS}
	${CODING_SOURCES}
	${LAYOUT_HEADERS}
	${LAYOUT_SOURCES}
	${META_HEADERS}
	${META_SOURCES}
	${UTIL_HEADERS}
	${UTIL_SOURCES}
	${EXT_HEADERS}
	${MAIN_HEADERS}
	${MAIN_SOURCES}
)
# Set up the proper include directories
set(libvgmstream_includes
	${VGM_SOURCE_DIR}/ext_includes
	coding
	layout
	meta
	util
)

# libvgmstream.a
add_library(libvgmstream STATIC ${libvgmstream_sources})
set_target_properties(libvgmstream PROPERTIES PREFIX "")

if(USE_FFMPEG AND FFMPEG_COMPILE)
	add_dependencies(libvgmstream avcodec)
endif()

setup_target(libvgmstream)

target_include_directories(libvgmstream PRIVATE ${libvgmstream_includes})

# libvgmstream.so
if(NOT EMSCRIPTEN)
	add_library(libvgmstream_shared SHARED EXCLUDE_FROM_ALL ${libvgmstream_sources})
	set_target_properties(libvgmstream_shared PROPERTIES
		PREFIX ""
		OUTPUT_NAME "libvgmstream"
	)
	
	if(USE_FFMPEG AND FFMPEG_COMPILE)
		add_dependencies(libvgmstream_shared avcodec)
	endif()
	
	setup_target(libvgmstream_shared TRUE)
	
	target_include_directories(libvgmstream_shared PRIVATE ${libvgmstream_includes})
	
	if(CMAKE_CXX_COMPILER_ID MATCHES Clang OR CMAKE_CXX_COMPILER_ID MATCHES GNU)
		set_target_properties(libvgmstream_shared PROPERTIES LINK_FLAGS_RELEASE -s)
	endif()
endif()
