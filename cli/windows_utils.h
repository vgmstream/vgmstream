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

// Allows UTF-8 stdout output by programatically setting 'chcp 65001'
// win terminal needs a font that supports UTF-8; output redir (vgmstream-cli ... > blah.txt) works fine.
// Returns current codepage to be restored later.
uint32_t windows_setup_codepage_utf8();

// Restores original codepage (just in case).
void windows_restore_codepage(uint32_t codepage);


// convert strings using char block, until done or not enough block
bool windows_wargs_to_args_stack(int argc, wchar_t** argv_w, char** argv, int argc_max, char* block, int block_size);
bool windows_wargs_to_args_alloc(int argc, wchar_t** argv_w, char*** p_argv, char** p_block);

// fopen that handles utf-8 strings
FILE* fopen_win(const char* path, const char* mode);

// windows entry point
int wmain(int argc, wchar_t** argv);

#endif
#endif
