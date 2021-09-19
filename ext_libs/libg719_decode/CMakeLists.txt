cmake_minimum_required(VERSION 3.5)
project(libg719_decode)

if(CMAKE_CXX_COMPILER_ID MATCHES Clang OR CMAKE_CXX_COMPILER_ID MATCHES GNU)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-comment -Wno-missing-braces")
endif()

file(GLOB_RECURSE SOURCES "reference_code/*.c")

add_library(g719_decode STATIC g719.c ${SOURCES})

target_include_directories(g719_decode PRIVATE reference_code/include)

set_property(TARGET g719_decode PROPERTY POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(g719_decode PRIVATE VAR_ARRAYS)
