# ALthough fdk-aac does have a Visual Studio project for Windows, this will also cover other compilers and platforms

file(GLOB LIBAACDEC_HEADERS "${FDK_AAC_PATH}/libAACdec/include/*.h" "${FDK_AAC_PATH}/libAACdec/src/*.h")
file(GLOB LIBAACDEC_SOURCES "${FDK_AAC_PATH}/libAACdec/src/*.cpp")
file(GLOB LIBAACENC_HEADERS "${FDK_AAC_PATH}/libAACenc/include/*.h" "${FDK_AAC_PATH}/libAACenc/src/*.h")
file(GLOB LIBAACENC_SOURCES "${FDK_AAC_PATH}/libAACenc/src/*.cpp")
file(GLOB LIBFDK_HEADERS "${FDK_AAC_PATH}/libFDK/include/*.h" "${FDK_AAC_PATH}/libFDK/include/x86/*h")
file(GLOB LIBFDK_SOURCES "${FDK_AAC_PATH}/libFDK/src/*.cpp")
file(GLOB LIBMPEGTPDEC_HEADERS "${FDK_AAC_PATH}/libMpegTPDec/include/*.h" "${FDK_AAC_PATH}/libMpegTPDec/src/*.h")
file(GLOB LIBMPEGTPDEC_SOURCES "${FDK_AAC_PATH}/libMpegTPDec/src/*.cpp")
file(GLOB LIBMPEGTPENC_HEADERS "${FDK_AAC_PATH}/libMpegTPEnc/include/*.h" "${FDK_AAC_PATH}/libMpegTPEnc/src/*.h")
file(GLOB LIBMPEGTPENC_SOURCES "${FDK_AAC_PATH}/libMpegTPEnc/src/*.cpp")
file(GLOB LIBPCMUTILS_HEADERS "${FDK_AAC_PATH}/libPCMutils/include/*.h")
file(GLOB LIBPCMUTILS_SOURCES "${FDK_AAC_PATH}/libPCMutils/src/*.cpp")
file(GLOB LIBSBRDEC_HEADERS "${FDK_AAC_PATH}/libSBRdec/include/*.h" "${FDK_AAC_PATH}/libSBRdec/src/*.h")
file(GLOB LIBSBRDEC_SOURCES "${FDK_AAC_PATH}/libSBRdec/src/*.cpp")
file(GLOB LIBSBRENC_HEADERS "${FDK_AAC_PATH}/libSBRenc/include/*.h" "${FDK_AAC_PATH}/libSBRenc/src/*.h")
file(GLOB LIBSBRENC_SOURCES "${FDK_AAC_PATH}/libSBRenc/src/*.cpp")
file(GLOB LIBSYS_HEADERS "${FDK_AAC_PATH}/libSYS/include/*.h")
file(GLOB LIBSYS_SOURCES "${FDK_AAC_PATH}/libSYS/src/*.cpp")
if(NOT WIN32)
	file(GLOB LIBSYS_LINUX_SOURCES "${FDK_AAC_PATH}/libSYS/linux/*.cpp")
endif()

# Setup source groups, mainly for Visual Studio
source_group("Header Files\\libAACdec" FILES ${LIBAACDEC_HEADERS})
source_group("Header Files\\libAACenc" FILES ${LIBAACENC_HEADERS})
source_group("Header Files\\libFDK" FILES ${LIBFDK_HEADERS})
source_group("Header Files\\libMpegTPDec" FILES ${LIBMPEGTPDEC_HEADERS})
source_group("Header Files\\libMpegTPEnc" FILES ${LIBMPEGTPENC_HEADERS})
source_group("Header Files\\libPCMutils" FILES ${LIBPCMUTILS_HEADERS})
source_group("Header Files\\libSBRdec" FILES ${LIBSBRDEC_HEADERS})
source_group("Header Files\\libSBRenc" FILES ${LIBSBRENC_HEADERS})
source_group("Header Files\\libSYS" FILES ${LIBSYS_HEADERS})
source_group("Source Files\\libAACdec" FILES ${LIBAACDEC_SOURCES})
source_group("Source Files\\libAACenc" FILES ${LIBAACENC_SOURCES})
source_group("Source Files\\libFDK" FILES ${LIBFDK_SOURCES})
source_group("Source Files\\libMpegTPDec" FILES ${LIBMPEGTPDEC_SOURCES})
source_group("Source Files\\libMpegTPEnc" FILES ${LIBMPEGTPENC_SOURCES})
source_group("Source Files\\libPCMutils" FILES ${LIBPCMUTILS_SOURCES})
source_group("Source Files\\libSBRdec" FILES ${LIBSBRDEC_SOURCES})
source_group("Source Files\\libSBRenc" FILES ${LIBSBRENC_SOURCES})
source_group("Source Files\\libSYS" FILES ${LIBSYS_SOURCES} ${LIBSYS_LINUX_SOURCES})

add_library(fdk-aac STATIC
	${LIBAACDEC_HEADERS}
	${LIBAACDEC_SOURCES}
	${LIBAACENC_HEADERS}
	${LIBAACENC_SOURCES}
	${LIBFDK_HEADERS}
	${LIBFDK_SOURCES}
	${LIBMPEGTPDEC_HEADERS}
	${LIBMPEGTPDEC_SOURCES}
	${LIBMPEGTPENC_HEADERS}
	${LIBMPEGTPENC_SOURCES}
	${LIBPCMUTILS_HEADERS}
	${LIBPCMUTILS_SOURCES}
	${LIBSBRDEC_HEADERS}
	${LIBSBRDEC_SOURCES}
	${LIBSBRENC_HEADERS}
	${LIBSBRENC_SOURCES}
	${LIBSYS_HEADERS}
	${LIBSYS_SOURCES}
	${LIBSYS_LINUX_SOURCES})

# Set up the proper include directories
target_include_directories(fdk-aac PRIVATE
	${FDK_AAC_PATH}/libAACdec/include
	${FDK_AAC_PATH}/libAACenc/include
	${FDK_AAC_PATH}/libSBRdec/include
	${FDK_AAC_PATH}/libSBRenc/include
	${FDK_AAC_PATH}/libMpegTPDec/include
	${FDK_AAC_PATH}/libMpegTPEnc/include
	${FDK_AAC_PATH}/libSYS/include
	${FDK_AAC_PATH}/libFDK/include
	${FDK_AAC_PATH}/libPCMutils/include)

# Make sure that whatever compiler we use can handle these features
target_compile_features(fdk-aac PRIVATE cxx_long_long_type)

# Set up position-independent code
set_target_properties(fdk-aac PROPERTIES
	POSITION_INDEPENDENT_CODE TRUE)
