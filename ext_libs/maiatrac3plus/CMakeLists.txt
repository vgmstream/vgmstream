# vim:noexpandtab:
cmake_minimum_required(VERSION 2.8.8)
project(MaiAT3)

if(ANDROID)
	if(ARMEABI OR ARMEABI_V7A)
		set(ARM ON)
	endif()
endif()

if (IOS OR BLACKBERRY)
	set(ARM ON)
endif()

if (MAEMO)
	set(ARM ON)
	#Maemo's gcc-4.7.2 is strickt
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive")
endif()

if(LOONGSON)
	set(MIPS ON) # MIPS + x86 for now
endif()

if(ARM)
	set(USING_GLES2 ON)
else() # Assume x86
	set(X86 ON)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	set(MACOSX ON)
endif()

if(ANDROID OR BLACKBERRY OR IOS OR PANDORA OR MAEMO OR MACOSX)
	set(HEADLESS OFF)
elseif(NOT DEFINED HEADLESS)
	set(HEADLESS ON)
endif()

# User-editable options (go into CMakeCache.txt)
option(ARM "Set to ON if targeting an ARM processor" ${ARM})
option(MIPS "Set to ON if targeting a MIPS processor" ${MIPS})
option(X86 "Set to ON if targeting an X86 processor" ${X86})
option(ANDROID "Set to ON if targeting an Android device" ${ANDROID})
option(BLACKBERRY "Set to ON if targeting a Blackberry device" ${BLACKBERRY})
option(PANDORA "Set to ON if targeting an OpenPandora device" ${PANDORA})
option(MAEMO "Set to ON if targeting an Maemo (N900) device" ${MAEMO})
option(IOS "Set to ON if targeting an iOS device" ${IOS})

if(ANDROID)
	if(NOT ANDROID_ABI)
		message(FATAL_ERROR "CMAKE_TOOLCHAIN_FILE was not set!\n"
			"Delete the CMakeCache.txt file and CMakeFiles directory."
			"Rerun ${CMAKE_COMMAND} with \"-DCMAKE_TOOLCHAIN_FILE"
			"=${CMAKE_SOURCE_DIR}/android/android.toolchain.cmake\"")
	endif()
	set(CoreLibName ppsspp_jni)
	set(CoreLinkType SHARED)
elseif(IOS)
	if (NOT IOS_PLATFORM)
		message(FATAL_ERROR "CMAKE_TOOLCHAIN_FILE was not set!\n"
			"Delete the CMakeCache.txt file and CMakeFiles directory."
			"Rerun ${CMAKE_COMMAND} with \"-DCMAKE_TOOLCHAIN_FILE"
			"=${CMAKE_SOURCE_DIR}/ios/ios.toolchain.cmake\"")
	endif()
	set(CoreLibName Core)
	set(CoreLinkType STATIC)
else()
	set(CoreLibName Core)
	set(CoreLinkType STATIC)
endif()

include(FindThreads)
if(APPLE)
	find_library(COCOA_LIBRARY Cocoa)
endif()

# Needed for Globals.h
include_directories("${CMAKE_SOURCE_DIR}")

if(BLACKBERRY)
	add_definitions(-DBLACKBERRY)
	if(BLACKBERRY VERSION_GREATER 10)
		add_definitions(-DBLACKBERRY10)
	endif()
endif()

if(ANDROID)
	add_definitions(-DANDROID)
endif()

if(IOS)
	add_definitions(-DIOS)
endif()

if(MAEMO)
	add_definitions(-DMAEMO)
endif()

if(PANDORA)
	add_definitions(-DPANDORA)
endif()

if(ARM)
	add_definitions(-DARM)
endif()

if(MIPS)
	add_definitions(-DMIPS)
endif()

if(MACOSX)
	add_definitions(-DMACOSX)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -D_NDEBUG")

if(NOT MSVC)
	# Disable some warnings
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
	add_definitions(-Wno-multichar)
	add_definitions(-fno-strict-aliasing)
	if (CMAKE_C_COMPILER_ID STREQUAL "Intel")
		set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -parallel -fopenmp")
	else()
		add_definitions(-ffast-math)
	endif()
	if(NOT APPLE)
		if (NOT CMAKE_C_COMPILER_ID STREQUAL "Intel")
			add_definitions(-Wno-psabi)
		endif()
		add_definitions(-D_XOPEN_SOURCE=600 -D_XOPEN_SOURCE_EXTENDED -D__BSD_VISIBLE=1)
		add_definitions(-D_LARGEFILE64_SOURCE=1 -D_FILE_OFFSET_BITS=64)
	endif()
	if(IOS)
		add_definitions(-DGL_ETC1_RGB8_OES=0)
	endif()

	if(BLACKBERRY)
		add_definitions(-D_QNX_SOURCE=1 -D_C99=1 -O3 -mfpu=neon -mtune=cortex-a9)
	endif()

	if(X86 AND NOT MIPS)
		# enable sse2 code generation
		add_definitions(-msse2)
		if(EMSCRIPTEN)
			add_definitions(-msimd128)
		endif()
	endif()

	if(IOS)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
	elseif(APPLE)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -U__STRICT_ANSI__")
		# Karen/angelXwind: --macosx-version-min=10.7 is needed in order to produce binaries that OS X 10.7 Lion can execute. However, it seems that PPSSPP won't support 10.6 or lower without getting rid of -stdlib=libc++ ...which probably won't end well. So I guess PPSSPP will strictly be a 10.7+ app.
		# vit9696: OSX 10.6 builds are possible: http://forums.ppsspp.org/showthread.php?tid=1826&pid=18875#pid18875
		set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")
	endif()
	if(CMAKE_COMPILER_IS_GNUCXX AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.7.0)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
	endif()
else()
	# Disable warnings about MS-specific _s variants of libc functions
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

if(ANDROID)
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/android/libs/${ANDROID_ABI}")
endif()

# This sets up the MSVC project dirs according to the physical project dirs
macro(setup_target_project TargetName ProjectDir)
	get_property(TargetSources TARGET "${TargetName}" PROPERTY SOURCES)
	foreach(Source ${TargetSources})
		# Figure out the file's path relative to the ProjectDir
		# NOTE: &#$@ double-quoted regexps
		string(REGEX REPLACE "${ProjectDir}" "" RelativePath "${Source}")
		string(REGEX REPLACE "[\\\\/][^\\\\/]*$" "" RelativePath "${RelativePath}")
		string(REGEX REPLACE "^[\\\\/]" "" RelativePath "${RelativePath}")
		string(REGEX REPLACE "/" "\\\\\\\\" RelativePath "${RelativePath}")
		# put the source file in a source_group equivalent to the relative path
		source_group("${RelativePath}" FILES ${Source})
	endforeach()
endmacro()

include_directories(include)
include_directories(include/SUB)

add_library(base STATIC
	src/base/Heap_Alloc0.cc
	src/base/MaiBitReader.cc
	src/base/MaiBufIO.cc
	src/base/MaiCriticalSection.cc
	src/base/MaiFile.cc
	src/base/MaiQueue0.cc
	src/base/MaiString.cc
	src/base/MaiThread.cc
	src/base/Mai_Sleep.cc
	src/base/Mai_Mem.cc
	src/base/Unicode.cc)

add_library(at3plusdecoder SHARED
	maiatrac3plus.cpp
	src/MaiAT3PlusCoreDecoder.cpp
	src/MaiAT3PlusCoreDecoder_DecFunc.cpp
	src/MaiAT3PlusCoreDecoder_StaticData.cpp
	src/MaiAT3PlusCoreDecoder_SubFunc.cpp
	src/MaiAT3PlusFrameDecoder.cpp)

target_link_libraries(at3plusdecoder base)
