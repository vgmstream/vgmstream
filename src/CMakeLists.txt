file(GLOB BASE_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/base/*.h")
file(GLOB BASE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/base/*.c")
file(GLOB CODING_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/coding/*.h")
file(GLOB CODING_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/coding/*.c")
file(GLOB CLIBS_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/coding/libs/*.h")
file(GLOB CLIBS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/coding/libs/*.c")
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
source_group("Header Files\\base" FILES ${BASE_HEADERS})
source_group("Header Files\\coding" FILES ${CODING_HEADERS})
source_group("Header Files\\coding\\libs" FILES ${CLIBS_HEADERS})
source_group("Header Files\\layout" FILES ${LAYOUT_HEADERS})
source_group("Header Files\\meta" FILES ${META_HEADERS})
source_group("Header Files\\util" FILES ${UTIL_HEADERS})
source_group("Header Files\\ext" FILES ${EXT_HEADERS})

source_group("Source Files\\base" FILES ${BASE_SOURCES})
source_group("Source Files\\coding" FILES ${CODING_SOURCES})
source_group("Source Files\\coding\\libs" FILES ${CLIBS_SOURCES})
source_group("Source Files\\layout" FILES ${LAYOUT_SOURCES})
source_group("Source Files\\meta" FILES ${META_SOURCES})
source_group("Source Files\\util" FILES ${UTIL_SOURCES})

set(libvgmstream_sources
	${BASE_SOURCES}
	${CODING_SOURCES}
	${CLIBS_SOURCES}
	${LAYOUT_SOURCES}
	${META_SOURCES}
	${UTIL_SOURCES}
	${MAIN_SOURCES}
	${EXT_HEADERS}
)
set(libvgmstream_headers
	${BASE_HEADERS}
	${CODING_HEADERS}
	${CLIBS_HEADERS}
	${LAYOUT_HEADERS}
	${META_HEADERS}
	${UTIL_HEADERS}
	${MAIN_HEADERS}
)
# Set up the proper include directories
set(libvgmstream_includes
	${VGM_SOURCE_DIR}/ext_includes
	base
	coding
	layout
	meta
	util
)

# libvgmstream.a
add_library(libvgmstream STATIC ${libvgmstream_sources} ${libvgmstream_headers})
set_target_properties(libvgmstream PROPERTIES PREFIX "")

if(USE_FFMPEG AND FFMPEG_COMPILE)
	add_dependencies(libvgmstream avcodec)
endif()

setup_target(libvgmstream)

target_include_directories(libvgmstream PRIVATE ${libvgmstream_includes})

# libvgmstream.so
if(BUILD_SHARED_LIBS)
	include(GNUInstallDirs)

	add_library(libvgmstream_shared SHARED ${libvgmstream_sources} ${libvgmstream_headers})
	target_include_directories(libvgmstream_shared INTERFACE $<INSTALL_INTERFACE:include>)
	set_target_properties(libvgmstream_shared PROPERTIES
		PREFIX ""
		OUTPUT_NAME "libvgmstream"
		EXPORT_NAME "vgmstream"
		PUBLIC_HEADER "${libvgmstream_headers}"
	)
	
	if(USE_FFMPEG AND FFMPEG_COMPILE)
		add_dependencies(libvgmstream_shared avcodec)
	endif()
	
	setup_target(libvgmstream_shared TRUE)
	
	target_include_directories(libvgmstream_shared PRIVATE ${libvgmstream_includes})

	install(TARGETS libvgmstream_shared EXPORT vgmstream-targets PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/vgmstream)
	install(EXPORT vgmstream-targets FILE vgmstream-targets.cmake NAMESPACE vgmstream:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vgmstream)
	include(CMakePackageConfigHelpers)
	configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/vgmstream-config.cmake.in
		"${CMAKE_CURRENT_BINARY_DIR}/vgmstream-config.cmake"
		INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vgmstream
	)
	install(
		FILES "${CMAKE_CURRENT_BINARY_DIR}/vgmstream-config.cmake"
		DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vgmstream
	)
endif()
