// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the MAIATRAC3PLUS_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// MAIATRAC3PLUS_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.

#ifdef _WIN32
#ifdef MAIATRAC3PLUS_EXPORTS
#define MAIATRAC3PLUS_API __declspec(dllexport)
#else
#define MAIATRAC3PLUS_API __declspec(dllimport)
#endif
#else
#define MAIATRAC3PLUS_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

MAIATRAC3PLUS_API void* Atrac3plusDecoder_openContext();
MAIATRAC3PLUS_API int Atrac3plusDecoder_closeContext(void* context);
MAIATRAC3PLUS_API int Atrac3plusDecoder_decodeFrame(void* context, void* inbuf, int inbytes, int* channels, void** outbuf);

#ifdef __cplusplus
}
#endif
