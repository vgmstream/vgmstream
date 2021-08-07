add_library(audacious_vgmstream SHARED
	plugin.h
	vfs.h
	plugin.cc
	vfs.cc)

# Link to the vgmstream library and audacious library
target_link_libraries(audacious_vgmstream
	libvgmstream
	${AUDACIOUS_LINK_LIBRARIES}
	${GTK_LINK_LIBRARIES})

setup_target(audacious_vgmstream TRUE)

# Remove the prefix and set output name
set_target_properties(audacious_vgmstream PROPERTIES
	PREFIX ""
	OUTPUT_NAME "vgmstream")

macro(extract_defines CFLAGS CFLAGS_COPY)
	set(${CFLAGS_COPY} ${${CFLAGS}})
	if(${CFLAGS_COPY})
		list(FILTER ${CFLAGS_COPY} INCLUDE REGEX "^-D.*")
	endif()
endmacro()

macro(extract_non_defines CFLAGS CFLAGS_COPY)
	set(${CFLAGS_COPY} ${${CFLAGS}})
	if(${CFLAGS_COPY})
		list(FILTER ${CFLAGS_COPY} EXCLUDE REGEX "^-D.*")
	endif()
endmacro()

extract_defines(AUDACIOUS_CFLAGS_OTHER AUDACIOUS_DEFINES)
extract_defines(GTK_CFLAGS_OTHER GTK_DEFINES)
extract_non_defines(AUDACIOUS_CFLAGS_OTHER AUDACIOUS_CFLAGS_OTHER)
extract_non_defines(GTK_CFLAGS_OTHER GTK_CFLAGS_OTHER)

# Include compile definitions for Audacious and GTK, as well as the version string
target_compile_definitions(audacious_vgmstream PRIVATE
	${AUDACIOUS_DEFINES}
	${GTK_DEFINES}
	VGMSTREAM_VERSION="${VGMSTREAM_VERSION}")

# Include compile flags for Audacious and GTK
set_target_properties(audacious_vgmstream PROPERTIES
	COMPILE_FLAGS "${AUDACIOUS_CFLAGS_OTHER} ${GTK_CFLAGS_OTHER}")

# Make sure that the binary directory is included (for version.h) as well as the Audacious and GTK include directories
target_include_directories(audacious_vgmstream PRIVATE
	${VGM_BINARY_DIR}
	${AUDACIOUS_INCLUDE_DIRS}
	${GTK_INCLUDE_DIRS})

# Make sure that whatever compiler we use can handle these features
target_compile_features(audacious_vgmstream PRIVATE
	cxx_auto_type
	cxx_constexpr
	cxx_decltype
	cxx_defaulted_move_initializers
	cxx_deleted_functions
	cxx_nullptr
	cxx_rvalue_references
	cxx_static_assert
	cxx_strong_enums
	cxx_variadic_macros
	cxx_variadic_templates)

# Install the plugin
install(TARGETS audacious_vgmstream
	DESTINATION ${AUDACIOUS_PLUGIN_DIR}/Input)
