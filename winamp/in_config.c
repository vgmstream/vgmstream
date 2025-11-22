/**
 * config for Winamp
 */
#include "in_vgmstream.h"

#define CFG_DBL_BUF_SIZE 128
#define CFG_INT_BUF_SIZE 32
#define CFG_LOG_LINES 32
#define CFG_LOG_LINE_SIZE 256
#define CFG_LOG_BUF_SIZE ((CFG_LOG_LINE_SIZE + 1) * CFG_LOG_LINES)

/* config */
#define CONFIG_APP_NAME         TEXT("vgmstream plugin")
#define CONFIG_INI_NAME         TEXT("plugin.ini")

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

TCHAR* dlg_priority_strings[7] = {
    TEXT("Idle"),
    TEXT("Lowest"),
    TEXT("Below Normal"),
    TEXT("Normal"),
    TEXT("Above Normal"),
    TEXT("Highest (not recommended)"),
    TEXT("Time Critical (not recommended)")
};

TCHAR* dlg_replaygain_strings[] = {
    TEXT("None"),
    TEXT("Album"),
    TEXT("Peak")
};

const int priority_values[7] = {
    THREAD_PRIORITY_IDLE,
    THREAD_PRIORITY_LOWEST,
    THREAD_PRIORITY_BELOW_NORMAL,
    THREAD_PRIORITY_NORMAL,
    THREAD_PRIORITY_ABOVE_NORMAL,
    THREAD_PRIORITY_HIGHEST,
    THREAD_PRIORITY_TIME_CRITICAL
};

// hidden messages added in later SDKs
#define WA_IPC_GETINIFILEW 1334
#define WA_IPC_GETINIDIRECTORYW 1335
#define WA_IPC_GETPLUGINDIRECTORYW 1336

/* Winamp INI reader */
static void ini_get_filename(In_Module* input_module, TCHAR* inifile, int inifile_len) {

    if (IsWindow(input_module->hMainWindow) && SendMessage(input_module->hMainWindow, WM_WA_IPC,0,IPC_GETVERSION) >= 0x5000) {
        // newer Winamp with per-user settings

        TCHAR* ini_dir = (TCHAR*)SendMessage(input_module->hMainWindow, WM_WA_IPC, 0, WA_IPC_GETINIDIRECTORYW);
        if (ini_dir) {
            // only "newer" winamp versions, probably helps for unicode paths/usernames
            cfg_strncpy(inifile, ini_dir, WINAMP_PATH_LIMIT);
        } else {
            // works with standard cases (should be compatible with the above)
            char* ini_dir = (char*)SendMessage(input_module->hMainWindow, WM_WA_IPC, 0, IPC_GETINIDIRECTORY);
            cfg_char_to_wchar(inifile, inifile_len, ini_dir);
        }

        cfg_strncat(inifile, TEXT("\\Plugins\\"), inifile_len);

        // can't be certain that \Plugins already exists in the user dir
        CreateDirectory(inifile, NULL);

        cfg_strncat(inifile, CONFIG_INI_NAME, inifile_len);
    }
    else {
        // older winamp with single settings
        TCHAR* lastSlash;

        GetModuleFileName(NULL, inifile, inifile_len);
        lastSlash = cfg_strrchr(inifile, TEXT('\\'));

        *(lastSlash + 1) = 0;

        /* XMPlay doesn't have a "plugins" subfolder */
        if (settings.is_xmplay) {
            cfg_strncat(inifile, CONFIG_INI_NAME, inifile_len);
        }
        else {
            cfg_strncat(inifile, TEXT("Plugins\\") CONFIG_INI_NAME, inifile_len);
        }

        /* Maybe should query IPC_GETINIDIRECTORY and use that, not sure what ancient Winamps need.
         * There must be some proper way to handle dirs since other Winamp plugins save config in 
         * XMPlay correctly (this feels like archaeology, try later) */
    }
}


static void ini_get_d(TCHAR* inifile, TCHAR* entry, double defval, double* p_val) {
    TCHAR buf[CFG_DBL_BUF_SIZE];
    TCHAR defbuf[CFG_DBL_BUF_SIZE];
    int consumed, res;

    cfg_sprintf(defbuf, TEXT("%.2lf"), defval);
    GetPrivateProfileString(CONFIG_APP_NAME, entry, defbuf, buf, CFG_DBL_BUF_SIZE, inifile);
    res = cfg_sscanf(buf, TEXT("%lf%n"), p_val, &consumed);
    if (res < 1 || consumed != cfg_strlen(buf) || *p_val < 0) {
        *p_val = defval;
    }
}

static void ini_get_i(TCHAR* inifile, TCHAR* entry, int defval, int* p_val, int min, int max) {
    *p_val = GetPrivateProfileInt(CONFIG_APP_NAME, entry, defval, inifile);
    if (*p_val < min || *p_val > max) {
        *p_val = defval;
    }
}

static void ini_get_b(TCHAR* inifile, TCHAR* entry, int defval, int* p_val) {
    ini_get_i(inifile, entry, defval, p_val, 0, 1);
}

static void ini_set_d(TCHAR* inifile, TCHAR* entry, double val) {
    TCHAR buf[CFG_DBL_BUF_SIZE];
    cfg_sprintf(buf, TEXT("%.2lf"), val);
    WritePrivateProfileString(CONFIG_APP_NAME, entry, buf, inifile);
}

static void ini_set_i(TCHAR* inifile, TCHAR* entry, int val) {
    TCHAR buf[CFG_INT_BUF_SIZE];
    cfg_sprintf(buf, TEXT("%d"), val);
    WritePrivateProfileString(CONFIG_APP_NAME, entry, buf, inifile);
}

static void ini_set_b(TCHAR* inifile, TCHAR* entry, int val) {
    ini_set_i(inifile, entry, val);
}

void load_defaults(winamp_settings_t* defaults) {
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

void load_config(In_Module* input_module, winamp_settings_t* settings, winamp_settings_t* defaults) {
    TCHAR inifile[WINAMP_PATH_LIMIT];

    ini_get_filename(input_module, inifile, WINAMP_PATH_LIMIT);

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

    /* exact 0 was allowed before (AKA "intro only") but confuses people and may result in unplayable files */
    if (settings->loop_count <= 0)
        settings->loop_count = 1;
}

static void save_config(In_Module* input_module, winamp_settings_t* settings) {
    TCHAR inifile[WINAMP_PATH_LIMIT];

    ini_get_filename(input_module, inifile, WINAMP_PATH_LIMIT);

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
    TCHAR buf[CFG_DBL_BUF_SIZE];
    cfg_sprintf(buf, TEXT("%.2lf"), val);
    SetDlgItemText(hDlg, idc, buf);
}

static void dlg_input_set_i(HWND hDlg, int idc, int val) {
    TCHAR buf[CFG_INT_BUF_SIZE];
    cfg_sprintf(buf, TEXT("%d"), val);
    SetDlgItemText(hDlg, idc, buf);
}

static void dlg_check_set(HWND hDlg, int idc, int val) {
    CheckDlgButton(hDlg, idc, val ? BST_CHECKED : BST_UNCHECKED);
}

static void cfg_combo_set(HWND hDlg, int idc, int val, TCHAR** list, int list_size) {
    int i;
    HANDLE hCombo = GetDlgItem(hDlg, idc);
    for (i = 0; i < list_size; i++) {
        SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)list[i]);
    }
    SendMessage(hCombo, CB_SETCURSEL, val, 0);
}
static void cfg_slider_set(HWND hDlg, int idc1, int idc2, int val, int min, int max, TCHAR** list) {
    HANDLE hSlider = GetDlgItem(hDlg, idc1);
    SendMessage(hSlider, TBM_SETRANGE, (WPARAM)TRUE, (LPARAM)MAKELONG(min, max));
    SendMessage(hSlider, TBM_SETPOS,   (WPARAM)TRUE, (LPARAM)val+1);
    SetDlgItemText(hDlg, idc2, list[val]);
}

static void dlg_input_get_d(HWND hDlg, int idc, double* p_val, LPCTSTR error, int* p_err) {
    TCHAR buf[CFG_DBL_BUF_SIZE];
    int res, consumed;
    double defval = *p_val;

    GetDlgItemText(hDlg, idc, buf, CFG_DBL_BUF_SIZE);
    res = cfg_sscanf(buf, TEXT("%lf%n"), p_val, &consumed);
    if (res < 1 || consumed != cfg_strlen(buf) || *p_val < 0) {
        MessageBox(hDlg, error, NULL, MB_OK|MB_ICONERROR);
        *p_val = defval;
        *p_err = 1;
    }
}

static void dlg_input_get_i(HWND hDlg, int idc, int* p_val, LPCTSTR error, int* p_err) {
    TCHAR buf[CFG_INT_BUF_SIZE];
    int res, consumed;
    int defval = *p_val;

    GetDlgItemText(hDlg, idc, buf, CFG_INT_BUF_SIZE);
    res = cfg_sscanf(buf, TEXT("%d%n"), p_val, &consumed);
    if (res < 1 || consumed != cfg_strlen(buf) || *p_val < 0) {
        MessageBox(hDlg, error, NULL, MB_OK|MB_ICONERROR);
        *p_val = defval;
        *p_err = 1;
    }
}

static void dlg_check_get(HWND hDlg, int idc, int* p_val) {
    *p_val = (IsDlgButtonChecked(hDlg, idc) == BST_CHECKED);
}

static void dlg_combo_get(HWND hDlg, int idc, int* p_val) {
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

    /* exact 0 was allowed before (AKA "intro only") but confuses people and may result in unplayable files */
    if (settings->loop_count <= 0)
        settings->loop_count = 1;

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
    /* globals to get them a bit controlled */
    winamp_settings_t* settings_ext = &settings;
    In_Module* input_module_ext = &input_module;

    switch (uMsg) {
        case WM_CLOSE: /* hide dialog */
            EndDialog(hDlg,TRUE);
            return TRUE;

        case WM_INITDIALOG: /* open dialog: load form with current settings */
            priority = settings_ext->thread_priority;
            dlg_save_form(hDlg, settings_ext, 0);
            break;

        case WM_COMMAND: /* button presses */
            switch (GET_WM_COMMAND_ID(wParam, lParam)) {
                case IDOK: { /* read and verify new values, save and close */
                    int ok;

                    settings_ext->thread_priority = priority;
                    ok = dlg_load_form(hDlg, settings_ext);
                    if (!ok) break; /* this leaves values changed though */

                    save_config(input_module_ext, settings_ext);

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

                case IDC_LOG_BUTTON: { /* shows log */
                    char buf_tmp[CFG_LOG_BUF_SIZE];
                    TCHAR buf[CFG_LOG_BUF_SIZE];
                    size_t buf_size = CFG_LOG_BUF_SIZE;
                    int max = 0;
                    const char** lines = logger_get_lines(&max);

                    /* could use some nice scrollable text but I don't know arcane Windows crap */
                    if (lines == NULL) {
                        snprintf(buf_tmp, CFG_LOG_LINE_SIZE, "%s\n", "couldn't read log");
                    }
                    else if (max == 0) {
                        snprintf(buf_tmp, CFG_LOG_LINE_SIZE, "%s\n", "(empty)");
                    }
                    else {
                        //TODO: improve
                        char* tmp = buf_tmp;
                        for (int i = 0; i < max; i++) {
                            int done = snprintf(tmp, CFG_LOG_LINE_SIZE, "%s", lines[i]);
                            if (done < 0 || done >= CFG_LOG_LINE_SIZE)
                                break;
                            tmp += (done); // + 1
                        }
                    }

                    cfg_char_to_wchar(buf, buf_size, buf_tmp);
                    MessageBox(hDlg, buf, TEXT("vgmstream log"), MB_OK);
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
