# CLI

add_executable(vgmstream_cli
	vgmstream_cli.c)

if(WIN32)
	set_target_properties(vgmstream_cli PROPERTIES
		PREFIX ""
		OUTPUT_NAME "test")
else()
	set_target_properties(vgmstream_cli PROPERTIES
		PREFIX ""
		OUTPUT_NAME "vgmstream-cli")
endif()

# Link to the vgmstream library
target_link_libraries(vgmstream_cli libvgmstream)

# Link to Jansson, if we have it
if (USE_JANSSON)
    target_compile_definitions(vgmstream_cli PRIVATE HAVE_JSON)
	target_include_directories(vgmstream_cli PRIVATE ${JANSSON_INCLUDE_DIRS})
	target_link_libraries(vgmstream_cli jansson)
endif()

setup_target(vgmstream_cli TRUE)

if(WIN32)
	# Add the preprocessor definitions
	target_compile_definitions(vgmstream_cli PRIVATE _CONSOLE)

	# Link to the getopt library
	target_link_libraries(vgmstream_cli getopt)

	# Make sure that the binary directory is included (for version.h), as well as the getopt library include directory
	target_include_directories(vgmstream_cli PRIVATE
		${VGM_BINARY_DIR}
		${VGM_SOURCE_DIR}/ext_libs/Getopt)

	# Include the version string
	if(MSVC)
		add_dependencies(vgmstream_cli version_h)
	elseif(MINGW)
		if(VGMSTREAM_VERSION)
			target_compile_definitions(vgmstream_cli PRIVATE VGMSTREAM_VERSION="${VGMSTREAM_VERSION}")
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
	target_compile_definitions(vgmstream_cli PRIVATE VGMSTREAM_VERSION="${VGMSTREAM_VERSION}")
endif()



# Install the CLI program
install(TARGETS vgmstream_cli
	RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

# TODO: Make it so vgmstream123 can build with Windows (this probably needs a libao.dll included with vgmstream, though)

if(NOT WIN32 AND BUILD_V123)
	# vgmstream123

	add_executable(vgmstream123
		vgmstream123.c)

	# Link to the vgmstream library as well as libao
	target_link_libraries(vgmstream123
		libvgmstream
		ao pthread dl)

	setup_target(vgmstream123 TRUE)

	# Add AO include directory
	target_include_directories(vgmstream123 PRIVATE
		${LIBAO_INCLUDE})

	# Include the version string
	target_compile_definitions(vgmstream123 PRIVATE VGMSTREAM_VERSION="${VGMSTREAM_VERSION}")

	

	# Install vgmstream123
	install(TARGETS vgmstream123
		RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
endif()
