# CLI

add_executable(vgmstream_cli
	vgmstream_cli.c)

# Link to the vgmstream library
target_link_libraries(vgmstream_cli libvgmstream)

setup_target(vgmstream_cli TRUE)

if(WIN32)
	# Add the preprocessor definitions
	target_compile_definitions(vgmstream_cli PRIVATE _CONSOLE)

	# Link to the getopt library
	target_link_libraries(vgmstream_cli getopt)

	# Make sure that the binary directory is included (for version.h), as well as the getopt library include directory
	target_include_directories(vgmstream_cli PRIVATE
		${CMAKE_BINARY_DIR}
		${CMAKE_SOURCE_DIR}/ext_libs/Getopt)

	# Include the version string
	if(MSVC)
		add_dependencies(vgmstream_cli version_h)
	elseif(MINGW)
		if(VGMSTREAM_VERSION)
			target_compile_definitions(vgmstream_cli PRIVATE VERSION="${VGMSTREAM_VERSION}")
		endif()

		# Also, on MinGW when using GCC, these flags need to be included to prevent requiring MinGW's runtime DLLs from being included, which does unfortunately increase the size of the EXE
		if(NOT CMAKE_CXX_COMPILER_ID MATCHES Clang)
			set_target_properties(vgmstream_cli PROPERTIES
				LINK_FLAGS "-static-libgcc -static-libstdc++")
		endif()
	endif()

	# Install the DLLs
	install_dlls(${CMAKE_INSTALL_PREFIX}/bin)
elseif(VGMSTREAM_VERSION)
	# Include the version string
	target_compile_definitions(vgmstream_cli PRIVATE VERSION="${VGMSTREAM_VERSION}")
endif()

# Install the CLI program
install(TARGETS vgmstream_cli
	RUNTIME DESTINATION bin)

# TODO: Make it so vgmstream123 can build with Windows (this probably needs a libao.dll included with vgmstream, though)

if(NOT WIN32)
	# vgmstream123

	add_executable(vgmstream123
		vgmstream123.c)

	# Link to the vgmstream library as well as libao
	target_link_libraries(vgmstream123
		libvgmstream
		${AO_LIBRARY})

	setup_target(vgmstream123 TRUE)

	# Add AO include directory
	target_include_directories(vgmstream123 PRIVATE
		${AO_INCLUDE_DIR})

	# Include the version string
	target_compile_definitions(vgmstream123 PRIVATE VERSION="${VGMSTREAM_VERSION}")

	# Install vgmstream123
	install(TARGETS vgmstream123
		RUNTIME DESTINATION bin)
endif()
