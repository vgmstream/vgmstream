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
#include <stdio.h>
#include <stdbool.h>

#include "../src/libvgmstream.h"
#include "sdk/in2.h"
#include "sdk/wa_ipc.h"
#include "sdk/ipc_pe.h"
#include "resource.h"


/* ************************************* */
/* IN_CONFIG                             */
/* ************************************* */

#define WINAMP_PATH_LIMIT 4096

extern In_Module input_module;
extern const int priority_values[7];

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

    bool is_xmplay;
} winamp_settings_t;

extern winamp_settings_t defaults;
extern winamp_settings_t settings;

void load_defaults(winamp_settings_t* defaults);
void load_config(In_Module* input_module, winamp_settings_t* settings, winamp_settings_t* defaults);
INT_PTR CALLBACK configDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);


/* ************************************* */
/* IN_LOG                                */
/* ************************************* */

typedef struct winamp_log_t winamp_log_t;
void logger_init();
void logger_free();
void logger_callback(int level, const char* str);
const char** logger_get_lines(int* p_max);

extern winamp_log_t* walog;


/* ************************************* */
/* IN_UNICODE                            */
/* ************************************* */
//TODO there must be a better way to handle unicode...

#ifdef _MSC_VER
  #define strcasecmp _stricmp
#endif

#ifdef UNICODE_INPUT_PLUGIN
#define wa_strcmp wcscmp
#define wa_strncmp wcsncmp
#define wa_strchr wcschr
#define wa_strstr wcsstr
#define wa_strlen wcslen
#define wa_strrchr wcsrchr
#define wa_sscanf swscanf
#define wa_snprintf _snwprintf

#define wa_fileinfo fileinfoW
#define wa_IPC_PE_INSERTFILENAME IPC_PE_INSERTFILENAMEW
#define wa_L(x) L ##x
#else
#define wa_strcmp strcmp
#define wa_strncmp strncmp
#define wa_strchr strchr
#define wa_strstr strstr
#define wa_strlen strlen
#define wa_strrchr strrchr
#define wa_sscanf sscanf
#define wa_snprintf _snprintf

#define wa_fileinfo fileinfo
#define wa_IPC_PE_INSERTFILENAME IPC_PE_INSERTFILENAME
#define wa_L(x) x
#endif

/* converts: utf16/utf8 to utf8 (depending on unicode flag) */
static inline void wa_ichar_to_char(char* dst, size_t dst_size, const in_char* isrc) {
    if (!dst || dst_size <= 0 || !isrc)
        return;

#ifdef UNICODE_INPUT_PLUGIN
    int isrc_size = -1; // assumes input is null-terminated; will return 0 if not enough dst_size
    int done = WideCharToMultiByte(CP_UTF8, 0, isrc, isrc_size, dst, dst_size, NULL, NULL);
    if (done <= 0)
        dst[0] = 0;
#else
    _snprintf(dst, dst_size, "%s", isrc);
    dst[dst_size - 1] = '\0'; // Windows's buggy _sn*printf
#endif
}

/* converts: utf8 to utf16/utf8 (depending on unicode flag) */
static inline void wa_char_to_ichar(in_char* idst, size_t idst_size, const char* src) {
    if (!idst || idst_size <= 0 || !src)
        return;

#ifdef UNICODE_INPUT_PLUGIN
    int src_size = -1; // assumes input is null-terminated
    int done = MultiByteToWideChar(CP_UTF8, 0, src, src_size, idst, idst_size);
    if (done <= 0)
        idst[0] = 0;
#else
    _snprintf(idst, idst_size, "%s", src);
    idst[idst_size - 1] = '\0'; // Windows's buggy _sn*printf
#endif
}

/* copies from utf16 to utf16/utf8 (depending on unicode flag) */
static inline void wa_wchar_to_ichar(in_char* idst, size_t idst_size, const wchar_t* wsrc) {
    if (!idst || idst_size <= 0 || !wsrc)
        return;

#ifdef UNICODE_INPUT_PLUGIN
    _snwprintf(idst, idst_size, L"%s", wsrc);
    idst[idst_size - 1] = '\0'; // Windows's buggy _sn*printf
#else
    int wsrc_size = -1; // assumes input is null-terminated; will return 0 if not enough dst_size
    int done = WideCharToMultiByte(CP_UTF8, 0, wsrc, wsrc_size, idst, idst_size, NULL, NULL);
    if (done <= 0)
        idst[0] = 0;
#endif
}

/* copies: utf8 to utf16 */
static inline void wa_char_to_wchar(wchar_t* wdst, size_t wdst_size, const char* src) {
    if (!wdst || wdst_size <= 0 || !src)
        return;

    int src_size = -1; // assumes input is null-terminated
    int done = MultiByteToWideChar(CP_UTF8, 0, src, src_size, wdst, wdst_size);
    if (done <= 0)
        wdst[0] = 0;
}

/* copies: utf16/utf8 to utf16/utf8 */
static inline void wa_istrcpy(in_char* idst, size_t idst_size, const in_char* isrc) {
    if (!idst || idst_size <= 0 || !isrc)
        return;

#ifdef UNICODE_INPUT_PLUGIN
    _snwprintf(idst, idst_size, L"%s", isrc);
    idst[idst_size - 1] = '\0'; // Windows's buggy _sn*printf
#else
    _snprintf(idst, idst_size, "%s", isrc);
    idst[idst_size - 1] = '\0'; // Windows's buggy _sn*printf
#endif
}

/* concats: utf16/utf8 to utf16/utf8 */
static inline void wa_istrcat(in_char* dst, size_t dst_size, const in_char* src) {
    if (!dst || dst_size <= 0 || !src)
        return;

    size_t str_len = wa_strlen(dst);
    if (str_len >= dst_size)
        return;

    wa_istrcpy(dst + str_len, dst_size - str_len, src);
}

/* concats: utf8 to utf8 */
static inline void wa_lstrcpy(char* dst, size_t dst_size, const char* src) {
    if (!dst || dst_size <= 0 || !src)
        return;

    _snprintf(dst, dst_size, "%s", src);
    dst[dst_size - 1] = '\0'; // Windows's buggy _sn*printf
}


//todo snprintf
/* Windows unicode, separate from Winamp's unicode flag */
#ifdef UNICODE
#define cfg_snprintf _snwprintf
#define cfg_sscanf swscanf
#define cfg_strlen wcslen
#define cfg_strrchr wcsrchr
//#define cfg_strncpy wcsncpy
#define cfg_strncat wcsncat

#else
#define cfg_snprintf _snprintf
#define cfg_sscanf sscanf
#define cfg_strlen strlen
#define cfg_strrchr strrchr
//#define cfg_strncpy strncpy
#define cfg_strncat strncat
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


/* in_streamfile.c */
libstreamfile_t* open_winamp_streamfile_by_ipath(const in_char* wpath);

void build_extension_list(char* extension_list, int list_size);

bool split_subsongs(const in_char* filename, int subsong_index, libvgmstream_t* vgmstream);

#endif
