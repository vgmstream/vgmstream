#ifndef _WINDOWS_UTILS_H_
#define _WINDOWS_UTILS_H_
#if defined(VGM_STDIO_UNICODE) && defined(WIN32)

#include <stdbool.h>
#include <wchar.h>

#if 0
// printf that handles utf-8 strings
void printf_win(const char* fmt, ...);
void fprintf_win(FILE* stream, const char* fmt, ...);
#endif


// fopen that handles utf-8 strings
FILE* fopen_win(const char* path, const char* mode);

// windows entry point
int wmain(int argc, wchar_t** argv);

// helper for wmain that converts wchar_t** to char** and calls main
int wmain_process(int argc, wchar_t** argv);


#endif
#endif
