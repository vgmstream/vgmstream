/**
 * vgmstream for XMPlay
 */

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdio.h>
#include <io.h>
#include <conio.h>
#include <string.h>
#include <ctype.h>

#include "xmp_vgmstream.h"

#include "../version.h"
#ifndef VGMSTREAM_VERSION
#define VGMSTREAM_VERSION "unknown version " __DATE__
#endif
#define PLUGIN_NAME  "vgmstream plugin " VGMSTREAM_VERSION
#define PLUGIN_INFO  PLUGIN_NAME " (" __DATE__ ")"


/* ************************************* */

#define XMPLAY_MAX_PATH  32768

/* XMPlay function library */
static XMPFUNC_IN* xmpf_in;
static XMPFUNC_MISC* xmpf_misc;
static XMPFUNC_FILE* xmpf_file;

/* XMPlay extension list, only needed to associate extensions in Windows */
#define EXTENSION_LIST_SIZE  (0x2000 * 6)

char working_extension_list[EXTENSION_LIST_SIZE] = {0};

char last_filepath[MAX_PATH] = {0};

/* plugin config */
double fade_seconds = 10.0;
double fade_delay = 0.0;
double loop_count = 2.0;
bool ignore_loop = false;
bool disable_subsongs = true;
BOOL xmplay_doneloop = 0;

/* plugin state */
libvgmstream_t* vgmstream = NULL;
int decode_pos_samples;
int length_samples = 0;
int current_subsong = 0;

//XMPFILE current_file = NULL;
//char current_fn[XMPLAY_MAX_PATH] = {0};

static int shownerror = 0; /* init error */

/* ************************************* */

static void load_vconfig(libvgmstream_config_t* vcfg) {

    vcfg->allow_play_forever = false;
  //vcfg->play_forever = loop_forever;
    vcfg->loop_count = loop_count;
    vcfg->fade_time = fade_seconds;
    vcfg->fade_delay = fade_delay;
    vcfg->ignore_loop = ignore_loop;

    vcfg->force_sfmt = LIBVGMSTREAM_SFMT_FLOAT; //xmplay only handles floats
}

/* opens vgmstream for xmplay */
libvgmstream_t* init_vgmstream_xmplay(XMPFILE infile, const char* path, int subsong) {

    libstreamfile_t* sf = NULL;
    if (infile) {
        sf = open_xmplay_streamfile_by_xmpfile(infile, xmpf_file, path, false); // external XMPFILE
        if (!sf) return NULL;
    }
    else {
        sf = libstreamfile_open_from_stdio(path); //TODO: unicode problems?
        if (!sf) return NULL;
    }

    libvgmstream_config_t vcfg = {0};
    load_vconfig(&vcfg);

    libvgmstream_t* vgmstream = libvgmstream_create(sf, subsong, &vcfg);
    if (!vgmstream) goto fail;

    libstreamfile_close(sf);
    return vgmstream;
fail:
    libstreamfile_close(sf);
    libvgmstream_free(vgmstream);
    return NULL;
}

/* ************************************* */

/* info for the "about" button in plugin options */
void WINAPI xmplay_About(HWND win) {
    MessageBox(win,
            PLUGIN_INFO "\n"
            "by hcs, FastElbja, manakoAT, bxaimc, snakemeat, soneek, kode54, bnnm, Nicknine, Thealexbarney, CyberBotX, and many others\n"
            "\n"
            "XMPlay plugin by unknownfile, PSXGamerPro1, kode54, others\n"
            "\n"
            "https://github.com/vgmstream/vgmstream/\n"
            "https://sourceforge.net/projects/vgmstream/ (original)"
            ,"about xmp-vgmstream",MB_OK);
}

#if 0
/* present config options to user (OPTIONAL) */
void WINAPI xmplay_Config(HWND win) {
    /* defined in resource.rc */
    DialogBox(input_module.hDllInstance, (const char *)IDD_CONFIG, win, configDlgProc);
}
#endif

/* quick check if a file is playable by this plugin */
BOOL WINAPI xmplay_CheckFile(const char *filename, XMPFILE file) {

    libvgmstream_t* infostream = init_vgmstream_xmplay(file, filename, 0);
    if (!infostream)
        return FALSE;

    libvgmstream_free(infostream);

    return TRUE;
}

/* update info from a file, returning the number of subsongs */
DWORD WINAPI xmplay_GetFileInfo(const char *filename, XMPFILE file, float **length, char **tags) {
    libvgmstream_t* infostream = NULL;
    int subsong_count;

    infostream = init_vgmstream_xmplay(file, filename, 0);
    if (!infostream)
        return 0;

    char temp_filepath[MAX_PATH];

    snprintf(temp_filepath, sizeof(temp_filepath), "%s", filename);
    temp_filepath[sizeof(temp_filepath) - 1] = '\0';

    if (length && infostream->format->sample_rate) {
        int length_samples = infostream->format->play_samples;
        float *lens = (float*)xmpf_misc->Alloc(sizeof(float));
        lens[0] = (float)length_samples / (float)infostream->format->sample_rate;
        *length = lens;
    }

    subsong_count = infostream->format->subsong_count;
    if (disable_subsongs || subsong_count == 0)
        subsong_count = 1;

    *tags = get_tags_from_filepath_info(infostream, xmpf_misc, temp_filepath);
    libvgmstream_free(infostream);

    return subsong_count;
}

/* open a file for playback, returning:  0=failed, 1=success, 2=success and XMPlay can close the file */
DWORD WINAPI xmplay_Open(const char *filename, XMPFILE file) {

    vgmstream = init_vgmstream_xmplay(file, filename, current_subsong+1);
    if (!vgmstream)
        return 0;

    snprintf(last_filepath, sizeof(last_filepath), "%s", filename);
    last_filepath[sizeof(last_filepath) - 1] = '\0';

    decode_pos_samples = 0;
    length_samples = vgmstream->format->play_samples;

    //strncpy(current_fn,filename,XMPLAY_MAX_PATH);
    //current_file = file;
    //current_subsong = 0;


    if (length_samples) {
        float length = (float)length_samples / (float)vgmstream->format->sample_rate;
        xmpf_in->SetLength(length, TRUE);
    }

    return 1;
}

/* close the playback file */
void WINAPI xmplay_Close() {
    libvgmstream_free(vgmstream);
    vgmstream = NULL;
}

/* set the sample format */
void WINAPI xmplay_SetFormat(XMPFORMAT *form) {
    form->chan = vgmstream->format->channels;
    form->rate = vgmstream->format->sample_rate;

    // xmplay only handles float samples so this is info only
    switch(vgmstream->format->sample_format) {
        case LIBVGMSTREAM_SFMT_FLOAT: form->res = 4; break;
        case LIBVGMSTREAM_SFMT_PCM24: form->res = 3; break;
        case LIBVGMSTREAM_SFMT_PCM16: form->res = 2; break;
      //case LIBVGMSTREAM_SFMT_PCM8:  form->res = 1; break;
        default: form->res = 0; break;
    }
}

/* get tags, return NULL to delay title update (OPTIONAL) */
char* WINAPI xmplay_GetTags() {
    return get_tags_from_filepath_info(vgmstream, xmpf_misc, last_filepath);
}

/* main panel info text (short file info) */
void WINAPI xmplay_GetInfoText(char* format, char* length) {
    if (!format)
        return;
    if (!vgmstream)
        return;

    int rate = vgmstream->format->sample_rate;
    int samples = vgmstream->format->stream_samples;
    int bps = vgmstream->format->stream_bitrate;

    int t = samples / rate;
    int tmin = t / 60;
    int tsec = t % 60;

    sprintf(format, "%s", vgmstream->format->codec_name);
    sprintf(length, "%d:%02d - %dKb/s - %dHz", tmin, tsec, bps, rate);
}

/* info for the "General" window/tab (buf is ~40K) */
void WINAPI xmplay_GetGeneralInfo(char* buf) {

    if (!buf)
        return;
    if (!vgmstream)
        return;

    
    char description[1024];
    libvgmstream_format_describe(vgmstream, description, sizeof(description));

    /* tags are divided with a tab and lines with carriage return so we'll do some guetto fixin' */
	int tag_done = 0;
	description[0] -= 32; //Replace small letter with capital for consistency with XMPlay's output
    for (int i = 0; i < 1024; i++) {
        if (description[i] == '\0')
            break;

        if (description[i] == ':' && !tag_done) { /* to ignore multiple ':' in a line*/
            description[i] = ' ';
            description[i+1] = '\t';
            tag_done = 1;
        }

        if (description[i] == '\n') {
            description[i] = '\r';
			if (description[i+1])
				description[i+1] -= 32; //Replace small letter with capital for consistency with XMPlay's output
            tag_done = 0;
        }
    }

    sprintf(buf,"%s", description);
}

/* get the seeking granularity in seconds */
double WINAPI xmplay_GetGranularity() {
    return 0.001; /* can seek in milliseconds */
}

/* seek to a position (in granularity units), return new position or -1 = failed */
double WINAPI xmplay_SetPosition(DWORD pos) {
    double cpos;
    int32_t seek_sample = (int32_t)(pos * xmplay_GetGranularity() * vgmstream->format->sample_rate);

    if (pos == XMPIN_POS_AUTOLOOP || pos == XMPIN_POS_LOOP)
        xmplay_doneloop = 1;

#if 0
    /* set a subsong */
    if (!disable_subsongs && (pos & XMPIN_POS_SUBSONG)) {
        int new_subsong = LOWORD(pos);

        /* "single subsong mode (don't show info on other subsongs)" */
        if (pos & XMPIN_POS_SUBSONG1) {
            // ???
        }

        if (new_subsong && new_subsong != current_subsong) { /* todo implicit? */
            if (current_file)
                return -1;

            vgmstream = init_vgmstream_xmplay(current_file, current_fn, current_subsong+1);
            if (!vgmstream) return -1;
            
            current_subsong = new_subsong;
            return 0.0;
        }
    }
#endif

    libvgmstream_seek(vgmstream, seek_sample);

    decode_pos_samples = seek_sample;

    cpos = (double)decode_pos_samples / vgmstream->format->sample_rate;
    return cpos;
}

/* decode some sample data */
DWORD WINAPI xmplay_Process(float* buf, DWORD bufsize) {

    if (vgmstream->decoder->done)
        return 0;

    bufsize /= vgmstream->format->channels;

    BOOL do_loop = xmpf_in->GetLooping(); //TODO: what is this?
    int32_t samples_to_do = do_loop ? bufsize : length_samples - decode_pos_samples;
    if (samples_to_do > bufsize)
        samples_to_do = bufsize;

    int res = libvgmstream_fill(vgmstream, buf, samples_to_do);
    if (res < 0) return 0;

    int32_t done = vgmstream->decoder->buf_samples;

    /* decode */
    decode_pos_samples += done;

    return done * vgmstream->format->channels;
}

static DWORD WINAPI xmplay_GetSubSongs(float *length) {
    int subsong_count;

    if (!vgmstream)
        return 0;

    subsong_count = vgmstream->format->subsong_count;
    if (disable_subsongs || subsong_count == 0)
        subsong_count = 1;

    /* get times for all subsongs */
    //todo request updating playlist update every subsong change instead?
    {
        /* not good for vgmstream as would mean re-parsing many times */
        //int i;
        //*length = 0;
        //for (i = 0; i < subsong_count; i++) {
        //    float subsong_length = ...
        //    *length += subsong_length;
        //}

        /* simply use the current length */ //todo just use 0?
        length_samples = vgmstream->format->play_samples;
        *length = (float)length_samples / (float)vgmstream->format->sample_rate;
    }

    return subsong_count;
}

/* *********************************** */

/* main plugin def, see xmpin.h */
XMPIN vgmstream_xmpin = {
    XMPIN_FLAG_CANSTREAM,
    "vgmstream for XMPlay",
    working_extension_list,
    xmplay_About,
    NULL,//XMP_Config
    xmplay_CheckFile,
    xmplay_GetFileInfo,
    xmplay_Open,
    xmplay_Close,
    NULL,
    xmplay_SetFormat,
    xmplay_GetTags, //(OPTIONAL) --actually mandatory
    xmplay_GetInfoText,
    xmplay_GetGeneralInfo,
    NULL,//GetMessage - text for the "Message" tab window/tab (OPTIONAL)
    xmplay_SetPosition,
    xmplay_GetGranularity,
    NULL,
    xmplay_Process,
    NULL,
    NULL,
    xmplay_GetSubSongs,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

/* get the plugin's XMPIN interface */
__declspec(dllexport) XMPIN* WINAPI  XMPIN_GetInterface(UINT32 face, InterfaceProc faceproc) {
    if (face != XMPIN_FACE) {
        // unsupported version
        if (face < XMPIN_FACE && !shownerror) {
            MessageBox(0,
                    "The xmp-vgmstream plugin requires XMPlay 3.8 or above.\n\n"
                    "Please update at:\n"
                    "http://www.un4seen.com/xmplay.html\n"
                    "http://www.un4seen.com/stuff/xmplay.exe", 0, MB_ICONEXCLAMATION);
            shownerror = 1;
        }
        return NULL;
    }

    /* retrieval of xmp helpers */
    xmpf_in = (XMPFUNC_IN*)faceproc(XMPFUNC_IN_FACE);
    xmpf_misc = (XMPFUNC_MISC*)faceproc(XMPFUNC_MISC_FACE);
    xmpf_file = (XMPFUNC_FILE*)faceproc(XMPFUNC_FILE_FACE);

    build_extension_list(working_extension_list, sizeof(working_extension_list), xmpf_misc->GetVersion());

    return &vgmstream_xmpin;
}

#if 0
// needed?
BOOL WINAPI DllMain(HINSTANCE hDLL, DWORD reason, LPVOID reserved) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hDLL);
            break;
    }
    return TRUE;
}
#endif
