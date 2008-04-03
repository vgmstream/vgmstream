/* Winamp plugin interface for vgmstream */
/* Largely copied from: */
/*
** Example Winamp .RAW input plug-in
** Copyright (c) 1998, Justin Frankel/Nullsoft Inc.
*/

#ifdef _MSC_VER
#define _CRT_SECURE_NO_DEPRECATE
#endif
#include <windows.h>
#include <stdio.h>

#include "../src/vgmstream.h"
#include "../src/util.h"
#include "in2.h"

#ifndef VERSION
#define VERSION
#endif

#define PLUGIN_DESCRIPTION "vgmstream plugin " VERSION " " __DATE__

/* post when playback stops */
#define WM_WA_MPEG_EOF WM_USER+2

In_Module input_module; /* the input module, declared at the bottom of this file */
DWORD WINAPI __stdcall decode(void *arg);

char lastfn[MAX_PATH+1] = {0}; /* name of the currently playing file */
short sample_buffer[576*2*2]; /* 576 16-bit samples, stereo, possibly doubled in size for DSP */

/* hardcode these now, TODO will be configurable later */
const double fade_seconds = 10.0;
const double loop_count = 2.0;

VGMSTREAM * vgmstream = NULL;
HANDLE decode_thread_handle = INVALID_HANDLE_VALUE;
int paused = 0;
int decode_abort = 0;
int decode_pos_ms = 0;
int decode_pos_samples = 0;
int stream_length_samples = 0;
int fade_samples = 0;

#define EXTENSION_LIST_SIZE 1024
char working_extension_list[EXTENSION_LIST_SIZE] = {0};
#define EXTENSION_COUNT 11
char * extension_list[EXTENSION_COUNT] = {
    "adx\0ADX Audio File (*.ADX)\0",
    "afc\0AFC Audio File (*.AFC)\0",
    "agsc\0AGSC Audio File (*.AGSC)\0",
    "ast\0AST Audio File (*.AST)\0",
    "brstm\0BRSTM Audio File (*.BRSTM)\0",
    "hps\0HALPST Audio File (*.HPS)\0",
    "strm\0STRM Audio File (*.STRM)\0",
    "adp\0ADP Audio File (*.ADP)\0",
    "rsf\0RSF Audio File (*.RSF)\0",
    "dsp\0DSP Audio File (*.DSP)\0",
    "gcw\0GCW Audio File (*.GCW)\0",
};

/* stubs, we don't do anything fancy yet */
void config(HWND hwndParent) {}
void about(HWND hwndParent) {}
void quit() {}

void build_extension_list() {
    int i;
    working_extension_list[0]='\0';
    working_extension_list[1]='\0';

    for (i=0;i<EXTENSION_COUNT;i++) {
        concatn_doublenull(EXTENSION_LIST_SIZE,working_extension_list,
                extension_list[i]);
    }
}

void init() {
    build_extension_list();
}

/* we don't recognize protocols */
int isourfile(char *fn) { return 0; }

/* request to start playing a file */
int play(char *fn)
{
    int max_latency;


    /* don't lose a pointer! */
    if (vgmstream) {
        /* TODO: this should either pop up an error box or close the file */
        return 1;
    }
    /* open the stream, set up */
    vgmstream = init_vgmstream(fn);
    /* were we able to open it? */
    if (!vgmstream) {
        return 1;
    }
    /* will we be able to play it? */
    if (vgmstream->channels <= 0 || vgmstream->channels > 2) {
        /* TODO: > 2 channels is not unheard of, we should probably complain */
        close_vgmstream(vgmstream);
        vgmstream=NULL;
        return 1;
    }

    /* Remember that name, friends! */
    strncpy(lastfn,fn,MAX_PATH);

    /* open the output plugin */
    max_latency = input_module.outMod->Open(vgmstream->sample_rate,vgmstream->channels,
            16, 0, 0);
    /* were we able to open it? */
    if (max_latency < 0) {
        close_vgmstream(vgmstream);
        vgmstream=NULL;
    }

    /* Set info display */
    /* TODO: actual bitrate */
    input_module.SetInfo(100,vgmstream->sample_rate/1000,vgmstream->channels,1);

    /* setup visualization */
    input_module.SAVSAInit(max_latency,vgmstream->sample_rate);
    input_module.VSASetInfo(vgmstream->sample_rate,vgmstream->channels);

    decode_abort = 0;
    decode_pos_ms = 0;
    decode_pos_samples = 0;
    paused = 0;
    stream_length_samples = get_vgmstream_play_samples(loop_count,fade_seconds,vgmstream);
    fade_samples = fade_seconds * vgmstream->sample_rate;

    decode_thread_handle = CreateThread(
            NULL,   /* handle cannot be inherited */
            0,      /* stack size, 0=default */
            decode, /* thread start routine */
            NULL,   /* no parameter to start routine */
            0,      /* run thread immediately */
            NULL);  /* don't keep track of the thread id */

    return 0;
}

/* pausing... */
void pause() { paused=1; input_module.outMod->Pause(1); }
void unpause() {paused=0; input_module.outMod->Pause(0); }
int ispaused() { return paused; }

/* stop playback */
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

/* get current stream length */
int getlength() {
    return stream_length_samples*1000LL/vgmstream->sample_rate;
}

/* get current output time */
int getoutputtime() {
    return decode_pos_ms+(input_module.outMod->GetOutputTime()-input_module.outMod->GetWrittenTime());
}

/* seek */
void setoutputtime(int t) {
    /* TODO: seeking */
}

/* pass these commands through */
void setvolume(int volume) { input_module.outMod->SetVolume(volume); }
void setpan(int pan) { input_module.outMod->SetPan(pan); }

/* display information */
int infoDlg(char *fn, HWND hwnd) {
    VGMSTREAM * infostream = NULL;
    char description[1024];
    description[0]='\0';

    if (!fn || !*fn) {
        if (!vgmstream) return 0;
        describe_vgmstream(vgmstream,description,1024);
    } else {
        infostream = init_vgmstream(fn);
        if (!infostream) return 0;
        describe_vgmstream(infostream,description,1024);
        close_vgmstream(infostream);
        infostream=NULL;
    }

    MessageBox(hwnd,description,"Stream info",MB_OK);
    return 0;
}

/* retrieve information on this or possibly another file */
    void getfileinfo(char *filename, char *title, int *length_in_ms) {
        if (!filename || !*filename)  // currently playing file
        {
            if (!vgmstream) return;
            if (length_in_ms) *length_in_ms=getlength();
            if (title) 
            {
                char *p=lastfn+strlen(lastfn);
                while (*p != '\\' && p >= lastfn) p--;
                strcpy(title,++p);
            }
        }
        else // some other file
        {
            VGMSTREAM * infostream;
            if (length_in_ms) 
            {
                *length_in_ms=-1000;
                if ((infostream=init_vgmstream(filename)))
                {
                    // these are only second-accurate, but how accurate does this need to be anyway?
                    *length_in_ms = get_vgmstream_play_samples(loop_count,fade_seconds,infostream)*1000LL/infostream->sample_rate;
                    close_vgmstream(infostream);
                    infostream=NULL;
                }
            }
            if (title) 
            {
                char *p=filename+strlen(filename);
                while (*p != '\\' && p >= filename) p--;
                strcpy(title,++p);
            }
        }
    }

/* nothin' */
void eq_set(int on, char data[10], int preamp) {}

/* the decode thread */
DWORD WINAPI __stdcall decode(void *arg) {
    while (!decode_abort) {

        int samples_to_do;
        int l;
        if (decode_pos_samples+576>stream_length_samples)
            samples_to_do=stream_length_samples-decode_pos_samples;
        else
            samples_to_do=576;

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
        else if (input_module.outMod->CanWrite() >= l) {
            /* let vgmstream do its thing */
            render_vgmstream(sample_buffer,samples_to_do,vgmstream);

            /* fade! */
            if (vgmstream->loop_flag && fade_samples > 0) {
                int samples_into_fade = decode_pos_samples - (stream_length_samples - fade_samples);
                if (samples_into_fade + samples_to_do > 0) {
                    int j,k;
                    for (j=0;j<samples_to_do;j++,samples_into_fade++) {
                        if (samples_into_fade > 0) {
                            double fadedness = (double)(fade_samples-samples_into_fade)/fade_samples;
                            for (k=0;k<vgmstream->channels;k++) {
                                sample_buffer[j*vgmstream->channels+k] =
                                    sample_buffer[j*vgmstream->channels+k]*fadedness;
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

In_Module input_module = 
{
    IN_VER,
    PLUGIN_DESCRIPTION,
    0,  // hMainWindow
    0,  // hDllInstance
    working_extension_list,
    1, // is_seekable
    1, // uses output
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

__declspec( dllexport ) In_Module * winampGetInModule2()
{
    return &input_module;
}

