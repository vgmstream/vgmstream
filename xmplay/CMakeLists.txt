add_library(xmp-vgmstream SHARED
	xmpfunc.h
	xmpin.h
	xmp_vgmstream.c
	xmpin.def)

# Link to the vgmstream library
target_link_libraries(xmp-vgmstream libvgmstream)

setup_target(xmp-vgmstream TRUE)

# Remove the prefix and set the suffix to .dll
set_target_properties(xmp-vgmstream PROPERTIES
	PREFIX ""
	SUFFIX ".dll")

# Make sure that the binary directory is included (for version.h)
target_include_directories(xmp-vgmstream PRIVATE ${VGM_BINARY_DIR})

# Include the version string
if(MSVC)
	add_dependencies(xmp-vgmstream version_h)
elseif(MINGW)
	if(VGMSTREAM_VERSION)
		target_compile_definitions(xmp-vgmstream PRIVATE VGMSTREAM_VERSION="${VGMSTREAM_VERSION}")
	endif()

	# Also, on MinGW when using GCC, these flags need to be included to prevent requiring MinGW's runtime DLLs from being included, which does unfortunately increase the size of the DLL
	if(NOT CMAKE_CXX_COMPILER_ID MATCHES Clang)
		set_target_properties(xmp-vgmstream PROPERTIES
			LINK_FLAGS "-static-libgcc -static-libstdc++")
	endif()
endif()

# Install the DLLs
install_dlls(${XMPLAY_INSTALL_PREFIX})

# Install the plugin
install(TARGETS xmp-vgmstream
	DESTINATION ${XMPLAY_INSTALL_PREFIX})
