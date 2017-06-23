/**
 * vgmstream for Winamp
 */

/* Winamp uses wchar_t filenames when this is on, so extra steps are needed.
 * To open unicode filenames it needs to use _wfopen, inside a WA_STREAMFILE to pass around */
//#define UNICODE_INPUT_PLUGIN

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

#include "../src/formats.h"
#include "../src/vgmstream.h"
#include "in2.h"
#include "wa_ipc.h"
#include "resource.h"


#ifndef VERSION
#include "../version.h"
#endif
#ifndef VERSION
#define VERSION "(unknown version)"
#endif

#define APP_NAME "vgmstream plugin"
#define PLUGIN_DESCRIPTION "vgmstream plugin " VERSION " " __DATE__
#define INI_NAME "plugin.ini"


/* post when playback stops */
#define WM_WA_MPEG_EOF WM_USER+2

In_Module input_module; /* the input module, declared at the bottom of this file */
DWORD WINAPI __stdcall decode(void *arg);


#define DEFAULT_FADE_SECONDS "10.00"
#define DEFAULT_FADE_DELAY_SECONDS "0.00"
#define DEFAULT_LOOP_COUNT "2.00"
#define DEFAULT_THREAD_PRIORITY 3
#define DEFAULT_LOOP_FOREVER 0
#define DEFAULT_IGNORE_LOOP 0

#define FADE_SECONDS_INI_ENTRY "fade_seconds"
#define FADE_DELAY_SECONDS_INI_ENTRY "fade_delay"
#define LOOP_COUNT_INI_ENTRY "loop_count"
#define THREAD_PRIORITY_INI_ENTRY "thread_priority"
#define LOOP_FOREVER_INI_ENTRY "loop_forever"
#define IGNORE_LOOP_INI_ENTRY "ignore_loop"

char *priority_strings[] = {"Idle","Lowest","Below Normal","Normal","Above Normal","Highest (not recommended)","Time Critical (not recommended)"};
int priority_values[] = {THREAD_PRIORITY_IDLE,THREAD_PRIORITY_LOWEST,THREAD_PRIORITY_BELOW_NORMAL,THREAD_PRIORITY_NORMAL,THREAD_PRIORITY_ABOVE_NORMAL,THREAD_PRIORITY_HIGHEST,THREAD_PRIORITY_TIME_CRITICAL};

#define WINAMP_MAX_PATH  32768  /* originally 260+1 */
in_char lastfn[WINAMP_MAX_PATH] = {0}; /* name of the currently playing file */

/* Winamp Play extension list, needed to accept/play and associate extensions in Windows */
#define EXTENSION_LIST_SIZE   VGM_EXTENSION_LIST_CHAR_SIZE * 6
#define EXT_BUFFER_SIZE 200
char working_extension_list[EXTENSION_LIST_SIZE] = {0};

/* plugin config */
double fade_seconds;
double fade_delay_seconds;
double loop_count;
int thread_priority;
int loop_forever;
int ignore_loop;

/* plugin state */
VGMSTREAM * vgmstream = NULL;
HANDLE decode_thread_handle = INVALID_HANDLE_VALUE;
short sample_buffer[576*2*2]; /* 576 16-bit samples, stereo, possibly doubled in size for DSP */

int paused = 0;
int decode_abort = 0;
int seek_needed_samples = -1;
int decode_pos_ms = 0;
int decode_pos_samples = 0;
int stream_length_samples = 0;
int fade_samples = 0;

/* ***************************************** */

/* Winamp INI reader */
static void GetINIFileName(char * iniFile) {
    /* if we're running on a newer winamp version that better supports
     * saving of settings to a per-user directory, use that directory - if not
     * then just revert to the old behaviour */

    if(IsWindow(input_module.hMainWindow) && SendMessage(input_module.hMainWindow, WM_WA_IPC,0,IPC_GETVERSION) >= 0x5000) {
        char * iniDir = (char *)SendMessage(input_module.hMainWindow, WM_WA_IPC, 0, IPC_GETINIDIRECTORY);
        strncpy(iniFile, iniDir, WINAMP_MAX_PATH);

        strncat(iniFile, "\\Plugins\\", WINAMP_MAX_PATH);
        /* can't be certain that \Plugins already exists in the user dir */
        CreateDirectory(iniFile,NULL);
        strncat(iniFile, INI_NAME, WINAMP_MAX_PATH);
    }
    else {
        char * lastSlash;

        GetModuleFileName(NULL, iniFile, WINAMP_MAX_PATH);
        lastSlash = strrchr(iniFile, '\\');

        *(lastSlash + 1) = 0;
        strncat(iniFile, "Plugins\\" INI_NAME,WINAMP_MAX_PATH);
    }
}

/* Adds ext to Winamp's extension list */
static void add_extension(int length, char * dst, const char * ext) {
    char buf[EXT_BUFFER_SIZE];
    char ext_upp[EXT_BUFFER_SIZE];
    int ext_len, written;
    int i,j;
    if (length <= 1)
        return;

    ext_len = strlen(ext);

    /* find end of dst (double \0), saved in i */
    for (i=0; i<length-2 && (dst[i] || dst[i+1]); i++)
        ;

    /* check if end reached or not enough room to add */
    if (i == length-2 || i + EXT_BUFFER_SIZE+2 > length-2 || ext_len * 3 + 20+2 > EXT_BUFFER_SIZE) {
        dst[i]='\0';
        dst[i+1]='\0';
        return;
    }

    if (i > 0)
        i++;

    /* uppercase ext */
    for (j=0; j < ext_len; j++)
        ext_upp[j] = toupper(ext[j]);
    ext_upp[j] = '\0';

    /* copy new extension + double null terminate */
    written = sprintf(buf, "%s%c%s Audio File (*.%s)%c", ext,'\0',ext_upp,ext_upp,'\0'); /*ex: "vgmstream\0vgmstream Audio File (*.VGMSTREAM)\0" */
    for (j=0; j < written; i++,j++)
        dst[i] = buf[j];
    dst[i]='\0';
    dst[i+1]='\0';
}

/* Creates Winamp's extension list, a single string that ends with \0\0.
 * Each extension must be in this format: "extension\0Description\0" */
static void build_extension_list() {
    const char ** ext_list;
    int ext_list_len;
    int i;

    working_extension_list[0]='\0';
    working_extension_list[1]='\0';

    ext_list = vgmstream_get_formats();
    ext_list_len = vgmstream_get_formats_length();

    for (i=0; i < ext_list_len; i++) {
        add_extension(EXTENSION_LIST_SIZE, working_extension_list, ext_list[i]);
    }
}

/* unicode utils */
static void copy_title(in_char * dst, int dst_size, const in_char * src) {
#ifdef UNICODE_INPUT_PLUGIN
    in_char *p = (in_char*)src + wcslen(src); /* find end */
    while (*p != '\\' && p >= src) /* and find last "\" */
        p--;
    p++;
    wcscpy(dst,p); /* copy filename only */
#else
    in_char *p = (in_char*)src + strlen(src); /* find end */
    while (*p != '\\' && p >= src) /* and find last "\" */
        p--;
    p++;
    strcpy(dst,p); /* copy filename only */
#endif
}

/* ***************************************** */

/* about dialog */
void about(HWND hwndParent) {
    MessageBox(hwndParent,
            PLUGIN_DESCRIPTION "\n"
            "by hcs, FastElbja, manakoAT, bxaimc, snakemeat, soneek, kode54, bnnm and many others\n"
            "\n"
            "Winamp plugin by hcs, others\n"
            "\n"
            "https://github.com/kode54/vgmstream/\n"
            "https://sourceforge.net/projects/vgmstream/ (original)"
            ,"about in_vgmstream",MB_OK);
}

/* called at program init */
void init() {
    char iniFile[WINAMP_MAX_PATH];
    char buf[256];
    int consumed;

    GetINIFileName(iniFile);

    thread_priority=GetPrivateProfileInt(APP_NAME,THREAD_PRIORITY_INI_ENTRY,DEFAULT_THREAD_PRIORITY,iniFile);
    if (thread_priority < 0 || thread_priority > 6) {
        sprintf(buf,"%d",DEFAULT_THREAD_PRIORITY);
        WritePrivateProfileString(APP_NAME,THREAD_PRIORITY_INI_ENTRY,buf,iniFile);
        thread_priority = DEFAULT_THREAD_PRIORITY;
    }

    GetPrivateProfileString(APP_NAME,FADE_SECONDS_INI_ENTRY,DEFAULT_FADE_SECONDS,buf,sizeof(buf),iniFile);
    if (sscanf(buf,"%lf%n",&fade_seconds,&consumed)<1 || consumed!=strlen(buf) || fade_seconds < 0) {
        WritePrivateProfileString(APP_NAME,FADE_SECONDS_INI_ENTRY,DEFAULT_FADE_SECONDS,iniFile);
        sscanf(DEFAULT_FADE_SECONDS,"%lf",&fade_seconds);
    }

    GetPrivateProfileString(APP_NAME,FADE_DELAY_SECONDS_INI_ENTRY,DEFAULT_FADE_DELAY_SECONDS,buf,sizeof(buf),iniFile);
    if (sscanf(buf,"%lf%n",&fade_delay_seconds,&consumed)<1 || consumed!=strlen(buf)) {
        WritePrivateProfileString(APP_NAME,FADE_DELAY_SECONDS_INI_ENTRY,DEFAULT_FADE_DELAY_SECONDS,iniFile);
        sscanf(DEFAULT_FADE_DELAY_SECONDS,"%lf",&fade_delay_seconds);
    }

    GetPrivateProfileString(APP_NAME,LOOP_COUNT_INI_ENTRY,DEFAULT_LOOP_COUNT,buf,sizeof(buf),iniFile);
    if (sscanf(buf,"%lf%n",&loop_count,&consumed)!=1 || consumed!=strlen(buf) || loop_count < 0) {
        WritePrivateProfileString(APP_NAME,LOOP_COUNT_INI_ENTRY,DEFAULT_LOOP_COUNT,iniFile);
        sscanf(DEFAULT_LOOP_COUNT,"%lf",&loop_count);
    }

    loop_forever=GetPrivateProfileInt(APP_NAME,LOOP_FOREVER_INI_ENTRY,DEFAULT_LOOP_FOREVER,iniFile);
    ignore_loop=GetPrivateProfileInt(APP_NAME,IGNORE_LOOP_INI_ENTRY,DEFAULT_IGNORE_LOOP,iniFile);

    if (loop_forever && ignore_loop) {
        sprintf(buf,"%d",DEFAULT_LOOP_FOREVER);
        WritePrivateProfileString(APP_NAME,LOOP_FOREVER_INI_ENTRY,buf,iniFile);
        loop_forever = DEFAULT_LOOP_FOREVER;
        sprintf(buf,"%d",DEFAULT_IGNORE_LOOP);
        WritePrivateProfileString(APP_NAME,IGNORE_LOOP_INI_ENTRY,buf,iniFile);
        ignore_loop = DEFAULT_IGNORE_LOOP;
    }

    build_extension_list();
}

/* called at program quit */
void quit() {
}

/* called before extension checks, to allow detection of mms://, etc */
int isourfile(const in_char *fn) {
    return 0; /* we don't recognize protocols */
}

/* request to start playing a file */
int play(const in_char *fn) {
    int max_latency;

    /* don't lose a pointer! */
    if (vgmstream) {
        /* TODO: this should either pop up an error box or close the file */
        return 1; /* error */
    }

    /* open the stream, set up */
    vgmstream = init_vgmstream(fn);
    /* were we able to open it? */
    if (!vgmstream) {
        return 1;
    }
    if (ignore_loop) vgmstream->loop_flag = 0;
    /* will we be able to play it? */
    if (vgmstream->channels <= 0) {
        close_vgmstream(vgmstream);
        vgmstream=NULL;
        return 1; /* error */
    }

    /* Remember that name, friends! */
    strncpy(lastfn,fn,WINAMP_MAX_PATH);

    /* open the output plugin */
    max_latency = input_module.outMod->Open(vgmstream->sample_rate,vgmstream->channels,
            16, 0, 0);
    /* were we able to open it? */
    if (max_latency < 0) {
        close_vgmstream(vgmstream);
        vgmstream=NULL;
        return 1; /* error */
    }

    /* Set info display */
    /* TODO: actual bitrate */
    input_module.SetInfo(get_vgmstream_average_bitrate(vgmstream)/1000,vgmstream->sample_rate/1000,vgmstream->channels,1);

    /* setup visualization */
    input_module.SAVSAInit(max_latency,vgmstream->sample_rate);
    input_module.VSASetInfo(vgmstream->sample_rate,vgmstream->channels);

    decode_abort = 0;
    seek_needed_samples = -1;
    decode_pos_ms = 0;
    decode_pos_samples = 0;
    paused = 0;
    stream_length_samples = get_vgmstream_play_samples(loop_count,fade_seconds,fade_delay_seconds,vgmstream);

    fade_samples = (int)(fade_seconds * vgmstream->sample_rate);

    decode_thread_handle = CreateThread(
            NULL,   /* handle cannot be inherited */
            0,      /* stack size, 0=default */
            decode, /* thread start routine */
            NULL,   /* no parameter to start routine */
            0,      /* run thread immediately */
            NULL);  /* don't keep track of the thread id */

    SetThreadPriority(decode_thread_handle,priority_values[thread_priority]);

    return 0; /* success */
}

/* pause stream */
void pause() {
    paused=1;
    input_module.outMod->Pause(1);
}

/* unpause stream */
void unpause() {
    paused=0;
    input_module.outMod->Pause(0);
}

/* ispaused? return 1 if paused, 0 if not */
int ispaused() {
    return paused;
}

/* stop (unload) stream */
void stop() {
    if (decode_thread_handle != INVALID_HANDLE_VALUE) {
        decode_abort=1;

        /* arbitrary wait length */
        if (WaitForSingleObject(decode_thread_handle,1000) == WAIT_TIMEOUT) {
            /* TODO: error? */
            TerminateThread(decode_thread_handle,0);
        }
        CloseHandle(decode_thread_handle);
        decode_thread_handle = INVALID_HANDLE_VALUE;
    }

    if (vgmstream) {
        close_vgmstream(vgmstream);
        vgmstream=NULL;
    }

    input_module.outMod->Close();
    input_module.SAVSADeInit();
}

/* get length in ms */
int getlength() {
    return stream_length_samples*1000LL/vgmstream->sample_rate;
}

/* current output time in ms */
int getoutputtime() {
    return decode_pos_ms+(input_module.outMod->GetOutputTime()-input_module.outMod->GetWrittenTime());
}

/* seeks to point in stream (in ms) */
void setoutputtime(int t) {
    if (vgmstream)
        seek_needed_samples = (long long)t * vgmstream->sample_rate / 1000LL;
}

/* pass these commands through */
void setvolume(int volume) {
    input_module.outMod->SetVolume(volume);
}
void setpan(int pan) {
    input_module.outMod->SetPan(pan);
}

/* display information */
int infoDlg(const in_char *fn, HWND hwnd) {
    VGMSTREAM * infostream = NULL;
    char description[1024] = {0};

    concatn(sizeof(description),description,PLUGIN_DESCRIPTION "\n\n");

    if (!fn || !*fn) {
        if (!vgmstream) return 0;
        describe_vgmstream(vgmstream,description,sizeof(description));
    } else {
        infostream = init_vgmstream((char*)fn);
        if (!infostream) return 0;
        describe_vgmstream(infostream,description,sizeof(description));
        close_vgmstream(infostream);
        infostream=NULL;
    }

    MessageBox(hwnd,description,"Stream info",MB_OK);
    return 0;
}

/* retrieve information on this or possibly another file */
void getfileinfo(const in_char *filename, in_char *title, int *length_in_ms) {

    if (!filename || !*filename)  /* no filename = use currently playing file */
    {
        if (!vgmstream)
            return;
        if (length_in_ms)
            *length_in_ms = getlength();

        if (title) {
            copy_title(title,GETFILEINFO_TITLE_LENGTH, lastfn);
        }
    }
    else /* some other file */
    {
        VGMSTREAM * infostream;

        if (length_in_ms) {
            *length_in_ms=-1000;

            if ((infostream=init_vgmstream(filename))) {
                *length_in_ms = get_vgmstream_play_samples(loop_count,fade_seconds,fade_delay_seconds,infostream)*1000LL/infostream->sample_rate;

                close_vgmstream(infostream);
                infostream=NULL;
            }
        }

        if (title) {
            copy_title(title,GETFILEINFO_TITLE_LENGTH, filename);
        }
    }
}

/* eq stuff */
void eq_set(int on, char data[10], int preamp) {
    /* nothin' */
}

/* the decode thread */
DWORD WINAPI __stdcall decode(void *arg) {
    /* channel count shouldn't change during decode */
    int max_buffer_samples = sizeof(sample_buffer)/sizeof(sample_buffer[0])/2/vgmstream->channels;

    while (!decode_abort) {

        int samples_to_do;
        int l;

        if (decode_pos_samples+max_buffer_samples>stream_length_samples && (!loop_forever || !vgmstream->loop_flag))
            samples_to_do=stream_length_samples-decode_pos_samples;
        else
            samples_to_do=max_buffer_samples;

        /* play 'till the end of this seek, or note if we're done seeking */
        if (seek_needed_samples != -1) {
            /* reset if we need to seek backwards */
            if (seek_needed_samples < decode_pos_samples) {
                reset_vgmstream(vgmstream);

                if (ignore_loop) vgmstream->loop_flag = 0;

                decode_pos_samples = 0;
                decode_pos_ms = 0;
            }

            if (decode_pos_samples < seek_needed_samples) {
                samples_to_do=seek_needed_samples-decode_pos_samples;
                if (samples_to_do>max_buffer_samples) samples_to_do=max_buffer_samples;
            } else
                seek_needed_samples = -1;

            input_module.outMod->Flush((int)decode_pos_ms);
        }

        l = (samples_to_do*vgmstream->channels*2)<<(input_module.dsp_isactive()?1:0);

        if (samples_to_do == 0) {
            input_module.outMod->CanWrite();    /* ? */
            if (!input_module.outMod->IsPlaying()) {
                PostMessage(input_module.hMainWindow,   /* message dest */
                        WM_WA_MPEG_EOF,     /* message id */
                        0,0);   /* no parameters */
                return 0;
            }
            Sleep(10);
        }
        else if (seek_needed_samples != -1) {
            render_vgmstream(sample_buffer,samples_to_do,vgmstream);

            decode_pos_samples+=samples_to_do;
            decode_pos_ms=decode_pos_samples*1000LL/vgmstream->sample_rate;
        }
        else if (input_module.outMod->CanWrite() >= l) {
            /* let vgmstream do its thing */
            render_vgmstream(sample_buffer,samples_to_do,vgmstream);

            /* fade! */
            if (vgmstream->loop_flag && fade_samples > 0 && !loop_forever) {
                int samples_into_fade = decode_pos_samples - (stream_length_samples - fade_samples);
                if (samples_into_fade + samples_to_do > 0) {
                    int j,k;
                    for (j=0;j<samples_to_do;j++,samples_into_fade++) {
                        if (samples_into_fade > 0) {
                            double fadedness = (double)(fade_samples-samples_into_fade)/fade_samples;
                            for (k=0;k<vgmstream->channels;k++) {
                                sample_buffer[j*vgmstream->channels+k] =
                                    (short)(sample_buffer[j*vgmstream->channels+k]*fadedness);
                            }
                        }
                    }
                }
            }

            input_module.SAAddPCMData((char*)sample_buffer,vgmstream->channels,16,decode_pos_ms);
            input_module.VSAAddPCMData((char*)sample_buffer,vgmstream->channels,16,decode_pos_ms);
            decode_pos_samples+=samples_to_do;
            decode_pos_ms=decode_pos_samples*1000LL/vgmstream->sample_rate;
            if (input_module.dsp_isactive())
                l =input_module.dsp_dosamples(sample_buffer,samples_to_do,16,vgmstream->channels,vgmstream->sample_rate) * 
                    2 * vgmstream->channels;

            input_module.outMod->Write((char*)sample_buffer,l);
        }   /* if we can write enough */
        else Sleep(20);
    }   /* main loop */
    return 0;
}

INT_PTR CALLBACK configDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    char buf[256];
    char iniFile[WINAMP_MAX_PATH];
    static int mypri;
    HANDLE hSlider;

    switch (uMsg) { 
        case WM_CLOSE:
            EndDialog(hDlg,TRUE);
            return TRUE;
        case WM_INITDIALOG:
            GetINIFileName(iniFile);

            /* set CPU Priority slider */
            hSlider=GetDlgItem(hDlg,IDC_THREAD_PRIORITY_SLIDER);
            SendMessage(hSlider, TBM_SETRANGE,
                    (WPARAM) TRUE,                  /* redraw flag */
                    (LPARAM) MAKELONG(1, 7));       /* min. & max. positions */
            SendMessage(hSlider, TBM_SETPOS, 
                    (WPARAM) TRUE,                  /* redraw flag */
                    (LPARAM) thread_priority+1);
            mypri=thread_priority;
            SetDlgItemText(hDlg,IDC_THREAD_PRIORITY_TEXT,priority_strings[thread_priority]);

            sprintf(buf,"%.2lf",fade_seconds);
            SetDlgItemText(hDlg,IDC_FADE_SECONDS,buf);
            sprintf(buf,"%.2lf",fade_delay_seconds);
            SetDlgItemText(hDlg,IDC_FADE_DELAY_SECONDS,buf);
            sprintf(buf,"%.2lf",loop_count);
            SetDlgItemText(hDlg,IDC_LOOP_COUNT,buf);

            if (loop_forever)
                CheckDlgButton(hDlg,IDC_LOOP_FOREVER,BST_CHECKED);
            else if (ignore_loop)
                CheckDlgButton(hDlg,IDC_IGNORE_LOOP,BST_CHECKED);
            else
                CheckDlgButton(hDlg,IDC_LOOP_NORMALLY,BST_CHECKED);

            break;
        case WM_COMMAND:
            switch (GET_WM_COMMAND_ID(wParam, lParam)) {
                case IDOK:
                    {
                        double temp_fade_seconds;
                        double temp_fade_delay_seconds;
                        double temp_loop_count;
                        int consumed;

                        /* read and verify */
                        GetDlgItemText(hDlg,IDC_FADE_SECONDS,buf,sizeof(buf));
                        if (sscanf(buf,"%lf%n",&temp_fade_seconds,&consumed)<1
                            || consumed!=strlen(buf) ||
                            temp_fade_seconds<0) {
                            MessageBox(hDlg,
                                    "Invalid value for Fade Length\n"
                                    "Must be a number greater than or equal to zero",
                                    "Error",MB_OK|MB_ICONERROR);
                            break;
                        }

                        GetDlgItemText(hDlg,IDC_FADE_DELAY_SECONDS,buf,sizeof(buf));
                        if (sscanf(buf,"%lf%n",&temp_fade_delay_seconds,
                            &consumed)<1 || consumed!=strlen(buf)) {
                            MessageBox(hDlg,
                                    "Invalid valid for Fade Delay\n"
                                    "Must be a number",
                                    "Error",MB_OK|MB_ICONERROR);
                            break;
                        }

                        GetDlgItemText(hDlg,IDC_LOOP_COUNT,buf,sizeof(buf));
                        if (sscanf(buf,"%lf%n",&temp_loop_count,&consumed)<1 ||
                                consumed!=strlen(buf) ||
                                temp_loop_count<0) {
                            MessageBox(hDlg,
                                    "Invalid value for Loop Count\n"
                                    "Must be a number greater than or equal to zero",
                                    "Error",MB_OK|MB_ICONERROR);
                            break;
                        }

                        GetINIFileName(iniFile);

                        thread_priority=mypri;
                        sprintf(buf,"%d",thread_priority);
                        WritePrivateProfileString(APP_NAME,THREAD_PRIORITY_INI_ENTRY,buf,iniFile);

                        fade_seconds = temp_fade_seconds;
                        sprintf(buf,"%.2lf",fade_seconds);
                        WritePrivateProfileString(APP_NAME,FADE_SECONDS_INI_ENTRY,buf,iniFile);

                        fade_delay_seconds = temp_fade_delay_seconds;
                        sprintf(buf,"%.2lf",fade_delay_seconds);
                        WritePrivateProfileString(APP_NAME,FADE_DELAY_SECONDS_INI_ENTRY,buf,iniFile);

                        loop_count = temp_loop_count;
                        sprintf(buf,"%.2lf",loop_count);
                        WritePrivateProfileString(APP_NAME,LOOP_COUNT_INI_ENTRY,buf,iniFile);

                        loop_forever = (IsDlgButtonChecked(hDlg,IDC_LOOP_FOREVER) == BST_CHECKED);
                        sprintf(buf,"%d",loop_forever);
                        WritePrivateProfileString(APP_NAME,LOOP_FOREVER_INI_ENTRY,buf,iniFile);

                        ignore_loop = (IsDlgButtonChecked(hDlg,IDC_IGNORE_LOOP) == BST_CHECKED);
                        sprintf(buf,"%d",ignore_loop);
                        WritePrivateProfileString(APP_NAME,IGNORE_LOOP_INI_ENTRY,buf,iniFile);
                    }

                    EndDialog(hDlg,TRUE);
                    break;
                case IDCANCEL:
                    EndDialog(hDlg,TRUE);
                    break;
                case IDC_DEFAULT_BUTTON:
                    /* set CPU Priority slider */
                    hSlider=GetDlgItem(hDlg,IDC_THREAD_PRIORITY_SLIDER);
                    SendMessage(hSlider, TBM_SETRANGE,
                            (WPARAM) TRUE,                  /* redraw flag */
                            (LPARAM) MAKELONG(1, 7));       /* min. & max. positions */
                    SendMessage(hSlider, TBM_SETPOS, 
                            (WPARAM) TRUE,                  /* redraw flag */
                            (LPARAM) DEFAULT_THREAD_PRIORITY+1);
                    mypri=DEFAULT_THREAD_PRIORITY;
                    SetDlgItemText(hDlg,IDC_THREAD_PRIORITY_TEXT,priority_strings[mypri]);

                    SetDlgItemText(hDlg,IDC_FADE_SECONDS,DEFAULT_FADE_SECONDS);
                    SetDlgItemText(hDlg,IDC_FADE_DELAY_SECONDS,DEFAULT_FADE_DELAY_SECONDS);
                    SetDlgItemText(hDlg,IDC_LOOP_COUNT,DEFAULT_LOOP_COUNT);

                    CheckDlgButton(hDlg,IDC_LOOP_FOREVER,BST_UNCHECKED);
                    CheckDlgButton(hDlg,IDC_IGNORE_LOOP,BST_UNCHECKED);
                    CheckDlgButton(hDlg,IDC_LOOP_NORMALLY,BST_CHECKED);
                    break;
                default:
                    return FALSE;
            }
        case WM_HSCROLL:
            if ((struct HWND__ *)lParam==GetDlgItem(hDlg,IDC_THREAD_PRIORITY_SLIDER)) {
                if (LOWORD(wParam)==TB_THUMBPOSITION || LOWORD(wParam)==TB_THUMBTRACK) mypri=HIWORD(wParam)-1;
                else mypri=SendMessage(GetDlgItem(hDlg,IDC_THREAD_PRIORITY_SLIDER),TBM_GETPOS,0,0)-1;
                SetDlgItemText(hDlg,IDC_THREAD_PRIORITY_TEXT,priority_strings[mypri]);
            }
            break;
        default:
            return FALSE;
    }

    return TRUE;
}

/* configuration dialog */
void config(HWND hwndParent) {
    /* defined in resource.rc */
    DialogBox(input_module.hDllInstance, (const char *)IDD_CONFIG, hwndParent, configDlgProc);
}

/* *********************************** */

/* main plugin def */
In_Module input_module = {
    IN_VER,
    PLUGIN_DESCRIPTION,
    0,  /* hMainWindow */
    0,  /* hDllInstance */
    working_extension_list,
    1, /* is_seekable  */
    1, /* uses output */
    config,
    about,
    init,
    quit,
    getfileinfo,
    infoDlg,
    isourfile,
    play,
    pause,
    unpause,
    ispaused,
    stop,
    getlength,
    getoutputtime,
    setoutputtime,
    setvolume,
    setpan,
    0,0,0,0,0,0,0,0,0, // vis stuff
    0,0, // dsp
    eq_set,
    NULL,       // setinfo
    0 // out_mod
};

__declspec( dllexport ) In_Module * winampGetInModule2() {
    return &input_module;
}
