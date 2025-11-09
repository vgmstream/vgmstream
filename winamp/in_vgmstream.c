/**
 * vgmstream for Winamp
 */
#include "in_vgmstream.h"
#include "sdk/in2_extra.h"



#include "../version.h"
#ifndef VGMSTREAM_VERSION
#define VGMSTREAM_VERSION "unknown version " __DATE__
#endif

#define PLUGIN_NAME  "vgmstream plugin " VGMSTREAM_VERSION
#define PLUGIN_INFO  PLUGIN_NAME " (" __DATE__ ")"


/* ***************************************** */
/* IN_STATE                                  */
/* ***************************************** */

#define VGMSTREAM_MAX_CHANNELS 64 //TODO: fix, vgmstream could change max
#define EXT_BUFFER_SIZE 200

/* plugin module (declared at the bottom of this file) */
In_Module input_module;
DWORD WINAPI __stdcall decode(void *arg);

/* Winamp Play extension list, to accept and associate extensions in Windows
 * (older versions of XMPlay also will crash with so many exts but can't autodetect version here) */
#define EXTENSION_LIST_SIZE   (0x2000 * 6)
/* fixed list to simplify but could also malloc/free on init/close */
char extension_list[EXTENSION_LIST_SIZE] = {0};


/* current play state */
typedef struct {
    int paused;
    int decode_abort;
    int seek_sample;
    int decode_pos_ms;
    int decode_pos_samples;
    double volume;
} winamp_state_t;


/* Winamp needs at least 576 16-bit samples, stereo, doubled in case DSP effects are active */
#define SAMPLE_BUFFER_SIZE 576
const char* tagfile_name = "!tags.m3u";

/* plugin state */
HANDLE decode_thread_handle = INVALID_HANDLE_VALUE;

libvgmstream_t* vgmstream = NULL;
in_char lastfn[WINAMP_PATH_LIMIT] = {0}; /* name of the currently playing file */

winamp_settings_t defaults;
winamp_settings_t settings;
winamp_state_t state;
short sample_buffer[SAMPLE_BUFFER_SIZE * 2 * VGMSTREAM_MAX_CHANNELS]; //todo maybe should be dynamic

/* info cache (optimization) */
in_char info_fn[WINAMP_PATH_LIMIT] = {0};
in_char info_title[GETFILEINFO_TITLE_LENGTH];
int64_t info_time;
bool info_valid;
bool info_was_protocol;


/* ***************************************** */
/* IN_VGMSTREAM UTILS                        */
/* ***************************************** */

#if 0
#if defined(VGM_LOG_OUTPUT) || defined(VGM_DEBUG_OUTPUT)
    static void vgm_logi(const char* fmt, ...) {
        va_list args;

        va_start(args, fmt);

        char line[256];
        int out;
        out = vsnprintf(line, sizeof(line), fmt, args);
        if (out < 0 || out > sizeof(line))
            strcpy(line, "(ignored log)"); //to-do something better, meh
        logger_callback(LIBVGMSTREAM_LOG_LEVEL_INFO, line);

        va_end(args);
    }
#else
    #define static vgm_logi(...) {} /* nothing */
#endif
#endif

/* parses a modified filename ('fakename') extracting tags parameters (NULL tag for first = filename) */
static int parse_fn_string(const in_char* fn, const in_char* tag, in_char* dst, int dst_size) {
    const in_char* end = wa_strchr(fn,'|');

    if (tag==NULL) {
        wa_strcpy(dst,fn);
        if (end)
            dst[end - fn] = '\0';
        return 1;
    }

    dst[0] = '\0';
    return 0;
}

static int parse_fn_int(const in_char* fn, const in_char* tag, int* num) {
    const in_char* start = wa_strchr(fn,'|');

    if (start > 0) {
        wa_sscanf(start+1, wa_L("$s=%i "), num);
        return 1;
    } else {
        *num = 0;
        return 0;
    }
}


static void load_vconfig(libvgmstream_config_t* vcfg, winamp_settings_t* settings) {

    vcfg->allow_play_forever = true;
    vcfg->play_forever = settings->loop_forever;
    vcfg->loop_count = settings->loop_count;
    vcfg->fade_time = settings->fade_time;
    vcfg->fade_delay = settings->fade_delay;
    vcfg->ignore_loop = settings->ignore_loop;

    vcfg->auto_downmix_channels = settings->downmix_channels;
    vcfg->force_sfmt = LIBVGMSTREAM_SFMT_PCM16; //winamp can only handle PCM16/24, and the later is almost never used in vgm
}

/* opens vgmstream for winamp */
static libvgmstream_t* init_vgmstream_winamp(const in_char* fn, int subsong) {

    libstreamfile_t* sf = open_winamp_streamfile_by_ipath(fn);
    if (!sf) return NULL;

    libvgmstream_config_t vcfg = {0};
    load_vconfig(&vcfg, &settings);

    libvgmstream_t* vgmstream = libvgmstream_create(sf, subsong, &vcfg);
    if (!vgmstream) goto fail;

    libstreamfile_close(sf);
    return vgmstream;
fail:
    libstreamfile_close(sf);
    libvgmstream_free(vgmstream);
    return NULL;
}

/* opens vgmstream with (possibly) an index */
static libvgmstream_t* init_vgmstream_winamp_fileinfo(const in_char* fn) {
    in_char filename[WINAMP_PATH_LIMIT];
    int stream_index = 0;

    /* check for info encoded in the filename */
    parse_fn_string(fn, NULL, filename, WINAMP_PATH_LIMIT);
    parse_fn_int(fn, wa_L("$s"), &stream_index);

    return init_vgmstream_winamp(filename, stream_index);
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

/* unicode utils */
static void get_title(in_char* dst, int dst_size, const in_char* fn, libvgmstream_t* infostream) {
    in_char filename[WINAMP_PATH_LIMIT];
    char buffer[WINAMP_PATH_LIMIT];
    char filename_utf8[WINAMP_PATH_LIMIT];

    parse_fn_string(fn, NULL, filename,WINAMP_PATH_LIMIT);
    //parse_fn_int(fn, wa_L("$s"), &stream_index);

    wa_ichar_to_char(filename_utf8, WINAMP_PATH_LIMIT, filename);

    /* infostream gets added at first with index 0, then once played it re-adds proper numbers */
    if (infostream) {
        libvgmstream_title_t tcfg = {0};
        bool is_first = infostream->format->subsong_index == 0;

        tcfg.force_title = settings.force_title;
        tcfg.subsong_range = is_first;
        tcfg.remove_extension = true;
        tcfg.filename = filename_utf8;

        libvgmstream_get_title(infostream, &tcfg, buffer, sizeof(buffer));

        wa_char_to_ichar(dst, dst_size, buffer);
    }
}

static int winampGetExtendedFileInfo_common(in_char* filename, char* metadata, char* ret, int retlen);

static double get_album_gain_volume(const in_char* fn) {
    char replaygain[64];
    double gain = 0.0;
    int had_replaygain = 0;
    if (settings.gain_type == REPLAYGAIN_NONE)
        return 1.0;

    //;{ char f8[WINAMP_PATH_LIMIT]; wa_ichar_to_char(f8,WINAMP_PATH_LIMIT,(in_char*)fn); vgm_logi("get_album_gain_volume: file %s\n", f8); }

    replaygain[0] = '\0'; /* reset each time to make sure we read actual tags */
    if (settings.gain_type == REPLAYGAIN_ALBUM
            && winampGetExtendedFileInfo_common((in_char*)fn, "replaygain_album_gain", replaygain, sizeof(replaygain))
            && replaygain[0] != '\0') {
        gain = atof(replaygain);
        had_replaygain = 1;
    }

    replaygain[0] = '\0';
    if (!had_replaygain
            && winampGetExtendedFileInfo_common((in_char*)fn, "replaygain_track_gain", replaygain, sizeof(replaygain))
            && replaygain[0] != '\0') {
        gain = atof(replaygain);
        had_replaygain = 1;
    }

    if (had_replaygain) {
        double vol = pow(10.0, gain / 20.0);
        double peak = 1.0;

        replaygain[0] = '\0';
        if (settings.clip_type == REPLAYGAIN_ALBUM
                && winampGetExtendedFileInfo_common((in_char*)fn, "replaygain_album_peak", replaygain, sizeof(replaygain))
                && replaygain[0] != '\0') {
            peak = atof(replaygain);
        }
        else if (settings.clip_type != REPLAYGAIN_NONE
                && winampGetExtendedFileInfo_common((in_char*)fn, "replaygain_track_peak", replaygain, sizeof(replaygain))
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
static void winamp_About(HWND hwndParent) {
    const char* ABOUT_TEXT =
            PLUGIN_INFO "\n"
            "by hcs, FastElbja, manakoAT, bxaimc, snakemeat, soneek, kode54, bnnm, Nicknine, Thealexbarney, CyberBotX, and many others\n"
            "\n"
            "Winamp plugin by hcs, others\n"
            "\n"
            "https://github.com/vgmstream/vgmstream/\n"
            "https://sourceforge.net/projects/vgmstream/ (original)";

    {
        TCHAR buf[1024];
        size_t buf_size = 1024;

        cfg_char_to_wchar(buf, buf_size, ABOUT_TEXT);
        MessageBox(hwndParent, buf, TEXT("about in_vgmstream"), MB_OK);
    }
}

/* called at program init */
static void winamp_Init() {

    settings.is_xmplay = is_xmplay();

    logger_init();

    libvgmstream_set_log(0, logger_callback);

    /* get ini config */
    load_defaults(&defaults);
    load_config(&input_module, &settings, &defaults);

    /* XMPlay with in_vgmstream doesn't support most IPC_x messages so no playlist manipulation */
    if (settings.is_xmplay) {
        settings.disable_subsongs = 1;
    }

    /* dynamically make a list of supported extensions */
    build_extension_list(extension_list, sizeof(extension_list));
}

/* called at program quit */
static void winamp_Quit() {
    logger_free();
}

/* called before extension checks, to allow detection of mms://, etc */
static int winamp_IsOurFile(const in_char *fn) {
    libvgmstream_t* infostream;
    libvgmstream_valid_t vcfg = {0};
    char filename_utf8[WINAMP_PATH_LIMIT];
    bool valid;

    //;vgm_logi("\nwinamp_IsOurFile: init\n");

    /* Winamp file opening 101:
     * - load modules from plugin dir, in NTFS order (mostly alphabetical *.dll but not 100% like explorer.exe)
     *   > plugin list in options is ordered by description so doesn't reflect this priority
     * - make path to file
     * - find first module that returns 1 in "IsOurFile", continue otherwise
     *   > generally plugins just return 0 as it's meant for protocols (a few do check the file's header there)
     * - find first module that reports that supports file extension (see build_extension_list)
     *   > for shared extensions this means plugin priority affects who hijacks the file
     * - if still no result, retry the above 2 steps with "hi." + default extension (from config: default .mp3, path if not set?)
     *   > procotols supported by Winamp (cda:// http:// rtcp:// etc) also reach this "hi.mp3" stage (only if no extension is used?)
     *   > seems skipped when doing playlist manipulation/subsongs
     * - if still nothing, any unplayable file will be considered "hi.mp3" by Winamp and accepted as a 0:00 file
     *   > this is annoying so vgmstream "accepts" hi.mp3 yet won't play it, so time is blank instead of 0:00
     *   > but must ignore "hi.mp3" if current file was a protocol to allow it
     *
     * Notes:
     * * if module/vgmstream is given the file (even if can't play it) Winamp will call GetInfo and stop if not valid info is returned
     * * on init seems Winamp calls IsOurFile with "cda://" protocol, should be ignored by is_valid() as well as any other protocol
     */

    // detect repeat retries
    if (wa_strcmp(fn, info_fn) == 0) { //TODO may need to check file size to invalidate cache
        //;vgm_logi("winamp_IsOurFile: repeated call\n");
        return info_valid;
    }

    // 'accept' hi.mp3; won't play so it'll show with blank time instead of 0:00
    if (/*settings.exts_common_on &&*/ wa_strncmp(fn, wa_L("hi."), 3) == 0) {
        if (info_was_protocol) { 
            // don't 'accept' with protocols, that somehow also go through hi.mp3 before playing
            //;vgm_logi("winamp_IsOurFile: ignored fakefile (was a protocol)\n");
            return 0;
        }
        //;vgm_logi("winamp_IsOurFile: accepted fakefile\n");
        return 1;
    }

    info_was_protocol = wa_strstr(fn, wa_L("://"));

    vcfg.skip_standard = false; /* validated by Winamp after IsOurFile, reject just in case */
    vcfg.accept_unknown = settings.exts_unknown_on;
    vcfg.accept_common = settings.exts_common_on;

    wa_ichar_to_char(filename_utf8, WINAMP_PATH_LIMIT, fn);
    //;vgm_logi("winamp_IsOurFile: %s\n", filename_utf8);

    /* Return 1 if we actually handle the format or 0 to let other plugins handle it. Checking the 
     * extension alone isn't enough, as we may hijack stuff like in_vgm's *.vgm, so also try to
     * open/get info from the file (slower so keep some cache) */

    info_valid = false; /* may not be playable */
    wa_strncpy(info_fn, fn, WINAMP_PATH_LIMIT); /* copy now for repeat calls */

    /* basic extension check */
    valid = libvgmstream_is_valid(filename_utf8, &vcfg);
    if (!valid) {
        //;vgm_logi("winamp_IsOurFile: false is-valid\n");
        return 0;
    }


    /* format check */
    infostream = init_vgmstream_winamp_fileinfo(fn);
    if (!infostream) {
        //;vgm_logi("winamp_IsOurFile: invalid infostream\n");
        return 0;
    }

    /* first thing winamp does after accepting a file is asking for time/title, so keep those around to avoid re-parsing the file */
    {
        info_time = infostream->format->play_samples * 1000LL / infostream->format->sample_rate;

        get_title(info_title,GETFILEINFO_TITLE_LENGTH, fn, infostream);
    }

    //;vgm_logi("winamp_IsOurFile: accepted\n");
    info_valid = true;
    libvgmstream_free(infostream);

    return 1;
}


/* request to start playing a file */
static int winamp_Play(const in_char *fn) {
    int max_latency;
    in_char filename[WINAMP_PATH_LIMIT];
    int stream_index = 0;

    //;{ char f8[WINAMP_PATH_LIMIT]; wa_ichar_to_char(f8,WINAMP_PATH_LIMIT,fn); vgm_logi("winamp_Play: file %s\n", f8); }

    /* shouldn't happen */
    if (vgmstream)
        return 1;

    /* check for info encoded in the filename */
    parse_fn_string(fn, NULL, filename,WINAMP_PATH_LIMIT);
    parse_fn_int(fn, wa_L("$s"), &stream_index);

    /* open the stream */
    vgmstream = init_vgmstream_winamp(filename, stream_index);
    if (!vgmstream)
        return 1;

    /* add N subsongs to the playlist, if any */
    if (split_subsongs(filename, stream_index, vgmstream)) {
        libvgmstream_free(vgmstream);
        vgmstream = NULL;
        return 1;
    }

    /* reset internals */
    state.paused = 0;
    state.decode_abort = 0;
    state.seek_sample = -1;
    state.decode_pos_ms = 0;
    state.decode_pos_samples = 0;
    state.volume = get_album_gain_volume(fn);

    /* save original name */
    wa_strncpy(lastfn,fn,WINAMP_PATH_LIMIT);

    /* open the output plugin */
    max_latency = input_module.outMod->Open(vgmstream->format->sample_rate, vgmstream->format->channels, 16, 0, 0);
    if (max_latency < 0) {
        libvgmstream_free(vgmstream);
        vgmstream = NULL;
        return 1;
    }

    /* set info display */
    input_module.SetInfo(vgmstream->format->stream_bitrate / 1000, vgmstream->format->sample_rate / 1000, vgmstream->format->channels, 1);

    /* setup visualization */
    input_module.SAVSAInit(max_latency, vgmstream->format->sample_rate);
    input_module.VSASetInfo(vgmstream->format->sample_rate, vgmstream->format->channels);

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
static void winamp_Pause() {
    state.paused = 1;
    input_module.outMod->Pause(1);
}

/* unpause stream */
static void winamp_UnPause() {
    state.paused = 0;
    input_module.outMod->Pause(0);
}

/* return 1 if paused, 0 if not */
static int winamp_IsPaused() {
    return state.paused;
}

/* stop (unload) stream */
static void winamp_Stop() {

    if (decode_thread_handle != INVALID_HANDLE_VALUE) {
        state.decode_abort = 1;

        /* arbitrary wait milliseconds (error can trigger if the system is *really* busy) */
        if (WaitForSingleObject(decode_thread_handle, 5000) == WAIT_TIMEOUT) {
            MessageBox(input_module.hMainWindow, TEXT("Error stopping decode thread\n"), TEXT("Error"), MB_OK|MB_ICONERROR);
            TerminateThread(decode_thread_handle, 0);
        }
        CloseHandle(decode_thread_handle);
        decode_thread_handle = INVALID_HANDLE_VALUE;
    }


    libvgmstream_free(vgmstream);
    vgmstream = NULL;

    input_module.outMod->Close();
    input_module.SAVSADeInit();
}

/* get length in ms */
static int winamp_GetLength() {
    if (!vgmstream)
        return 0;
    return vgmstream->format->play_samples * 1000LL / vgmstream->format->sample_rate;
}

/* current output time in ms */
static int winamp_GetOutputTime() {
    int32_t pos_ms = state.decode_pos_ms;
    /* for some reason this gets triggered hundred of times by non-classic skins when using subsongs */
    if (!vgmstream)
        return 0;

    /* pretend we have reached destination if called while seeking is on */
    if (state.seek_sample >= 0)
        pos_ms = state.seek_sample * 1000LL / vgmstream->format->sample_rate;

    return pos_ms + (input_module.outMod->GetOutputTime() - input_module.outMod->GetWrittenTime());
}

/* seeks to point in stream (in ms) */
static void winamp_SetOutputTime(int time_in_ms) {
    if (!vgmstream)
        return;
    state.seek_sample = (long long)time_in_ms * vgmstream->format->sample_rate / 1000LL;
}

/* pass these commands through */
static void winamp_SetVolume(int volume) {
    input_module.outMod->SetVolume(volume);
}
static void winamp_SetPan(int pan) {
    input_module.outMod->SetPan(pan);
}

// TODO: remove
static void wa_concatn(int length, char * dst, const char * src) {
    int i,j;
    if (length <= 0) return;
    for (i=0;i<length-1 && dst[i];i++);   /* find end of dst */
    for (j=0;i<length-1 && src[j];i++,j++)
        dst[i]=src[j];
    dst[i]='\0';
}


/* display info box (ALT+3) */
static int winamp_InfoBox(const in_char *fn, HWND hwnd) {
    char description[1024] = {0};
    TCHAR tbuf[1024] = {0};
    double tmpVolume = 1.0;

    if (!fn || !*fn) {
        /* no filename = current playing file */
        if (!vgmstream)
            return 0;

        libvgmstream_format_describe(vgmstream, description, sizeof(description));
    }
    else {
        /* some other file in playlist given by filename */
        libvgmstream_t* infostream = NULL;

        infostream = init_vgmstream_winamp_fileinfo(fn);
        if (!infostream) return 0;

        libvgmstream_format_describe(infostream, description, sizeof(description));

        libvgmstream_free(infostream);
        infostream = NULL;
        tmpVolume = get_album_gain_volume(fn);
    }

    if (tmpVolume != 1.0) {
        char tmp[128] = {0};
        snprintf(tmp, sizeof(tmp), "\nvolume: %.6f\n", tmpVolume);
        wa_concatn(sizeof(description), description, tmp);
    }

    wa_concatn(sizeof(description), description, "\n" PLUGIN_INFO);

    cfg_char_to_wchar(tbuf, sizeof(tbuf) / sizeof(TCHAR), description);
    MessageBox(hwnd, tbuf, TEXT("Stream info"), MB_OK);

    return 0;
}

/* retrieve title (playlist name) and time on the current or other file in the playlist */
static void winamp_GetFileInfo(const in_char *fn, in_char *title, int *length_in_ms) {

    if (!fn || !*fn) {
        /* no filename = current playing file */
        //;vgm_logi("winamp_GetFileInfo: current\n");

        /* Sometimes called even if no file is playing (usually when hovering "O" > preferences... submenu).
         * No idea what's that about so try to reuse last info */
        if (!vgmstream) {
            if (info_valid) {
                if (title) wa_strncpy(title, info_title, GETFILEINFO_TITLE_LENGTH);
                if (length_in_ms) *length_in_ms = info_time;
            }
            return;
        }

        if (title) {
            get_title(title,GETFILEINFO_TITLE_LENGTH, lastfn, vgmstream);
        }

        if (length_in_ms) {
            *length_in_ms = winamp_GetLength();
        }
    }
    else {
        libvgmstream_t* infostream = NULL;
        //;{ char f8[WINAMP_PATH_LIMIT]; wa_ichar_to_char(f8,WINAMP_PATH_LIMIT,fn); vgm_logi("winamp_GetFileInfo: file %s\n", f8); }

        /* not changed from last IsOurFile (most common) */
        if (info_valid && wa_strcmp(fn, info_fn) == 0) {
            if (title) wa_strncpy(title, info_title, GETFILEINFO_TITLE_LENGTH);
            if (length_in_ms) *length_in_ms = info_time;
            return;
        }

        /* some other file in playlist given by filename */
        infostream = init_vgmstream_winamp_fileinfo(fn);
        if (!infostream) return;

        if (title) {
            get_title(title,GETFILEINFO_TITLE_LENGTH, fn, infostream);
        }

        if (length_in_ms) {
            *length_in_ms = -1000;
            if (infostream) {
                *length_in_ms = infostream->format->play_samples * 1000LL / infostream->format->sample_rate;
            }
        }

        libvgmstream_free(infostream);
        infostream = NULL;
    }
}

/* eq stuff */
static void winamp_EQSet(int on, char data[10], int preamp) {
}

/*****************************************************************************/
/* MAIN DECODE (some used in extended part too, so avoid globals) */

static void do_seek(winamp_state_t* state, libvgmstream_t* vgmstream) {
    bool play_forever = vgmstream->format->play_forever;
    int seek_sample = state->seek_sample;  /* local due to threads/race conditions changing state->seek_sample elsewhere */

    /* ignore seeking past file, can happen using the right (->) key, ok if playing forever */
    if (state->seek_sample > vgmstream->format->play_samples && !play_forever) {
        state->seek_sample = -1;

        state->decode_pos_samples = vgmstream->format->play_samples;
        state->decode_pos_ms = state->decode_pos_samples * 1000LL / vgmstream->format->sample_rate;
        return;
    }

    /* could divide in N seeks (from pos) for slower files so cursor moves, but doesn't seem too necessary */
    libvgmstream_seek(vgmstream, seek_sample);

    state->decode_pos_samples = seek_sample;
    state->decode_pos_ms = state->decode_pos_samples * 1000LL / vgmstream->format->sample_rate;

    /* different sample: other seek may have been requested during seek */
    if (state->seek_sample == seek_sample)
        state->seek_sample = -1;
}

static void apply_gain(float volume, short* buf, int channels, int samples_to_do) {
    if (volume == 1.0)
        return;
    if (volume == 0.0) // possible?
        return;

    for (int s = 0; s < samples_to_do * channels; s++) {
        buf[s] = (short)(buf[s] * volume);
    }
}

/* the decode thread */
DWORD WINAPI __stdcall decode(void *arg) {
    const int max_buffer_samples = SAMPLE_BUFFER_SIZE;
    int channels = vgmstream->format->channels;
    int sample_rate = vgmstream->format->sample_rate;

    // Winamp's DSP may need up to x2 samples
    int max_output_bytes = (max_buffer_samples * channels * sizeof(short));
    if (input_module.dsp_isactive())
        max_output_bytes = max_output_bytes * 2;

    while (!state.decode_abort) {

        /* track finished and not seeking */
        if (vgmstream->decoder->done && state.seek_sample < 0) {
            input_module.outMod->CanWrite();    // ?
            if (!input_module.outMod->IsPlaying()) {
                PostMessage(input_module.hMainWindow, WM_WA_MPEG_EOF, 0,0); // end
                return 0;
            }
            Sleep(10);
            continue;
        }


        /* seek */
        if (state.seek_sample >= 0) {
            do_seek(&state, vgmstream);

            // flush Winamp buffers *after* fully seeking (allows to play buffered samples while we seek, feels a bit snappier)
            if (state.seek_sample < 0)
                input_module.outMod->Flush(state.decode_pos_ms);
            continue;
        }


        /* can't write right now */
        if (input_module.outMod->CanWrite() < max_output_bytes) {
            Sleep(20);
            continue;
        }


        /* decode */
        int err = libvgmstream_fill(vgmstream, sample_buffer, max_buffer_samples);
        if (err < 0) break;

        int buf_bytes = vgmstream->decoder->buf_bytes;
        int buf_samples = vgmstream->decoder->buf_samples;
        void* buf = vgmstream->decoder->buf; //sample_buffer
        if (!buf_samples)
            continue;

        apply_gain(state.volume, buf, buf_samples, channels); // apply ReplayGain, if needed


        /* output samples */
        input_module.SAAddPCMData(buf, channels, 16, state.decode_pos_ms);
        input_module.VSAAddPCMData(buf, channels, 16, state.decode_pos_ms);

        if (input_module.dsp_isactive()) {
            // DSP effects in Winamp assume buf is big enough (up to x2), and may be enabled/disabled mid-play
            int dsp_output_samples = input_module.dsp_dosamples(buf, buf_samples, 16, channels, sample_rate);
            buf_bytes = dsp_output_samples * channels * sizeof(short);
        }

        input_module.outMod->Write(buf, buf_bytes);

        state.decode_pos_samples += buf_samples;
        state.decode_pos_ms = state.decode_pos_samples * 1000LL / sample_rate;
    }

    return 0;
}

/* configuration dialog */
static void winamp_Config(HWND hwndParent) {
    /* open dialog defined in resource.rc */
    DialogBox(input_module.hDllInstance, (const TCHAR *)IDD_CONFIG, hwndParent, configDlgProc);
}

/* *********************************** */

/* main plugin def */
In_Module input_module = {
    IN_VER,
    PLUGIN_NAME,
    0,  /* hMainWindow (filled in by Winamp) */
    0,  /* hDllInstance (filled in by Winamp) */
    extension_list,
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

/* could malloc and stuff but totals aren't much bigger than WINAMP_PATH_LIMITs anyway */
#define WINAMP_TAGS_ENTRY_MAX      30
#define WINAMP_TAGS_ENTRY_SIZE     2048

typedef struct {
    int loaded;
    in_char filename[WINAMP_PATH_LIMIT]; /* tags are loaded for this file */
    int tag_count;

    char keys[WINAMP_TAGS_ENTRY_MAX][WINAMP_TAGS_ENTRY_SIZE+1];
    char vals[WINAMP_TAGS_ENTRY_MAX][WINAMP_TAGS_ENTRY_SIZE+1];
} winamp_tags;

winamp_tags last_tags;


/* Loads all tags for a filename in a temp struct to improve performance, as
 * Winamp requests one tag at a time and may reask for the same tag several times */
static void load_tagfile_info(in_char* filename) {
    libstreamfile_t* sf_tags = NULL;
    in_char filename_clean[WINAMP_PATH_LIMIT];
    char filename_utf8[WINAMP_PATH_LIMIT];
    char tagfile_path_utf8[WINAMP_PATH_LIMIT];
    in_char tagfile_path_i[WINAMP_PATH_LIMIT];
    char* path;


    if (settings.tagfile_disable) { /* reset values if setting changes during play */
        last_tags.loaded = 0;
        last_tags.tag_count = 0;
        return;
    }

    /* clean extra part for subsong tags */
    parse_fn_string(filename, NULL, filename_clean,WINAMP_PATH_LIMIT);

    if (wa_strcmp(last_tags.filename, filename_clean) == 0) {
        return; /* not changed, tags still apply */
    }

    last_tags.loaded = 0;

    /* tags are now for this filename, find tagfile path */
    wa_ichar_to_char(filename_utf8, WINAMP_PATH_LIMIT, filename_clean);
    strcpy(tagfile_path_utf8,filename_utf8);

    path = strrchr(tagfile_path_utf8,'\\');
    if (path != NULL) {
        path[1] = '\0'; /* includes "\", remove after that from tagfile_path */
        strcat(tagfile_path_utf8, tagfile_name);
    }
    else { /* ??? */
        strcpy(tagfile_path_utf8, tagfile_name);
    }
    wa_char_to_ichar(tagfile_path_i, WINAMP_PATH_LIMIT, tagfile_path_utf8);

    wa_strcpy(last_tags.filename, filename_clean);
    last_tags.tag_count = 0;

    /* load all tags from tagfile */
    sf_tags = open_winamp_streamfile_by_ipath(tagfile_path_i);
    if (sf_tags != NULL) {
        libvgmstream_tags_t* tags;

        tags = libvgmstream_tags_init(sf_tags);
        libvgmstream_tags_find(tags, filename_utf8);
        while (libvgmstream_tags_next_tag(tags)) {
            int repeated_tag = 0;
            int current_tag = last_tags.tag_count;
            if (current_tag >= WINAMP_TAGS_ENTRY_MAX)
                continue;

            /* should overwrite repeated tags as global tags may appear multiple times */
            for (int i = 0; i < current_tag; i++) {
                if (strcmp(last_tags.keys[i], tags->key) == 0) {
                    current_tag = i;
                    repeated_tag = 1;
                    break;
                }
            }

            last_tags.keys[current_tag][0] = '\0';
            strncat(last_tags.keys[current_tag], tags->key, WINAMP_TAGS_ENTRY_SIZE);
            last_tags.vals[current_tag][0] = '\0';
            strncat(last_tags.vals[current_tag], tags->val, WINAMP_TAGS_ENTRY_SIZE);
            if (!repeated_tag)
                last_tags.tag_count++;
        }

        libvgmstream_tags_free(tags);
        libstreamfile_close(sf_tags);
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

    //;{ char f8[WINAMP_PATH_LIMIT]; wa_ichar_to_char(f8,WINAMP_PATH_LIMIT,filename); vgm_logi("winampGetExtendedFileInfo_common: file %s\n", f8); }

    /* load list current tags, if necessary */
    load_tagfile_info(filename);
    if (!last_tags.loaded) /* tagfile not found, fail so default get_title takes over */
        return 0; //goto fail;

    /* always called (value in ms), must return ok so other tags get called.
     * "0" shows length 0 in the media library but works in the playlist, null seems correct (auto-loaded) */ 
    if (strcasecmp(metadata, "length") == 0) {
        strcpy(ret, "\0");
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

    //TODO: is this always needed for Winamp to use replaygain?
    if (!tag_found && strcasecmp(metadata, "replaygain_track_gain") == 0) {
        //strcpy(ret, "1.0"); //should set some default value?
        return 1;
    }

    if (!tag_found)
        goto fail;

    return 1;

fail:
    return 0;
}


/* for Winamp 5.24 */
__declspec (dllexport) int winampGetExtendedFileInfo(char *filename, char *metadata, char *ret, int retlen) {
    in_char filename_wchar[WINAMP_PATH_LIMIT];
    int ok;

    if (settings.tagfile_disable)
        return 0;

    wa_char_to_ichar(filename_wchar,WINAMP_PATH_LIMIT, filename);

    //;{ vgm_logi("winampGetExtendedFileInfo: file %s\n", filename); }

    ok = winampGetExtendedFileInfo_common(filename_wchar, metadata, ret, retlen);
    if (ok == 0)
        return 0;

    return 1;
}

/* for Winamp 5.3+ */
__declspec (dllexport) int winampGetExtendedFileInfoW(wchar_t *filename, char *metadata, wchar_t *ret, int retlen) {
    in_char filename_ichar[WINAMP_PATH_LIMIT];
    char ret_utf8[2048];
    int ok;

    if (settings.tagfile_disable)
        return 0;

    wa_wchar_to_ichar(filename_ichar,WINAMP_PATH_LIMIT, filename);

    //;{ char f8[WINAMP_PATH_LIMIT]; wa_ichar_to_char(f8,WINAMP_PATH_LIMIT,filename); vgm_logi("winampGetExtendedFileInfoW: file %s\n", f8); }

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
static void* winampGetExtendedRead_open_common(in_char *fn, int *size, int *bps, int *nch, int *srate) {
    libvgmstream_t* xvgmstream = NULL;

    //;{ char f8[WINAMP_PATH_LIMIT]; wa_ichar_to_char(f8,WINAMP_PATH_LIMIT,fn); vgm_logi("winampGetExtendedRead_open_common: open common file %s\n", f8); }

    /* open the stream */
    xvgmstream = init_vgmstream_winamp_fileinfo(fn);
    if (!xvgmstream) {
        return NULL;
    }

    /* reset internals */
    xstate.paused = 0; /* unused */
    xstate.decode_abort = 0; /* unused */
    xstate.seek_sample = -1;
    xstate.decode_pos_ms = 0; /* unused */
    xstate.decode_pos_samples = 0;
    xstate.volume = 1.0; /* unused */

    if (size) /* bytes to decode (-1 if unknown) */
        *size = xvgmstream->format->play_samples * xvgmstream->format->channels * sizeof(short);
    if (bps)
        *bps = 16;
    if (nch)
        *nch = xvgmstream->format->channels;
    if (srate)
        *srate = xvgmstream->format->sample_rate;

    return xvgmstream; /* handle passed to other extended functions */
}

__declspec(dllexport) void* winampGetExtendedRead_open(const char *fn, int *size, int *bps, int *nch, int *srate) {
    in_char filename_wchar[WINAMP_PATH_LIMIT];

    wa_char_to_ichar(filename_wchar, WINAMP_PATH_LIMIT, fn);

    return winampGetExtendedRead_open_common(filename_wchar, size, bps, nch, srate);
}

__declspec(dllexport) void* winampGetExtendedRead_openW(const wchar_t *fn, int *size, int *bps, int *nch, int *srate) {
    in_char filename_ichar[WINAMP_PATH_LIMIT];

    wa_wchar_to_ichar(filename_ichar, WINAMP_PATH_LIMIT, fn);

    return winampGetExtendedRead_open_common(filename_ichar, size, bps, nch, srate);
}

/* decode len to dest buffer, called multiple times until file done or decoding is aborted */
__declspec(dllexport) size_t winampGetExtendedRead_getData(void *handle, char *dest, size_t len, int *killswitch) {
    const int max_buffer_samples = SAMPLE_BUFFER_SIZE;
    unsigned copied = 0;

    libvgmstream_t* xvgmstream = handle;
    if (!xvgmstream)
        return 0;
    int channels = xvgmstream->format->channels;

    while (copied < len) {

        /* check decoding cancelled */
        if (killswitch && *killswitch) {
            break;
        }

        /* track finished */
        if (xvgmstream->decoder->done) {
            break;
        }

        /* seek */
        if (xstate.seek_sample != -1) {
            do_seek(&xstate, xvgmstream);
            continue;
        }

        /* decode */
        int samples_left = (len - copied) / channels * sizeof(short);
        int samples_to_do = max_buffer_samples;
        if (samples_to_do > samples_left)
            samples_to_do = samples_left;

        int samples_done = libvgmstream_fill(xvgmstream, xsample_buffer, samples_to_do);
        if (samples_done == 0)
            continue;
        // ReplayGain is automatically applied in this API

        memcpy(&dest[copied], xsample_buffer, samples_done * channels * sizeof(short));
        copied += samples_done * channels * sizeof(short);

        xstate.decode_pos_samples += samples_done;
    }

    return copied; /* return 0 to signal file done */
}

/* seek in the file (possibly unused) */
__declspec(dllexport) int winampGetExtendedRead_setTime(void *handle, int time_in_ms) {
    libvgmstream_t* xvgmstream = handle;
    if (xvgmstream) {
        xstate.seek_sample = (long long)time_in_ms * xvgmstream->format->sample_rate / 1000LL;
        return 1;
    }
    return 0;
}

/* file done */
__declspec(dllexport) void winampGetExtendedRead_close(void *handle) {
    libvgmstream_t* xvgmstream = handle;
    if (xvgmstream) {
        libvgmstream_free(xvgmstream);
    }
}
