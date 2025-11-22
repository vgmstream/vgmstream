#if defined(VGM_STDIO_UNICODE) && defined(WIN32)
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include "windows_utils.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define WIN_PATH_LIMIT 4096

// WideCharToMultiByte:
//  CodePage: CP_UTF8
//  dwFlags: config
//  lpWideCharStr: input wchar_t string
//  cchWideChar: size of input, or -1 for null-terminated
//  lpMultiByteStr: output string
//  cbMultiByte: output size (0 to check for needed string)
//  lpDefaultChar: char to use for non-representable chars (NULL for default)
//  lpUsedDefaultChar: flag to check if default char was used
// returns number of written chars, or 0 on error (not enough buffer, etc)

static int char_to_wchar(const char* str, wchar_t* wstr, int wstr_len) {
    return MultiByteToWideChar(CP_UTF8, 0, str, -1, wstr, wstr_len);
}

// MultiByteToWideChar:
//  CodePage: CP_UTF8
//  dwFlags: config
//  lpMultiByteStr: input wchar_t string
//  cbMultiByte: size of input, or -1 for null-terminated
//  lpWideCharStr: output string
//  cchWideChar: output size (0 to check for needed string)
// returns number of written chars, or 0 on error (not enough buffer, etc)

static int wchar_to_char(const wchar_t* wstr, char* str, int str_len) {
    return WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str, str_len, NULL, NULL);
}


#if 0
// code below was left for testing purposes

#define WIN_LINE 2024 //big-ish for describe

/* Windows can fwprintf to console but only if mode is set to:
 *    _setmode(_fileno(stdout), _O_U16TEXT);
 * That breaks fprintf, but we can use Windows' WriteConsoleW to bypass it.
 * However that also doesn't work it stdout is redirected to a file.
 * So it needs to detect console and use WriteFile (seems the same as printf).
 * 
 * Alternatively just set console's codepage to UTF-8 + printf, seems to work well enough.
 */

static HANDLE get_windows_std_handle(FILE* stream) {
    if (stream == stdout) {
        static HANDLE hSdtout = NULL;
        if (!hSdtout)
            hSdtout = GetStdHandle(STD_OUTPUT_HANDLE);
        return hSdtout;
    }
    else if (stream == stderr) {
        static HANDLE hErrout = NULL;
        if (!hErrout)
            hErrout = GetStdHandle(STD_ERROR_HANDLE);
        return hErrout;
    }

    return NULL;
}

bool is_windows_console(HANDLE h) {
    if (!h)
        return false;

    DWORD type = GetFileType(h);
    if (type != FILE_TYPE_CHAR)
        return false;

    // TO-DO: mode flags?
    DWORD mode = 0;
    return GetConsoleMode(h, &mode);
}

// WriteConsoleW:
//   hConsoleOutput: 
//   lpBuffer: input wchar_t string
//   nNumberOfCharsToWrite: input len
//   lpNumberOfCharsWritten: output chars written (optional)
//   lpReserved: always NULL
// returns 0 on error

static bool fwprint_win(FILE* stream, const wchar_t* wtext, int wtext_len) {

    HANDLE hOut = get_windows_std_handle(stream);
    if (hOut) {
        if (wtext_len == 0)
            wtext_len = wcslen(wtext);
        int ret = WriteConsoleW(hOut, wtext, wtext_len, NULL, NULL);
        printf("wrote to console %i\n", ret);

        if (ret <= 0) return false;
    }
    else {
        printf("no handle\n");
        fwprintf(stream, L"%s", wtext);
    }

    return true;
}

static bool fprint_line(FILE* stream, const char* text, int text_len) {
    HANDLE hOut = get_windows_std_handle(stream);
    if (is_windows_console(hOut)) {
        wchar_t wtext[WIN_LINE];
        int wtext_len = char_to_wchar(text, wtext, sizeof(wtext));
        if (wtext_len <= 0)
            return false;

        //if (wtext_len == 0)
        //    wtext_len = wcslen(wtext);
        int ret = WriteConsoleW(hOut, wtext, wtext_len, NULL, NULL);
        if (ret <= 0) return false;
    }
    else {
        //equivalent: fprintf(stream, "%s", text); 
        // also works for console but needs SetConsoleOutputCP(CP_UTF8)
        int ret = WriteFile(hOut, text, text_len, NULL, NULL);
        if (ret <= 0) return false;
    }

    return true;
}

static void fprintf_default(FILE* stream, const char* fmt, va_list args) {
    vfprintf(stream, fmt, args);
}

static void fprintf_args(FILE* stream, const char* fmt, va_list args) {
    char text[WIN_LINE];

    int done = vsnprintf(text, sizeof(text), fmt, args);
    if (done < 0 || done > sizeof(text)) {
        fprintf_default(stream, fmt, args);
        return;
    }

    bool ok = fprint_line(stream, text, done);
    if (!ok) {
        fprintf_default(stream, fmt, args);
    }
}

void printf_win(const char* fmt, ...) {
    va_list args;

    va_start(args, fmt);
    fprintf_args(stdout, fmt, args);
    va_end(args);
}

void fprintf_win(FILE* stream, const char* fmt, ...) {
    va_list args;

    va_start(args, fmt);
    fprintf_args(stream, fmt, args);
    va_end(args);
}
#endif

// handles stdout output by programatically setting 'chcp 65001'
// win terminal needs a font that supports utf-8; output redir (vgmstream... > blah.txt) works fine.
uint32_t windows_setup_codepage_utf8() {
    uint32_t prev_codepage = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);
    return prev_codepage;
}

// Restores original codepage since just in case.
void windows_restore_codepage(uint32_t codepage) {
    SetConsoleOutputCP(codepage);
}

bool windows_wargs_to_args_stack(int argc, wchar_t** argv_w, char** argv, int argc_max, char* block, int block_size) {
    if (argc > argc_max)
        return false;

    for (int i = 0; i < argc; i++) {
        int done = wchar_to_char(argv_w[i], block, block_size);
        if (done <= 0)
            return false;

        argv[i] = block;
        block += done;
        block_size -= done;
    }

    return true;
}

bool windows_wargs_to_args_alloc(int argc, wchar_t** argv_w, char*** p_argv, char** p_block) {
    int block_alloc_size = 0x2000;
    int block_pos = 0;
    int i;

    char** argv_alloc = NULL;
    char* block_alloc = NULL;

    argv_alloc = malloc(argc * sizeof(char*));
    if (!argv_alloc) goto fail;

    block_alloc = malloc(block_alloc_size);
    if (!block_alloc) goto fail;

    i = 0;
    while (i < argc) {
        char* block = block_alloc + block_pos;
        int block_size = block_alloc_size - block_pos;

        int done = wchar_to_char(argv_w[i], block, block_size);
        if (done <= 0) {
            DWORD err = GetLastError();
            if (err != ERROR_INSUFFICIENT_BUFFER)
                goto fail;

            if (block_alloc_size > 0x100000) //arbitrary max
                goto fail;

            // reserve more memory and retry (uncommon but needed if many files are passed)
            {
                int new_size = block_alloc_size * 2;
                char* new_block = realloc(block_alloc, new_size);
                if (!new_block) goto fail;

                block_alloc = new_block;
                block_alloc_size = new_size;
            }
        }
        else {
            argv_alloc[i] = block;
            block_pos += done;
            i++;
        }
    }

    *p_argv = argv_alloc;
    *p_block = block_alloc;
    return true;

fail:
    free(argv_alloc);
    free(block_alloc);
    return false;
}


static bool is_ascii(const char* str) {
    while (str[0] != 0x00) {
        uint8_t elem = (uint8_t)str[0];
        if (elem >= 0x80)
            return false;
        str++;
    }
    return true;
}

FILE* fopen_win(const char* path, const char* mode) {
    // Micro-optimization since converting small-ish ascii strings is the common case.
    // Possibly irrelevant since MultiByteToWideChar would do a full scan, but also extra steps.
    if (is_ascii(path)) {
        return fopen(path, mode);
    }

    int done;

    wchar_t wpath[WIN_PATH_LIMIT];
    done = char_to_wchar(path, wpath, sizeof(wpath));
    if (done <= 0) return NULL;

    wchar_t wmode[3+1];
    done = char_to_wchar(mode, wmode, sizeof(wmode));
    if (done <= 0) return NULL;

    return _wfopen(wpath, wmode);
}

#endif
