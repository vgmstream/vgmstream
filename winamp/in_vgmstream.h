#ifndef _IN_VGMSTREAM_
#define _IN_VGMSTREAM_


/* Normally Winamp opens unicode files by their DOS 8.3 name. #define this to use wchar_t filenames,
 * which must be opened with _wfopen in a WINAMP_STREAMFILE (needed for dual files like .pos).
 * Only for Winamp paths, other parts would need #define UNICODE for Windows. */
#ifdef VGM_WINAMP_UNICODE
#define UNICODE_INPUT_PLUGIN
#endif

#ifdef _MSC_VER
#define _CRT_SECURE_NO_DEPRECATE
#endif

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>

#include <string.h>
#include <ctype.h>
#include <math.h>

#include "../src/vgmstream.h"
#include "../src/plugins.h"
#include "sdk/in2.h"
#include "sdk/wa_ipc.h"
#include "sdk/ipc_pe.h"
#include "resource.h"


/* ************************************* */
/* IN_CONFIG                             */
/* ************************************* */

extern In_Module input_module;
extern int priority_values[7];

typedef enum {
    REPLAYGAIN_NONE,
    REPLAYGAIN_ALBUM,
    REPLAYGAIN_TRACK
} replay_gain_type_t;

/* loaded settings */
typedef struct {
    int thread_priority;

    double fade_time;
    double fade_delay;
    double loop_count;
    int ignore_loop;
    int loop_forever;

    int disable_subsongs;
    int downmix_channels;
    int tagfile_disable;
    int force_title;
    int exts_unknown_on;
    int exts_common_on;

    replay_gain_type_t gain_type;
    replay_gain_type_t clip_type;

    int is_xmplay;
} winamp_settings_t;

extern winamp_settings_t defaults;
extern winamp_settings_t settings;

/* in_config.c */
void load_defaults(winamp_settings_t* defaults);
void load_config(In_Module* input_module, winamp_settings_t* settings, winamp_settings_t* defaults);
INT_PTR CALLBACK configDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);


/* logger */
typedef struct winamp_log_t winamp_log_t;
void logger_init();
void logger_free();
void logger_callback(int level, const char* str);
const char** logger_get_lines(int* p_max);

extern winamp_log_t* walog;


/* ************************************* */
/* IN_UNICODE                            */
/* ************************************* */
//todo safe ops
//todo there must be a better way to handle unicode...
#ifdef UNICODE_INPUT_PLUGIN
#define wa_strcmp wcscmp
#define wa_strncmp wcsncmp
#define wa_strcpy wcscpy
#define wa_strncpy wcsncpy
#define wa_strcat wcscat
#define wa_strlen wcslen
#define wa_strchr wcschr
#define wa_sscanf swscanf
#define wa_snprintf _snwprintf
#define wa_strrchr wcsrchr
#define wa_fileinfo fileinfoW
#define wa_IPC_PE_INSERTFILENAME IPC_PE_INSERTFILENAMEW
#define wa_L(x) L ##x
#else
#define wa_strcmp strcmp
#define wa_strncmp strncmp
#define wa_strcpy strcpy
#define wa_strncpy strncpy
#define wa_strcat strcat
#define wa_strlen strlen
#define wa_strchr strchr
#define wa_sscanf sscanf
#define wa_snprintf snprintf
#define wa_strrchr strrchr
#define wa_fileinfo fileinfo
#define wa_IPC_PE_INSERTFILENAME IPC_PE_INSERTFILENAME
#define wa_L(x) x
#endif

/* converts from utf16 to utf8 (if unicode is on) */
static inline void wa_ichar_to_char(char *dst, size_t dstsize, const in_char *wsrc) {
#ifdef UNICODE_INPUT_PLUGIN
    /* converto to UTF8 codepage, default separate bytes, source wstr, wstr length */
    //int size_needed = WideCharToMultiByte(CP_UTF8,0, src,-1, NULL,0, NULL, NULL);
    WideCharToMultiByte(CP_UTF8,0, wsrc,-1, dst,dstsize, NULL, NULL);
#else
    strncpy(dst, wsrc, dstsize);
    dst[dstsize - 1] = '\0';
#endif
}

/* converts from utf8 to utf16 (if unicode is on) */
static inline void wa_char_to_ichar(in_char *wdst, size_t wdstsize, const char *src) {
#ifdef UNICODE_INPUT_PLUGIN
    //int size_needed = MultiByteToWideChar(CP_UTF8,0, src,-1, NULL,0);
    MultiByteToWideChar(CP_UTF8,0, src,-1, wdst,wdstsize);
#else
    strncpy(wdst, src, wdstsize);
    wdst[wdstsize - 1] = '\0';
#endif
}

/* copies from utf16 to utf16 (if unicode is active) */
static inline void wa_wchar_to_ichar(in_char *wdst, size_t wdstsize, const wchar_t *src) {
#ifdef UNICODE_INPUT_PLUGIN
    wcscpy(wdst,src);
#else
    strcpy(wdst,src); //todo ???
#endif
}

/* copies from utf16 to utf16 */
static inline void wa_char_to_wchar(wchar_t *wdst, size_t wdstsize, const char *src) {
#ifdef UNICODE_INPUT_PLUGIN
    MultiByteToWideChar(CP_UTF8,0, src,-1, wdst,wdstsize);
#else
    strcpy(wdst,src); //todo ???
#endif
}


//todo snprintf
/* Windows unicode, separate from Winamp's unicode flag */
#ifdef UNICODE
#define cfg_strncpy wcsncpy
#define cfg_strncat wcsncat
#define cfg_sprintf _swprintf
#define cfg_sscanf swscanf
#define cfg_strlen wcslen
#define cfg_strrchr wcsrchr
#else
#define cfg_strncpy strncpy
#define cfg_strncat strncat
#define cfg_sprintf sprintf
#define cfg_sscanf sscanf
#define cfg_strlen strlen
#define cfg_strrchr strrchr
#endif

/* converts from utf8 to utf16 (if unicode is active) */
static inline void cfg_char_to_wchar(TCHAR *wdst, size_t wdstsize, const char *src) {
#ifdef UNICODE
    //int size_needed = MultiByteToWideChar(CP_UTF8,0, src,-1, NULL,0);
    MultiByteToWideChar(CP_UTF8,0, src,-1, wdst,wdstsize);
#else
    strcpy(wdst,src);
#endif
}


/* ************************************* */
/* IN_STREAMFILE                         */
/* ************************************* */

/* in_streamfile.c */
STREAMFILE* open_winamp_streamfile_by_ipath(const in_char* wpath);

#endif /*_IN_VGMSTREAM_*/
