# NOTE: Although this does include a section for handling MinGW, it might not be possible to build the foobar2000 component using MinGW using only CMake, as foobar2000's SDK only includes Visual Studio projects

set(RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/foo_input_vgmstream.rc)

# Setup source groups, mainly for Visual Studio
source_group("Resource Files" FILES ${RESOURCES})

add_library(foo_input_vgmstream SHARED
	foo_filetypes.h
	foo_prefs.h
	foo_vgmstream.h
	resource.h
	foo_prefs.cpp
	foo_streamfile.cpp
	foo_vgmstream.cpp
	${RESOURCES})

# Link to the vgmstream library and foobar2000's shared library
target_link_libraries(foo_input_vgmstream
	libvgmstream
	${FB2K_SDK_PATH}/foobar2000/shared/shared.lib)

setup_target(foo_input_vgmstream TRUE)

# Remove the prefix and set the suffix to .dll
set_target_properties(foo_input_vgmstream PROPERTIES
	PREFIX ""
	SUFFIX ".dll")

# Add the preprocessor definitions
target_compile_definitions(foo_input_vgmstream PRIVATE
	__STDC_CONSTANT_MACROS
	UNICODE
	_UNICODE)

# Make sure that the binary directory is included (for version.h), as well as foobar2000's include directories and the WTL include directory
target_include_directories(foo_input_vgmstream PRIVATE
	${VGM_BINARY_DIR}
	${FB2K_SDK_PATH}/foobar2000/SDK
	${FB2K_SDK_PATH}/foobar2000/ATLHelpers
	${FB2K_SDK_PATH}/foobar2000/shared
	${FB2K_SDK_PATH}/foobar2000
	${WTL_INCLUDE_PATH})

# Add dependencies to foobar2000's SDK
add_dependencies(foo_input_vgmstream
	fb2k_sdk
	fb2k_sdk_helpers
	fb2k_atl_helpers
	fb2k_component_client
	pfc)

# Make sure that whatever compiler we use can handle these features
target_compile_features(foo_input_vgmstream PRIVATE
	cxx_auto_type
	cxx_nullptr
	cxx_rvalue_references
	cxx_static_assert
	cxx_variadic_templates)

# Include the version string
if(MSVC)
	add_dependencies(foo_input_vgmstream version_h)
elseif(MINGW)
	if(VGMSTREAM_VERSION)
		target_compile_definitions(foo_input_vgmstream PRIVATE VGMSTREAM_VERSION="${VGMSTREAM_VERSION}")
	endif()

	# Also, on MinGW when using GCC, these flags need to be included to prevent requiring MinGW's runtime DLLs from being included, which does unfortunately increase the size of the DLL
	if(NOT CMAKE_CXX_COMPILER_ID MATCHES Clang)
		set_target_properties(foo_input_vgmstream PROPERTIES
			LINK_FLAGS "-static-libgcc -static-libstdc++")
	endif()
endif()

# Install the DLLs
install_dlls(${FB2K_COMPONENT_INSTALL_PREFIX}/foo_input_vgmstream)

# Install the component
install(TARGETS foo_input_vgmstream
	DESTINATION ${FB2K_COMPONENT_INSTALL_PREFIX}/foo_input_vgmstream)
