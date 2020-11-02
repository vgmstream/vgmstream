/**
 * vgmstream for Winamp
 */

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
#include <stdio.h>
#include <io.h>
#include <string.h>
#include <ctype.h>

#include "../src/vgmstream.h"
#include "../src/plugins.h"
#include "sdk/in2.h"
#include "sdk/wa_ipc.h"
#include "sdk/ipc_pe.h"
#include "resource.h"


#ifndef VERSION
#include "version.h"
#endif
#ifndef VERSION
#define VERSION "(unknown version)"
#endif

#define PLUGIN_DESCRIPTION "vgmstream plugin " VERSION " " __DATE__


/* ************************************* */

#define EXT_BUFFER_SIZE 200

/* plugin module (declared at the bottom of this file) */
In_Module input_module;
DWORD WINAPI __stdcall decode(void *arg);

/* Winamp Play extension list, to accept and associate extensions in Windows */
#define EXTENSION_LIST_SIZE   (0x2000 * 6)
/* fixed list to simplify but could also malloc/free on init/close */
char working_extension_list[EXTENSION_LIST_SIZE] = {0};

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

/* current play state */
typedef struct {
    int paused;
    int decode_abort;
    int seek_sample;
    int decode_pos_ms;
    int decode_pos_samples;
    int length_samples;
    int output_channels;
    double volume;
} winamp_state_t;


/* Winamp needs at least 576 16-bit samples, stereo, doubled in case DSP effects are active */
#define SAMPLE_BUFFER_SIZE 576
const char* tagfile_name = "!tags.m3u";

/* plugin state */
HANDLE decode_thread_handle = INVALID_HANDLE_VALUE;

VGMSTREAM* vgmstream = NULL;
in_char lastfn[PATH_LIMIT] = {0}; /* name of the currently playing file */

winamp_settings_t defaults;
winamp_settings_t settings;
winamp_state_t state;
short sample_buffer[SAMPLE_BUFFER_SIZE*2 * VGMSTREAM_MAX_CHANNELS]; //todo maybe should be dynamic


/* ************************************* */
/* IN_UNICODE                            */
/* ************************************* */
//todo safe ops
//todo there must be a better way to handle unicode...
#ifdef UNICODE_INPUT_PLUGIN
#define wa_strcmp wcscmp
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
static void wa_ichar_to_char(char *dst, size_t dstsize, const in_char *wsrc) {
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
static void wa_char_to_ichar(in_char *wdst, size_t wdstsize, const char *src) {
#ifdef UNICODE_INPUT_PLUGIN
    //int size_needed = MultiByteToWideChar(CP_UTF8,0, src,-1, NULL,0);
    MultiByteToWideChar(CP_UTF8,0, src,-1, wdst,wdstsize);
#else
    strncpy(wdst, src, wdstsize);
    wdst[wdstsize - 1] = '\0';
#endif
}

/* copies from utf16 to utf16 (if unicode is active) */
static void wa_wchar_to_ichar(in_char *wdst, size_t wdstsize, const wchar_t *src) {
#ifdef UNICODE_INPUT_PLUGIN
    wcscpy(wdst,src);
#else
    strcpy(wdst,src); //todo ???
#endif
}

/* copies from utf16 to utf16 */
static void wa_char_to_wchar(wchar_t *wdst, size_t wdstsize, const char *src) {
#ifdef UNICODE_INPUT_PLUGIN
    MultiByteToWideChar(CP_UTF8,0, src,-1, wdst,wdstsize);
#else
    strcpy(wdst,src); //todo ???
#endif
}

/* opens a utf16 (unicode) path */
static FILE* wa_fopen(const in_char *wpath) {
#ifdef UNICODE_INPUT_PLUGIN
    return _wfopen(wpath,L"rb");
#else
    return fopen(wpath,"rb");
#endif
}

/* dupes a utf16 (unicode) file */
static FILE* wa_fdopen(int fd) {
#ifdef UNICODE_INPUT_PLUGIN
    return _wfdopen(fd,L"rb");
#else
    return fdopen(fd,"rb");
#endif
}

/* ************************************* */
/* IN_STREAMFILE                         */
/* ************************************* */

/* a STREAMFILE that operates via STDIOSTREAMFILE but handles Winamp's unicode (in_char) paths */
typedef struct {
    STREAMFILE sf;
    STREAMFILE *stdiosf;
    FILE *infile_ref; /* pointer to the infile in stdiosf (partially handled by stdiosf) */
} WINAMP_STREAMFILE;

static STREAMFILE *open_winamp_streamfile_by_file(FILE *infile, const char * path);
static STREAMFILE *open_winamp_streamfile_by_ipath(const in_char *wpath);

static size_t wasf_read(WINAMP_STREAMFILE* sf, uint8_t* dest, off_t offset, size_t length) {
    return sf->stdiosf->read(sf->stdiosf, dest, offset, length);
}

static off_t wasf_get_size(WINAMP_STREAMFILE* sf) {
    return sf->stdiosf->get_size(sf->stdiosf);
}

static off_t wasf_get_offset(WINAMP_STREAMFILE* sf) {
    return sf->stdiosf->get_offset(sf->stdiosf);
}

static void wasf_get_name(WINAMP_STREAMFILE* sf, char* buffer, size_t length) {
    sf->stdiosf->get_name(sf->stdiosf, buffer, length);
}

static STREAMFILE *wasf_open(WINAMP_STREAMFILE* sf, const char* const filename, size_t buffersize) {
    in_char wpath[PATH_LIMIT];
    char name[PATH_LIMIT];

    if (!filename)
        return NULL;

#if !defined (__ANDROID__) && !defined (_MSC_VER)
    /* When enabling this for MSVC it'll seemingly work, but there are issues possibly related to underlying
     * IO buffers when using dup(), noticeable by re-opening the same streamfile with small buffer sizes
     * (reads garbage). This reportedly causes issues in Android too */

    sf->stdiosf->get_name(sf->stdiosf, name, PATH_LIMIT);
    /* if same name, duplicate the file descriptor we already have open */ //unsure if all this is needed
    if (sf->infile_ref && !strcmp(name,filename)) {
        int new_fd;
        FILE *new_file;

        if (((new_fd = dup(fileno(sf->infile_ref))) >= 0) && (new_file = wa_fdopen(new_fd))) {
            STREAMFILE *new_sf = open_winamp_streamfile_by_file(new_file, filename);
            if (new_sf)
                return new_sf;
            fclose(new_file);
        }
        if (new_fd >= 0 && !new_file)
            close(new_fd); /* fdopen may fail when opening too many files */

        /* on failure just close and try the default path (which will probably fail a second time) */
    }
#endif

    /* STREAMFILEs carry char/UTF8 names, convert to wchar for Winamp */
    wa_char_to_ichar(wpath, PATH_LIMIT, filename);
    return open_winamp_streamfile_by_ipath(wpath);
}

static void wasf_close(WINAMP_STREAMFILE* sf) {
    /* closes infile_ref + frees in the internal STDIOSTREAMFILE (fclose for wchar is not needed) */
    sf->stdiosf->close(sf->stdiosf);
    free(sf); /* and the current struct */
}

static STREAMFILE *open_winamp_streamfile_by_file(FILE* file, const char* path) {
    WINAMP_STREAMFILE* this_sf = NULL;
    STREAMFILE* stdiosf = NULL;

    this_sf = calloc(1,sizeof(WINAMP_STREAMFILE));
    if (!this_sf) goto fail;

    stdiosf = open_stdio_streamfile_by_file(file, path);
    if (!stdiosf) goto fail;

    this_sf->sf.read = (void*)wasf_read;
    this_sf->sf.get_size = (void*)wasf_get_size;
    this_sf->sf.get_offset = (void*)wasf_get_offset;
    this_sf->sf.get_name = (void*)wasf_get_name;
    this_sf->sf.open = (void*)wasf_open;
    this_sf->sf.close = (void*)wasf_close;

    this_sf->stdiosf = stdiosf;
    this_sf->infile_ref = file;

    return &this_sf->sf; /* pointer to STREAMFILE start = rest of the custom data follows */

fail:
    close_streamfile(stdiosf);
    free(this_sf);
    return NULL;
}


static STREAMFILE* open_winamp_streamfile_by_ipath(const in_char* wpath) {
    FILE* infile = NULL;
    STREAMFILE* sf;
    char path[PATH_LIMIT];


    /* convert to UTF-8 if needed for internal use */
    wa_ichar_to_char(path,PATH_LIMIT, wpath);

    /* open a FILE from a Winamp (possibly UTF-16) path */
    infile = wa_fopen(wpath);
    if (!infile) {
        /* allow non-existing files in some cases */
        if (!vgmstream_is_virtual_filename(path))
            return NULL;
    }

    sf = open_winamp_streamfile_by_file(infile,path);
    if (!sf) {
        if (infile) fclose(infile);
    }

    return sf;
}

/* opens vgmstream for winamp */
static VGMSTREAM* init_vgmstream_winamp(const in_char* fn, int stream_index) {
    VGMSTREAM* vgmstream = NULL;

    //return init_vgmstream(fn);

    /* manually init streamfile to pass the stream index */
    STREAMFILE* sf = open_winamp_streamfile_by_ipath(fn); //open_stdio_streamfile(fn);
    if (sf) {
        sf->stream_index = stream_index;
        vgmstream = init_vgmstream_from_STREAMFILE(sf);
        close_streamfile(sf);
    }

    return vgmstream;
}


/* ************************************* */
/* IN_CONFIG                             */
/* ************************************* */
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
static void cfg_char_to_wchar(TCHAR *wdst, size_t wdstsize, const char *src) {
#ifdef UNICODE
    //int size_needed = MultiByteToWideChar(CP_UTF8,0, src,-1, NULL,0);
    MultiByteToWideChar(CP_UTF8,0, src,-1, wdst,wdstsize);
#else
    strcpy(wdst,src);
#endif
}

/* config */
#define CONFIG_APP_NAME  TEXT("vgmstream plugin")
#define CONFIG_INI_NAME  TEXT("plugin.ini")

#define INI_FADE_TIME           TEXT("fade_seconds")
#define INI_FADE_DELAY          TEXT("fade_delay")
#define INI_LOOP_COUNT          TEXT("loop_count")
#define INI_THREAD_PRIORITY     TEXT("thread_priority")
#define INI_LOOP_FOREVER        TEXT("loop_forever")
#define INI_IGNORE_LOOP         TEXT("ignore_loop")
#define INI_DISABLE_SUBSONGS    TEXT("disable_subsongs")
#define INI_DOWNMIX_CHANNELS    TEXT("downmix_channels")
#define INI_TAGFILE_DISABLE     TEXT("tagfile_disable")
#define INI_FORCE_TITLE         TEXT("force_title")
#define INI_EXTS_UNKNOWN_ON     TEXT("exts_unknown_on")
#define INI_EXTS_COMMON_ON      TEXT("exts_common_on")
#define INI_GAIN_TYPE           TEXT("gain_type")
#define INI_CLIP_TYPE           TEXT("clip_type")

TCHAR *dlg_priority_strings[] = {
        TEXT("Idle"),
        TEXT("Lowest"),
        TEXT("Below Normal"),
        TEXT("Normal"),
        TEXT("Above Normal"),
        TEXT("Highest (not recommended)"),
        TEXT("Time Critical (not recommended)")
};
TCHAR *dlg_replaygain_strings[] = {
        TEXT("None"),
        TEXT("Album"),
        TEXT("Peak")
};

int priority_values[] = {
        THREAD_PRIORITY_IDLE,
        THREAD_PRIORITY_LOWEST,
        THREAD_PRIORITY_BELOW_NORMAL,
        THREAD_PRIORITY_NORMAL,
        THREAD_PRIORITY_ABOVE_NORMAL,
        THREAD_PRIORITY_HIGHEST,
        THREAD_PRIORITY_TIME_CRITICAL
};

// todo finish UNICODE (requires IPC_GETINIDIRECTORYW from later SDKs to read the ini path properly)
/* Winamp INI reader */
static void ini_get_filename(TCHAR *inifile) {

    if (IsWindow(input_module.hMainWindow) && SendMessage(input_module.hMainWindow, WM_WA_IPC,0,IPC_GETVERSION) >= 0x5000) {
        /* newer Winamp with per-user settings */
        TCHAR *ini_dir = (TCHAR *)SendMessage(input_module.hMainWindow, WM_WA_IPC, 0, IPC_GETINIDIRECTORY);
        cfg_strncpy(inifile, ini_dir, PATH_LIMIT);

        cfg_strncat(inifile, TEXT("\\Plugins\\"), PATH_LIMIT);

        /* can't be certain that \Plugins already exists in the user dir */
        CreateDirectory(inifile,NULL);

        cfg_strncat(inifile, CONFIG_INI_NAME, PATH_LIMIT);
    }
    else {
        /* older winamp with single settings */
        TCHAR *lastSlash;

        GetModuleFileName(NULL, inifile, PATH_LIMIT);
        lastSlash = cfg_strrchr(inifile, TEXT('\\'));

        *(lastSlash + 1) = 0;

        /* XMPlay doesn't have a "plugins" subfolder */
        if (settings.is_xmplay)
            cfg_strncat(inifile, CONFIG_INI_NAME,PATH_LIMIT);
        else
            cfg_strncat(inifile, TEXT("Plugins\\") CONFIG_INI_NAME,PATH_LIMIT);
        /* Maybe should query IPC_GETINIDIRECTORY and use that, not sure what ancient Winamps need.
         * There must be some proper way to handle dirs since other Winamp plugins save config in 
         * XMPlay correctly (this feels like archaeology, try later) */
    }
}


static void ini_get_d(const char *inifile, const char *entry, double defval, double *p_val) {
    TCHAR buf[256];
    TCHAR defbuf[256];
    int consumed, res;

    cfg_sprintf(defbuf, TEXT("%.2lf"), defval);
    GetPrivateProfileString(CONFIG_APP_NAME, entry, defbuf, buf, 256, inifile);
    res = cfg_sscanf(buf, TEXT("%lf%n"), p_val, &consumed);
    if (res < 1 || consumed != cfg_strlen(buf) || *p_val < 0) {
        *p_val = defval;
    }
}
static void ini_get_i(const char *inifile, const char *entry, int defval, int *p_val, int min, int max) {
    *p_val = GetPrivateProfileInt(CONFIG_APP_NAME, entry, defval, inifile);
    if (*p_val < min || *p_val > max) {
        *p_val = defval;
    }
}
static void ini_get_b(const char *inifile, const char *entry, int defval, int *p_val) {
    ini_get_i(inifile, entry, defval, p_val, 0, 1);
}

static void ini_set_d(const char *inifile, const char *entry, double val) {
    TCHAR buf[256];
    cfg_sprintf(buf, TEXT("%.2lf"), val);
    WritePrivateProfileString(CONFIG_APP_NAME, entry, buf, inifile);
}
static void ini_set_i(const char *inifile, const char *entry, int val) {
    TCHAR buf[32];
    cfg_sprintf(buf, TEXT("%d"), val);
    WritePrivateProfileString(CONFIG_APP_NAME, entry, buf, inifile);
}
static void ini_set_b(const char *inifile, const char *entry, int val) {
    ini_set_i(inifile, entry, val);
}

static void load_defaults(winamp_settings_t* defaults) {
    defaults->thread_priority = 3;
    defaults->fade_time = 10.0;
    defaults->fade_delay = 0.0;
    defaults->loop_count = 2.0;
    defaults->loop_forever = 0;
    defaults->ignore_loop = 0;
    defaults->disable_subsongs = 0;
    defaults->downmix_channels = 0;
    defaults->tagfile_disable = 0;
    defaults->force_title = 0;
    defaults->exts_unknown_on = 0;
    defaults->exts_common_on = 0;
    defaults->gain_type = 1;
    defaults->clip_type = 2;
}

static void load_config(winamp_settings_t* settings, winamp_settings_t* defaults) {
    TCHAR inifile[PATH_LIMIT];

    ini_get_filename(inifile);

    ini_get_i(inifile, INI_THREAD_PRIORITY, defaults->thread_priority, &settings->thread_priority, 0, 6);

    ini_get_d(inifile, INI_FADE_TIME, defaults->fade_time, &settings->fade_time);
    ini_get_d(inifile, INI_FADE_DELAY, defaults->fade_delay, &settings->fade_delay);
    ini_get_d(inifile, INI_LOOP_COUNT, defaults->loop_count, &settings->loop_count);

    ini_get_b(inifile, INI_LOOP_FOREVER, defaults->loop_forever, &settings->loop_forever);
    ini_get_b(inifile, INI_IGNORE_LOOP, defaults->ignore_loop, &settings->ignore_loop);

    ini_get_b(inifile, INI_DISABLE_SUBSONGS, defaults->disable_subsongs, &settings->disable_subsongs);
    ini_get_i(inifile, INI_DOWNMIX_CHANNELS, defaults->downmix_channels, &settings->downmix_channels, 0, 64);
    ini_get_b(inifile, INI_TAGFILE_DISABLE, defaults->tagfile_disable, &settings->tagfile_disable);
    ini_get_b(inifile, INI_FORCE_TITLE, defaults->force_title, &settings->force_title);
    ini_get_b(inifile, INI_EXTS_UNKNOWN_ON, defaults->exts_unknown_on, &settings->exts_unknown_on);
    ini_get_b(inifile, INI_EXTS_COMMON_ON, defaults->exts_common_on, &settings->exts_common_on);

    ini_get_i(inifile, INI_GAIN_TYPE, defaults->gain_type, (int*)&settings->gain_type, 0, 3);
    ini_get_i(inifile, INI_CLIP_TYPE, defaults->clip_type, (int*)&settings->clip_type, 0, 3);

    if (settings->loop_forever && settings->ignore_loop)
        settings->ignore_loop = 0;
}

static void save_config(winamp_settings_t* settings) {
    TCHAR inifile[PATH_LIMIT];

    ini_get_filename(inifile);

    ini_set_i(inifile, INI_THREAD_PRIORITY, settings->thread_priority);

    ini_set_d(inifile, INI_FADE_TIME, settings->fade_time);
    ini_set_d(inifile, INI_FADE_DELAY, settings->fade_delay);
    ini_set_d(inifile, INI_LOOP_COUNT, settings->loop_count);

    ini_set_b(inifile, INI_LOOP_FOREVER, settings->loop_forever);
    ini_set_b(inifile, INI_IGNORE_LOOP, settings->ignore_loop);

    ini_set_b(inifile, INI_DISABLE_SUBSONGS, settings->disable_subsongs);
    ini_set_i(inifile, INI_DOWNMIX_CHANNELS, settings->downmix_channels);
    ini_set_b(inifile, INI_TAGFILE_DISABLE, settings->tagfile_disable);
    ini_set_b(inifile, INI_FORCE_TITLE, settings->force_title);
    ini_set_b(inifile, INI_EXTS_UNKNOWN_ON, settings->exts_unknown_on);
    ini_set_b(inifile, INI_EXTS_COMMON_ON, settings->exts_common_on);

    ini_set_i(inifile, INI_GAIN_TYPE, settings->gain_type);
    ini_set_i(inifile, INI_CLIP_TYPE, settings->clip_type);
}


static void dlg_input_set_d(HWND hDlg, int idc, double val) {
    TCHAR buf[256];
    cfg_sprintf(buf, TEXT("%.2lf"), val);
    SetDlgItemText(hDlg, idc, buf);
}
static void dlg_input_set_i(HWND hDlg, int idc, int val) {
    TCHAR buf[32];
    cfg_sprintf(buf, TEXT("%d"), val);
    SetDlgItemText(hDlg, idc, buf);
}
static void dlg_check_set(HWND hDlg, int idc, int val) {
    CheckDlgButton(hDlg, idc, val ? BST_CHECKED : BST_UNCHECKED);
}
static void cfg_combo_set(HWND hDlg, int idc, int val, TCHAR **list, int list_size) {
    int i;
    HANDLE hCombo = GetDlgItem(hDlg, idc);
    for (i = 0; i < list_size; i++) {
        SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)list[i]);
    }
    SendMessage(hCombo, CB_SETCURSEL, val, 0);
}
static void cfg_slider_set(HWND hDlg, int idc1, int idc2, int val, int min, int max, TCHAR **list) {
    HANDLE hSlider = GetDlgItem(hDlg, idc1);
    SendMessage(hSlider, TBM_SETRANGE, (WPARAM)TRUE, (LPARAM)MAKELONG(min, max));
    SendMessage(hSlider, TBM_SETPOS,   (WPARAM)TRUE, (LPARAM)val+1);
    SetDlgItemText(hDlg, idc2, list[val]);
}

static void dlg_input_get_d(HWND hDlg, int idc, double *p_val, LPCTSTR error, int *p_err) {
    TCHAR buf[256];
    int res, consumed;
    double defval = *p_val;

    GetDlgItemText(hDlg, idc, buf, 256);
    res = cfg_sscanf(buf, TEXT("%lf%n"), p_val, &consumed);
    if (res < 1 || consumed != cfg_strlen(buf) || *p_val < 0) {
        MessageBox(hDlg, error, NULL, MB_OK|MB_ICONERROR);
        *p_val = defval;
        *p_err = 1;
    }
}
static void dlg_input_get_i(HWND hDlg, int idc, int *p_val, LPCTSTR error, int *p_err) {
    TCHAR buf[32];
    int res, consumed;
    int defval = *p_val;

    GetDlgItemText(hDlg, idc, buf, 32);
    res = cfg_sscanf(buf, TEXT("%d%n"), p_val, &consumed);
    if (res < 1 || consumed != cfg_strlen(buf) || *p_val < 0) {
        MessageBox(hDlg, error, NULL, MB_OK|MB_ICONERROR);
        *p_val = defval;
        *p_err = 1;
    }
}
static void dlg_check_get(HWND hDlg, int idc, int *p_val) {
    *p_val = (IsDlgButtonChecked(hDlg, idc) == BST_CHECKED);
}
static void dlg_combo_get(HWND hDlg, int idc, int *p_val) {
    *p_val = SendMessage(GetDlgItem(hDlg, idc), CB_GETCURSEL, 0, 0);
}

static int dlg_load_form(HWND hDlg, winamp_settings_t* settings) {
    int err = 0;
    dlg_input_get_d(hDlg, IDC_FADE_TIME, &settings->fade_time,  TEXT("Fade Length must be a positive number"), &err);
    dlg_input_get_d(hDlg, IDC_FADE_DELAY, &settings->fade_delay, TEXT("Fade Delay must be a positive number"), &err);
    dlg_input_get_d(hDlg, IDC_LOOP_COUNT, &settings->loop_count, TEXT("Loop Count must be a positive number"), &err);

    dlg_check_get(hDlg, IDC_LOOP_FOREVER, &settings->loop_forever);
    dlg_check_get(hDlg, IDC_IGNORE_LOOP, &settings->ignore_loop);

    dlg_check_get(hDlg, IDC_DISABLE_SUBSONGS, &settings->disable_subsongs);
    dlg_input_get_i(hDlg, IDC_DOWNMIX_CHANNELS, &settings->downmix_channels, TEXT("Downmix must be a positive integer number"), &err);
    dlg_check_get(hDlg, IDC_TAGFILE_DISABLE, &settings->tagfile_disable);
    dlg_check_get(hDlg, IDC_FORCE_TITLE, &settings->force_title);
    dlg_check_get(hDlg, IDC_EXTS_UNKNOWN_ON, &settings->exts_unknown_on);
    dlg_check_get(hDlg, IDC_EXTS_COMMON_ON, &settings->exts_common_on);

    dlg_combo_get(hDlg, IDC_GAIN_TYPE, (int*)&settings->gain_type);
    dlg_combo_get(hDlg, IDC_CLIP_TYPE, (int*)&settings->clip_type);

    return err ? 0 : 1;
}

static void dlg_save_form(HWND hDlg, winamp_settings_t* settings, int reset) {
    cfg_slider_set(hDlg, IDC_THREAD_PRIORITY_SLIDER, IDC_THREAD_PRIORITY_TEXT, settings->thread_priority, 1, 7, dlg_priority_strings);

    dlg_input_set_d(hDlg, IDC_FADE_TIME, settings->fade_time);
    dlg_input_set_d(hDlg, IDC_FADE_DELAY, settings->fade_delay);
    dlg_input_set_d(hDlg, IDC_LOOP_COUNT, settings->loop_count);

    dlg_check_set(hDlg, IDC_LOOP_FOREVER, settings->loop_forever);
    dlg_check_set(hDlg, IDC_IGNORE_LOOP, settings->ignore_loop);
    dlg_check_set(hDlg, IDC_LOOP_NORMALLY, (!settings->loop_forever && !settings->ignore_loop));

    dlg_check_set(hDlg, IDC_DISABLE_SUBSONGS, settings->disable_subsongs);
    dlg_input_set_i(hDlg, IDC_DOWNMIX_CHANNELS, settings->downmix_channels);
    dlg_check_set(hDlg, IDC_TAGFILE_DISABLE, settings->tagfile_disable);
    dlg_check_set(hDlg, IDC_FORCE_TITLE, settings->force_title);
    dlg_check_set(hDlg, IDC_EXTS_UNKNOWN_ON, settings->exts_unknown_on);
    dlg_check_set(hDlg, IDC_EXTS_COMMON_ON, settings->exts_common_on);

    cfg_combo_set(hDlg, IDC_GAIN_TYPE, settings->gain_type, dlg_replaygain_strings, (reset ? 0 : 3));
    cfg_combo_set(hDlg, IDC_CLIP_TYPE, settings->clip_type, dlg_replaygain_strings, (reset ? 0 : 3));

}

/* config dialog handler */
INT_PTR CALLBACK configDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static int priority;

    switch (uMsg) {
        case WM_CLOSE: /* hide dialog */
            EndDialog(hDlg,TRUE);
            return TRUE;

        case WM_INITDIALOG: /* open dialog: load form with current settings */
            priority = settings.thread_priority;
            dlg_save_form(hDlg, &settings, 0);
            break;

        case WM_COMMAND: /* button presses */
            switch (GET_WM_COMMAND_ID(wParam, lParam)) {
                case IDOK: { /* read and verify new values, save and close */
                    int ok;

                    settings.thread_priority = priority;
                    ok = dlg_load_form(hDlg, &settings);
                    if (!ok) break; /* this leaves values changed though */

                    save_config(&settings);

                    EndDialog(hDlg,TRUE);
                    break;
                }

                case IDCANCEL: /* cancel dialog */
                    EndDialog(hDlg,TRUE);
                    break;

                case IDC_DEFAULT_BUTTON: { /* reset values */
                    priority = defaults.thread_priority;
                    dlg_save_form(hDlg, &defaults, 1);

                    /* we don't save settings here as user can still cancel the dialog */
                    break;
                }

                default:
                    return FALSE;
            }
            return FALSE;

        case WM_HSCROLL: /* priority scroll */
            if ((struct HWND__*)lParam == GetDlgItem(hDlg, IDC_THREAD_PRIORITY_SLIDER)) {
                if (LOWORD(wParam) == TB_THUMBPOSITION || LOWORD(wParam) == TB_THUMBTRACK) {
                    priority = HIWORD(wParam)-1;
                }
                else {
                    priority = SendMessage(GetDlgItem(hDlg,IDC_THREAD_PRIORITY_SLIDER),TBM_GETPOS,0,0)-1;
                }
                SetDlgItemText(hDlg, IDC_THREAD_PRIORITY_TEXT, dlg_priority_strings[priority]);
            }
            break;

        default:
            return FALSE;
    }

    return TRUE;
}


/* ***************************************** */
/* IN_VGMSTREAM UTILS                        */
/* ***************************************** */

/* makes a modified filename, suitable to pass parameters around */
static void make_fn_subsong(in_char * dst, int dst_size, const in_char * filename, int stream_index) {
    /* Follows "(file)(config)(ext)". Winamp needs to "see" (ext) to validate, and file goes first so relative
     * files work in M3Us (path is added). Protocols a la "vgmstream://(config)(file)" work but don't get full paths. */
    wa_snprintf(dst,dst_size, wa_L("%s|$s=%i|.vgmstream"), filename, stream_index);
}

/* unpacks the subsongs by adding entries to the playlist */
static int split_subsongs(const in_char * filename, int stream_index, VGMSTREAM *vgmstream) {
    int i, playlist_index;
    HWND hPlaylistWindow;


    if (settings.disable_subsongs || vgmstream->num_streams <= 1)
        return 0; /* don't split if no subsongs */
    if (stream_index > 0 || vgmstream->stream_index > 0)
        return 0; /* no split if already playing subsong */

    hPlaylistWindow = (HWND)SendMessage(input_module.hMainWindow, WM_WA_IPC, IPC_GETWND_PE, IPC_GETWND);
    playlist_index = SendMessage(input_module.hMainWindow,WM_WA_IPC,0,IPC_GETLISTPOS);

    /* The only way to pass info around in Winamp is encoding it into the filename, so a fake name
     * is created with the index. Then, winamp_Play (and related) intercepts and reads the index. */
    for (i = 0; i < vgmstream->num_streams; i++) {
        in_char stream_fn[PATH_LIMIT];

        make_fn_subsong(stream_fn,PATH_LIMIT, filename, (i+1)); /* encode index in filename */

        /* insert at index */
        {
            COPYDATASTRUCT cds = {0};
            wa_fileinfo f;

            wa_strncpy(f.file, stream_fn,MAX_PATH-1);
            f.file[MAX_PATH-1] = '\0';
            f.index = playlist_index + (i+1);
            cds.dwData = wa_IPC_PE_INSERTFILENAME;
            cds.lpData = (void*)&f;
            cds.cbData = sizeof(wa_fileinfo);
            SendMessage(hPlaylistWindow,WM_COPYDATA,0,(LPARAM)&cds);
        }
        /* IPC_ENQUEUEFILE can pre-set the title without needing the Playlist handle, but can't insert at index */
    }

    /* remove current file from the playlist */
    SendMessage(hPlaylistWindow, WM_WA_IPC, IPC_PE_DELETEINDEX, playlist_index);

    /* autoplay doesn't always advance to the first unpacked track, but manually fails somehow */
    //SendMessage(input_module.hMainWindow,WM_WA_IPC,playlist_index,IPC_SETPLAYLISTPOS);
    //SendMessage(input_module.hMainWindow,WM_WA_IPC,0,IPC_STARTPLAY);

    return 1;
}

/* parses a modified filename ('fakename') extracting tags parameters (NULL tag for first = filename) */
static int parse_fn_string(const in_char * fn, const in_char * tag, in_char * dst, int dst_size) {
    const in_char *end = wa_strchr(fn,'|');

    if (tag==NULL) {
        wa_strcpy(dst,fn);
        if (end)
            dst[end - fn] = '\0';
        return 1;
    }

    dst[0] = '\0';
    return 0;
}
static int parse_fn_int(const in_char * fn, const in_char * tag, int * num) {
    const in_char * start = wa_strchr(fn,'|');

    if (start > 0) {
        wa_sscanf(start+1, wa_L("$s=%i "), num);
        return 1;
    } else {
        *num = 0;
        return 0;
    }
}

/* try to detect XMPlay, which can't interact with the playlist = no splitting */
static int is_xmplay() {
    if (GetModuleHandle( TEXT("xmplay.exe") ))
        return 1;
    if (GetModuleHandle( TEXT("xmp-wadsp.dll") ))
        return 1;
    if (GetModuleHandle( TEXT("xmp-wma.dll") ))
        return 1;

    return 0;
}

/* Adds ext to Winamp's extension list */
static void add_extension(char *dst, int dst_len, const char *ext) {
    char buf[EXT_BUFFER_SIZE];
    char ext_upp[EXT_BUFFER_SIZE];
    int ext_len, written;
    int i,j;
    if (dst_len <= 1)
        return;

    ext_len = strlen(ext);

    /* find end of dst (double \0), saved in i */
    for (i = 0; i < dst_len - 2 && (dst[i] || dst[i+1]); i++)
        ;

    /* check if end reached or not enough room to add */
    if (i == dst_len - 2 || i + EXT_BUFFER_SIZE+2 > dst_len - 2 || ext_len * 3 + 20+2 > EXT_BUFFER_SIZE) {
        dst[i] = '\0';
        dst[i+1] = '\0';
        return;
    }

    if (i > 0)
        i++;

    /* uppercase ext */
    for (j = 0; j < ext_len; j++)
        ext_upp[j] = toupper(ext[j]);
    ext_upp[j] = '\0';

    /* copy new extension + double null terminate */
    /* ex: "vgmstream\0vgmstream Audio File (*.VGMSTREAM)\0" */
    written = snprintf(buf,sizeof(buf)-1, "%s%c%s Audio File (*.%s)%c", ext,'\0',ext_upp,ext_upp,'\0');
    for (j = 0; j < written; i++,j++)
        dst[i] = buf[j];
    dst[i] = '\0';
    dst[i+1] = '\0';
}

/* Creates Winamp's extension list, a single string that ends with \0\0.
 * Each extension must be in this format: "extension\0Description\0"
 * The list is used to accept extensions by default when IsOurFile returns 0, and to register file types.
 * It could be ignored/empty and just detect in IsOurFile instead. */
static void build_extension_list(char *winamp_list, int winamp_list_size) {
    const char ** ext_list;
    size_t ext_list_len;
    int i;

    winamp_list[0]='\0';
    winamp_list[1]='\0';

    ext_list = vgmstream_get_formats(&ext_list_len);

    for (i=0; i < ext_list_len; i++) {
        add_extension(winamp_list, winamp_list_size, ext_list[i]);
    }
}

/* unicode utils */
static void get_title(in_char* dst, int dst_size, const in_char* fn, VGMSTREAM* infostream) {
    in_char filename[PATH_LIMIT];
    char buffer[PATH_LIMIT];
    char filename_utf8[PATH_LIMIT];

    parse_fn_string(fn, NULL, filename,PATH_LIMIT);
    //parse_fn_int(fn, wa_L("$s"), &stream_index);

    wa_ichar_to_char(filename_utf8, PATH_LIMIT, filename);

    /* infostream gets added at first with index 0, then once played it re-adds proper numbers */
    if (infostream) {
        vgmstream_title_t tcfg = {0};
        int is_first = infostream->stream_index == 0;

        tcfg.force_title = settings.force_title;
        tcfg.subsong_range = is_first;
        tcfg.remove_extension = 1;

        vgmstream_get_title(buffer, sizeof(buffer), filename_utf8, infostream, &tcfg);

        wa_char_to_ichar(dst, dst_size, buffer);
    }
}

static void apply_config(VGMSTREAM* vgmstream, winamp_settings_t* settings) {
    vgmstream_cfg_t vcfg = {0};

    vcfg.allow_play_forever = 1;
    vcfg.play_forever = settings->loop_forever;
    vcfg.loop_count = settings->loop_count;
    vcfg.fade_time = settings->fade_time;
    vcfg.fade_delay = settings->fade_delay;
    vcfg.ignore_loop = settings->ignore_loop;

    vgmstream_apply_config(vgmstream, &vcfg);
}

static int winampGetExtendedFileInfo_common(in_char* filename, char *metadata, char* ret, int retlen);

static double get_album_gain_volume(const in_char *fn) {
    char replaygain[64];
    double gain = 0.0;
    int had_replaygain = 0;
    if (settings.gain_type == REPLAYGAIN_NONE)
        return 1.0;

    replaygain[0] = '\0'; /* reset each time to make sure we read actual tags */
    if (settings.gain_type == REPLAYGAIN_ALBUM
            && winampGetExtendedFileInfo_common((in_char *)fn, "replaygain_album_gain", replaygain, sizeof(replaygain))
            && replaygain[0] != '\0') {
        gain = atof(replaygain);
        had_replaygain = 1;
    }

    replaygain[0] = '\0';
    if (!had_replaygain
            && winampGetExtendedFileInfo_common((in_char *)fn, "replaygain_track_gain", replaygain, sizeof(replaygain))
            && replaygain[0] != '\0') {
        gain = atof(replaygain);
        had_replaygain = 1;
    }

    if (had_replaygain) {
        double vol = pow(10.0, gain / 20.0);
        double peak = 1.0;

        replaygain[0] = '\0';
        if (settings.clip_type == REPLAYGAIN_ALBUM
                && winampGetExtendedFileInfo_common((in_char *)fn, "replaygain_album_peak", replaygain, sizeof(replaygain))
                && replaygain[0] != '\0') {
            peak = atof(replaygain);
        }
        else if (settings.clip_type != REPLAYGAIN_NONE
                && winampGetExtendedFileInfo_common((in_char *)fn, "replaygain_track_peak", replaygain, sizeof(replaygain))
                && replaygain[0] != '\0') {
            peak = atof(replaygain);
        }
        return peak != 1.0 ? min(vol, 1.0 / peak) : vol;
    }

    return 1.0;
}


/* ***************************************** */
/* IN_VGMSTREAM                              */
/* ***************************************** */

/* about dialog */
void winamp_About(HWND hwndParent) {
    const char *ABOUT_TEXT =
            PLUGIN_DESCRIPTION "\n"
            "by hcs, FastElbja, manakoAT, bxaimc, snakemeat, soneek, kode54, bnnm and many others\n"
            "\n"
            "Winamp plugin by hcs, others\n"
            "\n"
            "https://github.com/kode54/vgmstream/\n"
            "https://sourceforge.net/projects/vgmstream/ (original)";

    {
        TCHAR buf[1024];
        size_t buf_size = 1024;

        cfg_char_to_wchar(buf, buf_size, ABOUT_TEXT);
        MessageBox(hwndParent, buf,TEXT("about in_vgmstream"),MB_OK);
    }
}

/* called at program init */
void winamp_Init() {

    settings.is_xmplay = is_xmplay();

    /* get ini config */
    load_defaults(&defaults);
    load_config(&settings, &defaults);

    /* XMPlay with in_vgmstream doesn't support most IPC_x messages so no playlist manipulation */
    if (settings.is_xmplay) {
        settings.disable_subsongs = 1;
    }

    /* dynamically make a list of supported extensions */
    build_extension_list(working_extension_list, EXTENSION_LIST_SIZE);
}

/* called at program quit */
void winamp_Quit() {
}

/* called before extension checks, to allow detection of mms://, etc */
int winamp_IsOurFile(const in_char *fn) {
    vgmstream_ctx_valid_cfg cfg = {0};
    char filename_utf8[PATH_LIMIT];

    wa_ichar_to_char(filename_utf8, PATH_LIMIT, fn);

    cfg.skip_standard = 1; /* validated by Winamp */
    cfg.accept_unknown = settings.exts_unknown_on;
    cfg.accept_common = settings.exts_common_on;

    /* Winamp seem to have bizarre handling of MP3 without standard names (ex song.mp3a),
     * in that it'll try to open normally, rejected if unknown_exts_on is not set, and
     * finally retry with "hi.mp3", accepted if exts_common_on is set. */

    /* returning 0 here means it only accepts the extensions in working_extension_list */
    return vgmstream_ctx_is_valid(filename_utf8, &cfg);
}


/* request to start playing a file */
int winamp_Play(const in_char *fn) {
    int max_latency;
    in_char filename[PATH_LIMIT];
    int stream_index = 0;

    /* shouldn't happen */
    if (vgmstream)
        return 1;

    /* check for info encoded in the filename */
    parse_fn_string(fn, NULL, filename,PATH_LIMIT);
    parse_fn_int(fn, wa_L("$s"), &stream_index);

    /* open the stream */
    vgmstream = init_vgmstream_winamp(filename,stream_index);
    if (!vgmstream)
        return 1;

    /* add N subsongs to the playlist, if any */
    if (split_subsongs(filename, stream_index, vgmstream)) {
        close_vgmstream(vgmstream);
        vgmstream = NULL;
        return 1;
    }

    /* config */
    apply_config(vgmstream, &settings);

    /* enable after all config but before outbuf (though ATM outbuf is not dynamic so no need to read input_channels) */
    vgmstream_mixing_autodownmix(vgmstream, settings.downmix_channels);
    vgmstream_mixing_enable(vgmstream, SAMPLE_BUFFER_SIZE, NULL /*&input_channels*/, &state.output_channels);

    /* reset internals */
    state.paused = 0;
    state.decode_abort = 0;
    state.seek_sample = -1;
    state.decode_pos_ms = 0;
    state.decode_pos_samples = 0;
    state.length_samples = vgmstream_get_samples(vgmstream);
    state.volume = get_album_gain_volume(fn);


    /* save original name */
    wa_strncpy(lastfn,fn,PATH_LIMIT);

    /* open the output plugin */
    max_latency = input_module.outMod->Open(vgmstream->sample_rate, state.output_channels, 16, 0, 0);
    if (max_latency < 0) {
        close_vgmstream(vgmstream);
        vgmstream = NULL;
        return 1;
    }

    /* set info display */
    input_module.SetInfo(get_vgmstream_average_bitrate(vgmstream) / 1000, vgmstream->sample_rate / 1000, state.output_channels, 1);

    /* setup visualization */
    input_module.SAVSAInit(max_latency,vgmstream->sample_rate);
    input_module.VSASetInfo(vgmstream->sample_rate, state.output_channels);

    /* start */
    decode_thread_handle = CreateThread(
            NULL,   /* handle cannot be inherited */
            0,      /* stack size, 0=default */
            decode, /* thread start routine */
            NULL,   /* no parameter to start routine */
            0,      /* run thread immediately */
            NULL);  /* don't keep track of the thread id */

    SetThreadPriority(decode_thread_handle, priority_values[settings.thread_priority]);

    return 0; /* success */
}

/* pause stream */
void winamp_Pause() {
    state.paused = 1;
    input_module.outMod->Pause(1);
}

/* unpause stream */
void winamp_UnPause() {
    state.paused = 0;
    input_module.outMod->Pause(0);
}

/* return 1 if paused, 0 if not */
int winamp_IsPaused() {
    return state.paused;
}

/* stop (unload) stream */
void winamp_Stop() {

    if (decode_thread_handle != INVALID_HANDLE_VALUE) {
        state.decode_abort = 1;

        /* arbitrary wait milliseconds (error can trigger if the system is *really* busy) */
        if (WaitForSingleObject(decode_thread_handle, 5000) == WAIT_TIMEOUT) {
            MessageBox(input_module.hMainWindow, TEXT("Error stopping decode thread\n"), ("Error"),MB_OK|MB_ICONERROR);
            TerminateThread(decode_thread_handle, 0);
        }
        CloseHandle(decode_thread_handle);
        decode_thread_handle = INVALID_HANDLE_VALUE;
    }


    close_vgmstream(vgmstream);
    vgmstream = NULL;

    input_module.outMod->Close();
    input_module.SAVSADeInit();
}

/* get length in ms */
int winamp_GetLength() {
    if (!vgmstream)
        return 0;
    return state.length_samples * 1000LL / vgmstream->sample_rate;
}

/* current output time in ms */
int winamp_GetOutputTime() {
    int32_t pos_ms = state.decode_pos_ms;
    /* for some reason this gets triggered hundred of times by non-classic skins when using subsongs */
    if (!vgmstream)
        return 0;

    /* pretend we have reached destination if called while seeking is on */
    if (state.seek_sample >= 0)
        pos_ms = state.seek_sample * 1000LL / vgmstream->sample_rate;

    return pos_ms + (input_module.outMod->GetOutputTime() - input_module.outMod->GetWrittenTime());
}

/* seeks to point in stream (in ms) */
void winamp_SetOutputTime(int time_in_ms) {
    if (!vgmstream)
        return;
    state.seek_sample = (long long)time_in_ms * vgmstream->sample_rate / 1000LL;
}

/* pass these commands through */
void winamp_SetVolume(int volume) {
    input_module.outMod->SetVolume(volume);
}
void winamp_SetPan(int pan) {
    input_module.outMod->SetPan(pan);
}

/* display info box (ALT+3) */
int winamp_InfoBox(const in_char *fn, HWND hwnd) {
    char description[1024] = {0}, tmp[1024] = {0};
    size_t description_size = 1024;
    double tmpVolume = 1.0;

    concatn(description_size,description,PLUGIN_DESCRIPTION "\n\n");

    if (!fn || !*fn) {
        /* no filename = current playing file */
        if (!vgmstream)
            return 0;

        describe_vgmstream(vgmstream,description,description_size);
    }
    else {
        /* some other file in playlist given by filename */
        VGMSTREAM* infostream = NULL;
        in_char filename[PATH_LIMIT];
        int stream_index = 0;

        /* check for info encoded in the filename */
        parse_fn_string(fn, NULL, filename,PATH_LIMIT);
        parse_fn_int(fn, wa_L("$s"), &stream_index);

        infostream = init_vgmstream_winamp(filename, stream_index);
        if (!infostream) return 0;

        apply_config(infostream, &settings);

        vgmstream_mixing_autodownmix(infostream, settings.downmix_channels);
        vgmstream_mixing_enable(infostream, 0, NULL, NULL);

        describe_vgmstream(infostream,description,description_size);

        close_vgmstream(infostream);
        infostream = NULL;
        tmpVolume = get_album_gain_volume(fn);
    }


    {
        TCHAR buf[1024] = {0};
        size_t buf_size = 1024;

        snprintf(tmp, sizeof(tmp), "\nvolume: %.6f", tmpVolume);
        concatn(description_size, description, tmp);

        cfg_char_to_wchar(buf, buf_size, description);
        MessageBox(hwnd,buf,TEXT("Stream info"),MB_OK);
    }
    return 0;
}

/* retrieve title (playlist name) and time on the current or other file in the playlist */
void winamp_GetFileInfo(const in_char *fn, in_char *title, int *length_in_ms) {

    if (!fn || !*fn) {
        /* no filename = current playing file */

        if (!vgmstream)
            return;

        if (title) {
            get_title(title,GETFILEINFO_TITLE_LENGTH, lastfn, vgmstream);
        }

        if (length_in_ms) {
            *length_in_ms = winamp_GetLength();
        }
    }
    else {
        /* some other file in playlist given by filename */
        VGMSTREAM* infostream = NULL;
        in_char filename[PATH_LIMIT];
        int stream_index = 0;

        /* check for info encoded in the filename */
        parse_fn_string(fn, NULL, filename,PATH_LIMIT);
        parse_fn_int(fn, wa_L("$s"), &stream_index);

        infostream = init_vgmstream_winamp(filename, stream_index);
        if (!infostream) return;

        apply_config(infostream, &settings);

        vgmstream_mixing_autodownmix(infostream, settings.downmix_channels);
        vgmstream_mixing_enable(infostream, 0, NULL, NULL);

        if (title) {
            get_title(title,GETFILEINFO_TITLE_LENGTH, fn, infostream);
        }

        if (length_in_ms) {
            *length_in_ms = -1000;
            if (infostream) {
                const int num_samples = vgmstream_get_samples(infostream);
                *length_in_ms = num_samples * 1000LL /infostream->sample_rate;
            }
        }

        close_vgmstream(infostream);
        infostream = NULL;
    }
}

/* eq stuff */
void winamp_EQSet(int on, char data[10], int preamp) {
}

/*****************************************************************************/
/* MAIN DECODE (some used in extended part too, so avoid globals) */

static void do_seek(winamp_state_t* state, VGMSTREAM* vgmstream) {
    int play_forever = vgmstream_get_play_forever(vgmstream);
    int seek_sample = state->seek_sample;  /* local due to threads/race conditions changing state->seek_sample elsewhere */

    /* ignore seeking past file, can happen using the right (->) key, ok if playing forever */
    if (state->seek_sample > state->length_samples && !play_forever) {
        state->seek_sample = -1;
        //state->seek_sample = state->length_samples;
        //seek_sample = state->length_samples;

        state->decode_pos_samples = state->length_samples;
        state->decode_pos_ms = state->decode_pos_samples * 1000LL / vgmstream->sample_rate;
        return;
    }

    /* could divide in N seeks (from pos) for slower files so cursor moves, but doesn't seem too necessary */
    seek_vgmstream(vgmstream, seek_sample);

    state->decode_pos_samples = seek_sample;
    state->decode_pos_ms = state->decode_pos_samples * 1000LL / vgmstream->sample_rate;

    /* different sample: other seek may have been requested during seek_vgmstream */
    if (state->seek_sample == seek_sample)
        state->seek_sample = -1;
}

static void apply_gain(winamp_state_t* state, int samples_to_do) {

    /* apply ReplayGain, if needed */
    if (state->volume != 1.0) {
        int j, k;
        int channels = state->output_channels;

        for (j = 0; j < samples_to_do; j++) {
            for (k = 0; k < channels; k++) {
                sample_buffer[j*channels + k] = (short)(sample_buffer[j*channels + k] * state->volume);
            }
        }
    }
}

/* the decode thread */
DWORD WINAPI __stdcall decode(void *arg) {
    const int max_buffer_samples = SAMPLE_BUFFER_SIZE;
    int play_forever = vgmstream_get_play_forever(vgmstream);

    while (!state.decode_abort) {
        int samples_to_do;
        int output_bytes;

        if (state.decode_pos_samples + max_buffer_samples > state.length_samples && !play_forever) {
            samples_to_do = state.length_samples - state.decode_pos_samples;
            if (samples_to_do < 0) /* just in case */
                samples_to_do = 0;
        }
        else {
            samples_to_do = max_buffer_samples;
        }

        output_bytes = (samples_to_do * state.output_channels * sizeof(short));
        if (input_module.dsp_isactive())
            output_bytes = output_bytes * 2; /* Winamp's DSP may need double samples */

        if (samples_to_do == 0 && state.seek_sample < 0) { /* track finished and not seeking */
            input_module.outMod->CanWrite();    /* ? */
            if (!input_module.outMod->IsPlaying()) {
                PostMessage(input_module.hMainWindow, WM_WA_MPEG_EOF, 0,0); /* end */
                return 0;
            }
            Sleep(10);
        }
        else if (state.seek_sample >= 0) { /* seek */
            do_seek(&state, vgmstream);

            /* flush Winamp buffers *after* fully seeking (allows to play buffered samples while we seek, feels a bit snappier) */
            if (state.seek_sample < 0)
                input_module.outMod->Flush(state.decode_pos_ms);
        }
        else if (input_module.outMod->CanWrite() >= output_bytes) { /* decode */
            render_vgmstream(sample_buffer, samples_to_do, vgmstream);

            apply_gain(&state, samples_to_do); /* apply ReplayGain, if needed */

            /* output samples */
            input_module.SAAddPCMData((char*)sample_buffer, state.output_channels, 16, state.decode_pos_ms);
            input_module.VSAAddPCMData((char*)sample_buffer, state.output_channels, 16, state.decode_pos_ms);

            if (input_module.dsp_isactive()) { /* find out DSP's needs */
                int dsp_output_samples = input_module.dsp_dosamples(sample_buffer, samples_to_do, 16, state.output_channels, vgmstream->sample_rate);
                output_bytes = dsp_output_samples * state.output_channels * sizeof(short);
            }

            input_module.outMod->Write((char*)sample_buffer, output_bytes);

            state.decode_pos_samples += samples_to_do;
            state.decode_pos_ms = state.decode_pos_samples * 1000LL / vgmstream->sample_rate;
        }
        else { /* can't write right now */
            Sleep(20);
        }
    }

    return 0;
}

/* configuration dialog */
void winamp_Config(HWND hwndParent) {
    /* open dialog defined in resource.rc */
    DialogBox(input_module.hDllInstance, (const TCHAR *)IDD_CONFIG, hwndParent, configDlgProc);
}

/* *********************************** */

/* main plugin def */
In_Module input_module = {
    IN_VER,
    PLUGIN_DESCRIPTION,
    0,  /* hMainWindow (filled in by Winamp) */
    0,  /* hDllInstance (filled in by Winamp) */
    working_extension_list,
    1, /* is_seekable flag  */
    9, /* UsesOutputPlug flag */
    winamp_Config,
    winamp_About,
    winamp_Init,
    winamp_Quit,
    winamp_GetFileInfo,
    winamp_InfoBox,
    winamp_IsOurFile,
    winamp_Play,
    winamp_Pause,
    winamp_UnPause,
    winamp_IsPaused,
    winamp_Stop,
    winamp_GetLength,
    winamp_GetOutputTime,
    winamp_SetOutputTime,
    winamp_SetVolume,
    winamp_SetPan,
    0,0,0,0,0,0,0,0,0, /* vis stuff */
    0,0, /* dsp stuff */
    winamp_EQSet,
    NULL, /* SetInfo */
    0 /* outMod */
};

__declspec(dllexport) In_Module * winampGetInModule2() {
    return &input_module;
}


/* ************************************* */
/* IN_TAGS                               */
/* ************************************* */

/* could malloc and stuff but totals aren't much bigger than PATH_LIMITs anyway */
#define WINAMP_TAGS_ENTRY_MAX      30
#define WINAMP_TAGS_ENTRY_SIZE     2048

typedef struct {
    int loaded;
    in_char filename[PATH_LIMIT]; /* tags are loaded for this file */
    int tag_count;

    char keys[WINAMP_TAGS_ENTRY_MAX][WINAMP_TAGS_ENTRY_SIZE+1];
    char vals[WINAMP_TAGS_ENTRY_MAX][WINAMP_TAGS_ENTRY_SIZE+1];
} winamp_tags;

winamp_tags last_tags;


/* Loads all tags for a filename in a temp struct to improve performance, as
 * Winamp requests one tag at a time and may reask for the same tag several times */
static void load_tagfile_info(in_char* filename) {
    STREAMFILE *tagFile = NULL;
    in_char filename_clean[PATH_LIMIT];
    char filename_utf8[PATH_LIMIT];
    char tagfile_path_utf8[PATH_LIMIT];
    in_char tagfile_path_i[PATH_LIMIT];
    char *path;


    if (settings.tagfile_disable) { /* reset values if setting changes during play */
        last_tags.loaded = 0;
        last_tags.tag_count = 0;
        return;
    }

    /* clean extra part for subsong tags */
    parse_fn_string(filename, NULL, filename_clean,PATH_LIMIT);

    if (wa_strcmp(last_tags.filename, filename_clean) == 0) {
        return; /* not changed, tags still apply */
    }

    last_tags.loaded = 0;

    /* tags are now for this filename, find tagfile path */
    wa_ichar_to_char(filename_utf8, PATH_LIMIT, filename_clean);
    strcpy(tagfile_path_utf8,filename_utf8);

    path = strrchr(tagfile_path_utf8,'\\');
    if (path != NULL) {
        path[1] = '\0'; /* includes "\", remove after that from tagfile_path */
        strcat(tagfile_path_utf8,tagfile_name);
    }
    else { /* ??? */
        strcpy(tagfile_path_utf8,tagfile_name);
    }
    wa_char_to_ichar(tagfile_path_i, PATH_LIMIT, tagfile_path_utf8);

    wa_strcpy(last_tags.filename, filename_clean);
    last_tags.tag_count = 0;

    /* load all tags from tagfile */
    tagFile = open_winamp_streamfile_by_ipath(tagfile_path_i);
    if (tagFile != NULL) {
        VGMSTREAM_TAGS* tags;
        const char *tag_key, *tag_val;
        int i;

        tags = vgmstream_tags_init(&tag_key, &tag_val);
        vgmstream_tags_reset(tags, filename_utf8);
        while (vgmstream_tags_next_tag(tags, tagFile)) {
            int repeated_tag = 0;
            int current_tag = last_tags.tag_count;
            if (current_tag >= WINAMP_TAGS_ENTRY_MAX)
                continue;

            /* should overwrite repeated tags as global tags may appear multiple times */
            for (i = 0; i < current_tag; i++) {
                if (strcmp(last_tags.keys[i], tag_key) == 0) {
                    current_tag = i;
                    repeated_tag = 1;
                    break;
                }
            }

            last_tags.keys[current_tag][0] = '\0';
            strncat(last_tags.keys[current_tag], tag_key, WINAMP_TAGS_ENTRY_SIZE);
            last_tags.vals[current_tag][0] = '\0';
            strncat(last_tags.vals[current_tag], tag_val, WINAMP_TAGS_ENTRY_SIZE);
            if (!repeated_tag)
                last_tags.tag_count++;
        }

        vgmstream_tags_close(tags);
        close_streamfile(tagFile);
        last_tags.loaded = 1;
    }
}

/* Winamp repeatedly calls this for every known tag currently used in the Advanced Title Formatting (ATF)
 * config, 'metadata' being the requested tag. Returns 0 on failure/tag not found.
 * May be called again after certain actions (adding file to playlist, Play, GetFileInfo, etc), and
 * doesn't seem the plugin can tell Winamp all tags it supports at once or use custom tags. */
//todo unicode stuff could be improved... probably
static int winampGetExtendedFileInfo_common(in_char* filename, char *metadata, char* ret, int retlen) {
    int i, tag_found;
    int max_len;

    /* load list current tags, if necessary */
    load_tagfile_info(filename);
    if (!last_tags.loaded) /* tagfile not found, fail so default get_title takes over */
        goto fail;

    /* always called (value in ms), must return ok so other tags get called */
    if (strcasecmp(metadata, "length") == 0) {
        strcpy(ret, "0");//todo should export but shows GetFileInfo's ms if not provided
        return 1;
    }

#if 0
    /* special case to fill WA5's unified dialog */
    if (strcasecmp(metadata, "formatinformation") == 0) {
        generate_format_string(...);
    }
#endif


    /* find requested tag */
    tag_found = 0;
    max_len = (retlen > 0) ? retlen-1 : retlen;
    for (i = 0; i < last_tags.tag_count; i++) {
        if (strcasecmp(metadata,last_tags.keys[i]) == 0) {
            ret[0] = '\0';
            strncat(ret, last_tags.vals[i], max_len);
            tag_found = 1;
            break;
        }
    }

    /* if tagfile exists but TITLE doesn't Winamp won't default to GetFileInfo, so call it
     * manually as it's useful for files with stream names */
    if (!tag_found && strcasecmp(metadata, "title") == 0) {
        in_char ret_wchar[2048];

        winamp_GetFileInfo(filename, ret_wchar, NULL);
        wa_ichar_to_char(ret, retlen, ret_wchar);
        return 1;
    }

    if (!tag_found)
        goto fail;

    return 1;

fail:
    //TODO: is this always needed for Winamp to use replaygain?
    //strcpy(ret, "1.0"); //should set some default value?
    return strcasecmp(metadata, "replaygain_track_gain") == 0 ? 1 : 0;
}


/* for Winamp 5.24 */
__declspec (dllexport) int winampGetExtendedFileInfo(char *filename, char *metadata, char *ret, int retlen) {
    in_char filename_wchar[PATH_LIMIT];
    int ok;

    if (settings.tagfile_disable)
        return 0;

    wa_char_to_ichar(filename_wchar,PATH_LIMIT, filename);

    ok = winampGetExtendedFileInfo_common(filename_wchar, metadata, ret, retlen);
    if (ok == 0)
        return 0;

    return 1;
}

/* for Winamp 5.3+ */
__declspec (dllexport) int winampGetExtendedFileInfoW(wchar_t *filename, char *metadata, wchar_t *ret, int retlen) {
    in_char filename_ichar[PATH_LIMIT];
    char ret_utf8[2048];
    int ok;

    if (settings.tagfile_disable)
        return 0;

    wa_wchar_to_ichar(filename_ichar,PATH_LIMIT, filename);

    ok = winampGetExtendedFileInfo_common(filename_ichar, metadata, ret_utf8,2048);
    if (ok == 0)
        return 0;

    wa_char_to_wchar(ret,retlen, ret_utf8);

    return 1;
}

/* return 1 if you want winamp to show it's own file info dialogue, 0 if you want to show your own (via In_Module.InfoBox)
 * if returning 1, remember to implement winampGetExtendedFileInfo("formatinformation")! */
__declspec(dllexport) int winampUseUnifiedFileInfoDlg(const wchar_t * fn) {
    return 0;
}

__declspec(dllexport) int winampUninstallPlugin(HINSTANCE hDllInst, HWND hwndDlg, int param) {
    /* may uninstall without restart as we aren't subclassing */
    return IN_PLUGIN_UNINSTALL_NOW;
}

/* ************************************* */
/* EXTENDED DECODE                       */
/* ************************************* */

//TODO: call common functions to avoid so much repeated code
/* the following functions are used for ReplayGain and transcoding, in places like the media
 * library or CD burner, if implemented. In usual Winamp fashion they are messy, barely
 * documented, slightly different repeats of the above. */

winamp_state_t xstate;
short xsample_buffer[SAMPLE_BUFFER_SIZE*2 * VGMSTREAM_MAX_CHANNELS];


/* open the file and prepares to decode */
static void *winampGetExtendedRead_open_common(in_char *fn, int *size, int *bps, int *nch, int *srate) {
    VGMSTREAM *xvgmstream = NULL;
    in_char filename[PATH_LIMIT];
    int stream_index = 0;

    /* check for info encoded in the filename */
    parse_fn_string(fn, NULL, filename, PATH_LIMIT);
    parse_fn_int(fn, wa_L("$s"), &stream_index);

    /* open the stream */
    xvgmstream = init_vgmstream_winamp(filename, stream_index);
    if (!xvgmstream) {
        return NULL;
    }

    /* config */
    apply_config(xvgmstream, &settings);

    /* enable after all config but before outbuf (though ATM outbuf is not dynamic so no need to read input_channels) */
    vgmstream_mixing_autodownmix(xvgmstream, settings.downmix_channels);
    vgmstream_mixing_enable(xvgmstream, SAMPLE_BUFFER_SIZE, NULL /*&input_channels*/, &xstate.output_channels);

    /* reset internals */
    xstate.paused = 0; /* unused */
    xstate.decode_abort = 0; /* unused */
    xstate.seek_sample = -1;
    xstate.decode_pos_ms = 0; /* unused */
    xstate.decode_pos_samples = 0;
    xstate.length_samples = vgmstream_get_samples(xvgmstream);
    xstate.volume = 1.0; /* unused */

    if (size) /* bytes to decode (-1 if unknown) */
        *size = xstate.length_samples * xstate.output_channels * sizeof(short);
    if (bps)
        *bps = 16;
    if (nch)
        *nch = xstate.output_channels;
    if (srate)
        *srate = xvgmstream->sample_rate;

    return xvgmstream; /* handle passed to other extended functions */
}

__declspec(dllexport) void *winampGetExtendedRead_open(const char *fn, int *size, int *bps, int *nch, int *srate) {
    in_char filename_wchar[PATH_LIMIT];

    wa_char_to_ichar(filename_wchar, PATH_LIMIT, fn);

    return winampGetExtendedRead_open_common(filename_wchar, size, bps, nch, srate);
}

__declspec(dllexport) void *winampGetExtendedRead_openW(const wchar_t *fn, int *size, int *bps, int *nch, int *srate) {
    in_char filename_ichar[PATH_LIMIT];

    wa_wchar_to_ichar(filename_ichar, PATH_LIMIT, fn);

    return winampGetExtendedRead_open_common(filename_ichar, size, bps, nch, srate);
}

/* decode len to dest buffer, called multiple times until file done or decoding is aborted */
__declspec(dllexport) size_t winampGetExtendedRead_getData(void *handle, char *dest, size_t len, int *killswitch) {
    const int max_buffer_samples = SAMPLE_BUFFER_SIZE;
    unsigned copied = 0;
    int done = 0;
    VGMSTREAM* xvgmstream = handle;
    int play_forever;
    if (!xvgmstream)
        return 0;

    play_forever = vgmstream_get_play_forever(xvgmstream);

    while (copied + (max_buffer_samples * xvgmstream->channels * sizeof(short)) < len && !done) {
        int samples_to_do;

        if (xstate.decode_pos_samples + max_buffer_samples > xstate.length_samples && !play_forever) {
            samples_to_do = xstate.length_samples - xstate.decode_pos_samples;
            if (samples_to_do < 0) /* just in case */
                samples_to_do = 0;
        }
        else {
            samples_to_do = max_buffer_samples;
        }

        if (!samples_to_do) { /* track finished */
            break;
        }
        else if (xstate.seek_sample != -1) { /* seek */
            do_seek(&xstate, xvgmstream);
        }
        else { /* decode */
            render_vgmstream(xsample_buffer, samples_to_do, xvgmstream);

            /* output samples */
            memcpy(&dest[copied], xsample_buffer, samples_to_do * xstate.output_channels * sizeof(short));
            copied += samples_to_do * xstate.output_channels * sizeof(short);

            xstate.decode_pos_samples += samples_to_do;
        }

        /* check decoding cancelled */
        if (killswitch && *killswitch) {
            break;
        }
    }

    return copied; /* return 0 to signal file done */
}

/* seek in the file (possibly unused) */
__declspec(dllexport) int winampGetExtendedRead_setTime(void *handle, int time_in_ms) {
    VGMSTREAM *xvgmstream = handle;
    if (xvgmstream) {
        xstate.seek_sample = (long long)time_in_ms * xvgmstream->sample_rate / 1000LL;
        return 1;
    }
    return 0;
}

/* file done */
__declspec(dllexport) void winampGetExtendedRead_close(void *handle) {
    VGMSTREAM *xvgmstream = handle;
    if (xvgmstream) {
        close_vgmstream(xvgmstream);
    }
}

/* other winamp sekrit exports: */
#if 0
__declspec(dllexport) void winampAddUnifiedFileInfoPane(?) {
    ?
}
#endif
