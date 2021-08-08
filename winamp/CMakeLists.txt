file(GLOB WINAMP_SDK_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/sdk/*.h")
set(RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/resource.rc)
file(GLOB WINAMP_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
file(GLOB WINAMP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

# Setup source groups, mainly for Visual Studio
source_group("Header Files\\sdk" FILES ${WINAMP_SDK_HEADERS})
source_group("Resource Files" FILES ${RESOURCES})

add_library(in_vgmstream SHARED
	${WINAMP_SDK_HEADERS}
    ${WINAMP_HEADERS}
    ${WINAMP_SOURCES}
	${RESOURCES})

# Link to the vgmstream library
target_link_libraries(in_vgmstream libvgmstream)

setup_target(in_vgmstream TRUE)

# Remove the prefix and set the suffix to .dll
set_target_properties(in_vgmstream PROPERTIES
	PREFIX ""
	SUFFIX ".dll")

# Add the preprocessor definitions
target_compile_definitions(in_vgmstream PRIVATE
	VGM_WINAMP_UNICODE
	__STDC_CONSTANT_MACROS
	UNICODE
	_UNICODE)

# Make sure that the binary directory is included (for version.h)
target_include_directories(in_vgmstream PRIVATE ${VGM_BINARY_DIR})

# Include the version string
if(MSVC)
	add_dependencies(in_vgmstream version_h)
elseif(MINGW)
	if(VGMSTREAM_VERSION)
		target_compile_definitions(in_vgmstream PRIVATE VGMSTREAM_VERSION="${VGMSTREAM_VERSION}")
	endif()

	# Also, on MinGW when using GCC, these flags need to be included to prevent requiring MinGW's runtime DLLs from being included, which does unfortunately increase the size of the DLL
	if(NOT CMAKE_CXX_COMPILER_ID MATCHES Clang)
		set_target_properties(in_vgmstream PROPERTIES
			LINK_FLAGS "-static-libgcc -static-libstdc++")
	endif()
endif()

# Install the DLLs
install_dlls(${WINAMP_INSTALL_PREFIX})

# Install the plugin
install(TARGETS in_vgmstream
	DESTINATION ${WINAMP_INSTALL_PREFIX}/Plugins)
