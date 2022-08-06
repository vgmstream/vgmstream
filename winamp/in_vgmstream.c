/**
 * vgmstream for Winamp
 */
#include "in_vgmstream.h"


#include "../version.h"
#ifndef VGMSTREAM_VERSION
#define VGMSTREAM_VERSION "unknown version " __DATE__
#endif

#define PLUGIN_NAME  "vgmstream plugin " VGMSTREAM_VERSION
#define PLUGIN_INFO  PLUGIN_NAME " (" __DATE__ ")"


/* ***************************************** */
/* IN_STATE                                  */
/* ***************************************** */

#define EXT_BUFFER_SIZE 200

/* plugin module (declared at the bottom of this file) */
In_Module input_module;
DWORD WINAPI __stdcall decode(void *arg);

/* Winamp Play extension list, to accept and associate extensions in Windows */
#define EXTENSION_LIST_SIZE   (0x2000 * 6)
/* fixed list to simplify but could also malloc/free on init/close */
char working_extension_list[EXTENSION_LIST_SIZE] = {0};


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

/* info cache (optimization) */
in_char info_fn[PATH_LIMIT] = {0};
in_char info_title[GETFILEINFO_TITLE_LENGTH];
int info_time;
int info_valid;


/* ***************************************** */
/* IN_VGMSTREAM UTILS                        */
/* ***************************************** */

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

/* opens vgmstream with (possibly) an index */
static VGMSTREAM* init_vgmstream_winamp_fileinfo(const in_char* fn) {
    in_char filename[PATH_LIMIT];
    int stream_index = 0;

    /* check for info encoded in the filename */
    parse_fn_string(fn, NULL, filename,PATH_LIMIT);
    parse_fn_int(fn, wa_L("$s"), &stream_index);

    return init_vgmstream_winamp(filename, stream_index);
}


/* makes a modified filename, suitable to pass parameters around */
static void make_fn_subsong(in_char* dst, int dst_size, const in_char* filename, int stream_index) {
    /* Follows "(file)(config)(ext)". Winamp needs to "see" (ext) to validate, and file goes first so relative
     * files work in M3Us (path is added). Protocols a la "vgmstream://(config)(file)" work but don't get full paths. */
    wa_snprintf(dst,dst_size, wa_L("%s|$s=%i|.vgmstream"), filename, stream_index);
}

/* unpacks the subsongs by adding entries to the playlist */
static int split_subsongs(const in_char* filename, int stream_index, VGMSTREAM *vgmstream) {
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
static int add_extension(char* dst, int dst_len, const char* ext) {
    int ext_len;

    ext_len = strlen(ext);
    if (dst_len <= ext_len + 1)
        return 0;

    strcpy(dst, ext); /* seems winamp uppercases this if needed */
    dst[ext_len] = ';';

    return ext_len + 1;
}

/* Creates Winamp's extension list, a single string that ends with \0\0.
 * Each extension must be in this format: "extensions\0Description\0"
 *
 * The list is used to accept extensions by default when IsOurFile returns 0, to register file
 * types, and in the open dialog's type combo. Format actually can be:
 * - "ext1;ext2;...\0EXTS Audio Files (*.ext1; *.ext2; *...\0", //single line with all
 * - "ext1\0EXT1 Audio File (*.ext1)\0ext2\0EXT2 Audio File (*.ext2)\0...", //multiple lines
 * Open dialog's text (including all plugin's "Description") seems limited to old MAX_PATH 260
 * (max size for "extensions" checks seems ~0x40000 though). Given vgmstream's huge number
 * of exts, use single line to (probably) work properly with dialogs (used to be multi line).
 */
static void build_extension_list(char* winamp_list, int winamp_list_size) {
    const char** ext_list;
    size_t ext_list_len;
    int i, written;
    int description_size = 0x100; /* reserved max at the end */

    winamp_list[0] = '\0';
    winamp_list[1] = '\0';

    ext_list = vgmstream_get_formats(&ext_list_len);

    for (i = 0; i < ext_list_len; i++) {
        int used = add_extension(winamp_list, winamp_list_size - description_size, ext_list[i]);
        if (used <= 0) {
            vgm_logi("build_extension_list: not enough buf for all exts\n");
            break;
        }
        winamp_list += used;
        winamp_list_size -= used;
    }
    if (i > 0) {
        winamp_list[-1] = '\0'; /* last "ext;" to "ext\0" */
    }

    /* generic description for the info dialog since we can't really show everything */
    written = snprintf(winamp_list, winamp_list_size - 2, "vgmstream Audio Files%c", '\0');

    /* should end with double \0 */
    if (written < 0) {
        winamp_list[0] = '\0';
        winamp_list[1] = '\0';
    }
    else {
        winamp_list[written + 0] = '\0';
        winamp_list[written + 1] = '\0';
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

static int winampGetExtendedFileInfo_common(in_char* filename, char* metadata, char* ret, int retlen);

static double get_album_gain_volume(const in_char* fn) {
    char replaygain[64];
    double gain = 0.0;
    int had_replaygain = 0;
    if (settings.gain_type == REPLAYGAIN_NONE)
        return 1.0;

    //;{ char f8[PATH_LIMIT]; wa_ichar_to_char(f8,PATH_LIMIT,(in_char*)fn); vgm_logi("get_album_gain_volume: file %s\n", f8); }

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
void winamp_About(HWND hwndParent) {
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
        MessageBox(hwndParent, buf,TEXT("about in_vgmstream"),MB_OK);
    }
}

/* called at program init */
void winamp_Init() {

    settings.is_xmplay = is_xmplay();

    logger_init();
    vgmstream_set_log_callback(VGM_LOG_LEVEL_ALL, logger_callback);

    /* get ini config */
    load_defaults(&defaults);
    load_config(&input_module, &settings, &defaults);

    /* XMPlay with in_vgmstream doesn't support most IPC_x messages so no playlist manipulation */
    if (settings.is_xmplay) {
        settings.disable_subsongs = 1;
    }

    /* dynamically make a list of supported extensions */
    build_extension_list(working_extension_list, sizeof(working_extension_list));
}

/* called at program quit */
void winamp_Quit() {
    logger_free();
}

/* called before extension checks, to allow detection of mms://, etc */
int winamp_IsOurFile(const in_char *fn) {
    VGMSTREAM* infostream;
    vgmstream_ctx_valid_cfg cfg = {0};
    char filename_utf8[PATH_LIMIT];
    int valid;

    /* Winamp file opening 101:
     * - load modules from plugin dir, in NTFS order (mostly alphabetical *.dll but not 100% like explorer)
     *   > plugin list in options is ordered by description so doesn't reflect this priority
     * - make path to file
     * - find first module that returns 1 in "IsOurFile" (continue otherwise)
     *   > generally plugins just return 0 there as it's meant for protocols (a few do check the file's header there)
     * - find first module that reports that supports file extension (see build_extension_list)
     *   > this means plugin priority affects who hijacks the file, for shared extensions
     * - if no result, retry the above 2 with "hi." + default extension (from config, default .mp3, path if not set?)
     *   > seems skipped when doing playlist manipulation/subsongs
     * ! if module/vgmstream is given the file (even if can't play it) Winamp will call GetInfo and stop if not valid info is returned
     * ! on init seems Winamp calls IsOurFile with "cda://" protocol, but should be ignored by is_valid()
     */

    /* Detect repeat retries and fake "hi." calls as they are useless for our detection.
     * before only ignored "hi's" when commons exts where on but who wants unplayable files reporting 0:00. */
    if (wa_strcmp(fn, info_fn) == 0) { //TODO may need to check file size to invalidate cache
        //;vgm_logi("winamp_IsOurFile: repeated call\n");
        return info_valid;
    }

    if (/*settings.exts_common_on &&*/ wa_strncmp(fn, wa_L("hi."), 3) == 0) {
        //;vgm_logi("winamp_IsOurFile: ignored fakefile\n");
        return 1;
    }


    cfg.skip_standard = 0; /* validated by Winamp after IsOurFile, reject just in case */
    cfg.accept_unknown = settings.exts_unknown_on;
    cfg.accept_common = settings.exts_common_on;

    wa_ichar_to_char(filename_utf8, PATH_LIMIT, fn);

    //;vgm_logi("winamp_IsOurFile: %s\n", filename_utf8);

    /* Return 1 if we actually handle the format or 0 to let other plugins handle it. Checking the 
     * extension alone isn't enough, as we may hijack stuff like in_vgm's *.vgm, so also try to
     * open/get info from the file (slower so keep some cache) */

    info_valid = 0; /* may not be playable */
    wa_strncpy(info_fn, fn, PATH_LIMIT); /* copy now for repeat calls */

    /* basic extension check */
    valid = vgmstream_ctx_is_valid(filename_utf8, &cfg);
    if (!valid) {
        //;vgm_logi("winamp_IsOurFile: invalid extension\n");
        return 0;
    }


    /* format check */
    infostream = init_vgmstream_winamp_fileinfo(fn);
    if (!infostream) {
        //;vgm_logi("winamp_IsOurFile: invalid infostream\n");
        return 0;
    }

    //TODO simplify, variations used in other places (or rather, fix the API)

    /* first thing winamp does after accepting a file is asking for time/title, so keep those around to avoid re-parsing the file */
    {
        int32_t num_samples;

        apply_config(infostream, &settings);

        vgmstream_mixing_autodownmix(infostream, settings.downmix_channels);
        vgmstream_mixing_enable(infostream, 0, NULL, NULL);

        num_samples = vgmstream_get_samples(infostream);
        info_time = num_samples * 1000LL /infostream->sample_rate;

        get_title(info_title,GETFILEINFO_TITLE_LENGTH, fn, infostream);
    }

    //;vgm_logi("winamp_IsOurFile: accepted\n");
    info_valid = 1;
    close_vgmstream(infostream);

    return 1;
}


/* request to start playing a file */
int winamp_Play(const in_char *fn) {
    int max_latency;
    in_char filename[PATH_LIMIT];
    int stream_index = 0;

    //;{ char f8[PATH_LIMIT]; wa_ichar_to_char(f8,PATH_LIMIT,fn); vgm_logi("winamp_Play: file %s\n", f8); }

    /* shouldn't happen */
    if (vgmstream)
        return 1;

    /* check for info encoded in the filename */
    parse_fn_string(fn, NULL, filename,PATH_LIMIT);
    parse_fn_int(fn, wa_L("$s"), &stream_index);

    /* open the stream */
    vgmstream = init_vgmstream_winamp(filename, stream_index);
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
    char description[1024] = {0};
    TCHAR tbuf[1024] = {0};
    double tmpVolume = 1.0;

    if (!fn || !*fn) {
        /* no filename = current playing file */
        if (!vgmstream)
            return 0;

        describe_vgmstream(vgmstream,description,sizeof(description));
    }
    else {
        /* some other file in playlist given by filename */
        VGMSTREAM* infostream = NULL;

        infostream = init_vgmstream_winamp_fileinfo(fn);
        if (!infostream) return 0;

        apply_config(infostream, &settings);

        vgmstream_mixing_autodownmix(infostream, settings.downmix_channels);
        vgmstream_mixing_enable(infostream, 0, NULL, NULL);

        describe_vgmstream(infostream,description,sizeof(description));

        close_vgmstream(infostream);
        infostream = NULL;
        tmpVolume = get_album_gain_volume(fn);
    }

    if (tmpVolume != 1.0) {
        char tmp[128] = {0};
        snprintf(tmp, sizeof(tmp), "\nvolume: %.6f\n", tmpVolume);
        concatn(sizeof(description), description, tmp);
    }

    concatn(sizeof(description), description, "\n" PLUGIN_INFO);

    cfg_char_to_wchar(tbuf, sizeof(tbuf) / sizeof(TCHAR), description);
    MessageBox(hwnd, tbuf, TEXT("Stream info"), MB_OK);

    return 0;
}

/* retrieve title (playlist name) and time on the current or other file in the playlist */
void winamp_GetFileInfo(const in_char *fn, in_char *title, int *length_in_ms) {

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
        VGMSTREAM* infostream = NULL;
        //;{ char f8[PATH_LIMIT]; wa_ichar_to_char(f8,PATH_LIMIT,fn); vgm_logi("winamp_GetFileInfo: file %s\n", f8); }

        /* not changed from last IsOurFile (most common) */
        if (info_valid && wa_strcmp(fn, info_fn) == 0) {
            if (title) wa_strncpy(title, info_title, GETFILEINFO_TITLE_LENGTH);
            if (length_in_ms) *length_in_ms = info_time;
            return;
        }

        /* some other file in playlist given by filename */
        infostream = init_vgmstream_winamp_fileinfo(fn);
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
    PLUGIN_NAME,
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

    //;{ char f8[PATH_LIMIT]; wa_ichar_to_char(f8,PATH_LIMIT,filename); vgm_logi("winampGetExtendedFileInfo_common: file %s\n", f8); }

    /* load list current tags, if necessary */
    load_tagfile_info(filename);
    if (!last_tags.loaded) /* tagfile not found, fail so default get_title takes over */
        return 0; //goto fail;

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

    //;{ vgm_logi("winampGetExtendedFileInfo: file %s\n", filename); }

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

    //;{ char f8[PATH_LIMIT]; wa_ichar_to_char(f8,PATH_LIMIT,filename); vgm_logi("winampGetExtendedFileInfoW: file %s\n", f8); }

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
    VGMSTREAM* xvgmstream = NULL;

    //;{ char f8[PATH_LIMIT]; wa_ichar_to_char(f8,PATH_LIMIT,fn); vgm_logi("winampGetExtendedRead_open_common: open common file %s\n", f8); }

    /* open the stream */
    xvgmstream = init_vgmstream_winamp_fileinfo(fn);
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

__declspec(dllexport) void* winampGetExtendedRead_open(const char *fn, int *size, int *bps, int *nch, int *srate) {
    in_char filename_wchar[PATH_LIMIT];

    wa_char_to_ichar(filename_wchar, PATH_LIMIT, fn);

    return winampGetExtendedRead_open_common(filename_wchar, size, bps, nch, srate);
}

__declspec(dllexport) void* winampGetExtendedRead_openW(const wchar_t *fn, int *size, int *bps, int *nch, int *srate) {
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
winampGetExtendedRead_open_float
winampGetExtendedRead_openW_float
void winampAddUnifiedFileInfoPane
#endif
