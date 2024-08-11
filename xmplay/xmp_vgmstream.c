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

#define SAMPLE_BUFFER_SIZE  1024

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
int ignore_loop = 0;
int disable_subsongs = 1;
BOOL xmplay_doneloop = 0;

/* plugin state */
VGMSTREAM* vgmstream = NULL;
int decode_pos_samples;
int length_samples = 0;
int output_channels;
int current_subsong = 0;
INT16 sample_buffer[SAMPLE_BUFFER_SIZE * VGMSTREAM_MAX_CHANNELS];

//XMPFILE current_file = NULL;
//char current_fn[XMPLAY_MAX_PATH] = {0};

static int shownerror = 0; /* init error */

/* ************************************* */

VGMSTREAM* init_vgmstream_xmplay(XMPFILE infile, const char* path, int subsong) {
    STREAMFILE* sf = NULL;
    VGMSTREAM* vgmstream = NULL;

    sf = open_xmplay_streamfile_by_xmpfile(infile, xmpf_file, path, false); /* external XMPFILE */
    if (!sf) return NULL;

    sf->stream_index = subsong;
    vgmstream = init_vgmstream_from_STREAMFILE(sf);
    if (!vgmstream) goto fail;

    return vgmstream;

fail:
    close_streamfile(sf);
    return NULL;
}

/* ************************************* */

static void apply_config(VGMSTREAM* vgmstream) {
    vgmstream_cfg_t vcfg = {0};

    vcfg.allow_play_forever = 0;
  //vcfg.play_forever = loop_forever;
    vcfg.loop_count = loop_count;
    vcfg.fade_time = fade_seconds;
    vcfg.fade_delay = fade_delay;
    vcfg.ignore_loop = ignore_loop;

    vgmstream_apply_config(vgmstream, &vcfg);
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
    VGMSTREAM* infostream = NULL;
    if (file)
        infostream = init_vgmstream_xmplay(file, filename, 0);
    else
        infostream = init_vgmstream(filename); //TODO: unicode problems?
    if (!infostream)
        return FALSE;

    close_vgmstream(infostream);

    return TRUE;
}

/* update info from a file, returning the number of subsongs */
DWORD WINAPI xmplay_GetFileInfo(const char *filename, XMPFILE file, float **length, char **tags) {
    VGMSTREAM* infostream;
    int subsong_count;

    if (file)
        infostream = init_vgmstream_xmplay(file, filename, 0);
    else
        infostream = init_vgmstream(filename); //TODO: unicode problems?
    if (!infostream)
        return 0;

    char temp_filepath[MAX_PATH];

    snprintf(temp_filepath, sizeof(temp_filepath), "%s", filename);
    temp_filepath[sizeof(temp_filepath) - 1] = '\0';

    apply_config(infostream);

    //vgmstream_mixing_autodownmix(infostream, downmix_channels);
    vgmstream_mixing_enable(infostream, 0, NULL, NULL);

    if (length && infostream->sample_rate) {
        int length_samples = vgmstream_get_samples(infostream);
        float *lens = (float*)xmpf_misc->Alloc(sizeof(float));
        lens[0] = (float)length_samples / (float)infostream->sample_rate;
        *length = lens;
    }

    subsong_count = infostream->num_streams;
    if (disable_subsongs || subsong_count == 0)
        subsong_count = 1;

    *tags = get_tags_from_filepath_info(infostream, xmpf_misc, temp_filepath);
    close_vgmstream(infostream);

    return subsong_count;
}

/* open a file for playback, returning:  0=failed, 1=success, 2=success and XMPlay can close the file */
DWORD WINAPI xmplay_Open(const char *filename, XMPFILE file) {
    if (file)
        vgmstream = init_vgmstream_xmplay(file, filename, current_subsong+1);
    else
        vgmstream = init_vgmstream(filename);
    if (!vgmstream)
        return 0;

    snprintf(last_filepath, sizeof(last_filepath), "%s", filename);
    last_filepath[sizeof(last_filepath) - 1] = '\0';

    apply_config(vgmstream);

    //vgmstream_mixing_autodownmix(vgmstream, downmix_channels);
    vgmstream_mixing_enable(vgmstream, SAMPLE_BUFFER_SIZE, NULL /*&input_channels*/, &output_channels);

    decode_pos_samples = 0;
    length_samples = vgmstream_get_samples(vgmstream);

    //strncpy(current_fn,filename,XMPLAY_MAX_PATH);
    //current_file = file;
    //current_subsong = 0;


    if (length_samples) {
        float length = (float)length_samples / (float)vgmstream->sample_rate;
        xmpf_in->SetLength(length, TRUE);
    }

    return 1;
}

/* close the playback file */
void WINAPI xmplay_Close() {
    close_vgmstream(vgmstream);
    vgmstream = NULL;
}

/* set the sample format */
void WINAPI xmplay_SetFormat(XMPFORMAT *form) {
    form->res = 16 / 8; /* PCM 16 */
    form->chan = output_channels;
    form->rate = vgmstream->sample_rate;
}

/* get tags, return NULL to delay title update (OPTIONAL) */
char* WINAPI xmplay_GetTags() {
    return get_tags_from_filepath_info(vgmstream, xmpf_misc, last_filepath);
}

/* main panel info text (short file info) */
void WINAPI xmplay_GetInfoText(char* format, char* length) {
    int rate, samples, bps;
    char fmt[128];
    int t, tmin, tsec;

    if (!format)
        return;
    if (!vgmstream)
        return;

    rate = vgmstream->sample_rate;
    samples = vgmstream->num_samples;
    bps = get_vgmstream_average_bitrate(vgmstream) / 1000;

    //get_vgmstream_coding_description(vgmstream, fmt, sizeof(fmt));
	//if (strcmp(fmt, "FFmpeg") == 0)
	{
		char buffer[1024];
		buffer[0] = '\0';
		describe_vgmstream(vgmstream, buffer, sizeof(buffer));
		char* enc_tmp = strstr(buffer, "encoding: ") + 10;
		char* enc_end = strstr(enc_tmp, "\n");
		int len = (int)enc_end - (int)enc_tmp;

		memset(fmt, 0x00, sizeof(fmt));
		strncpy(fmt, enc_tmp, len);
	}

    t = samples / rate;
    tmin = t / 60;
    tsec = t % 60;

    sprintf(format, "%s", fmt);
    sprintf(length, "%d:%02d - %dKb/s - %dHz", tmin, tsec, bps, rate);
}

/* info for the "General" window/tab (buf is ~40K) */
void WINAPI xmplay_GetGeneralInfo(char* buf) {
    int i;
    char description[1024];

    if (!buf)
        return;
    if (!vgmstream)
        return;

    description[0] = '\0';
    describe_vgmstream(vgmstream,description,sizeof(description));

    /* tags are divided with a tab and lines with carriage return so we'll do some guetto fixin' */
	int tag_done = 0;
	description[0] -= 32; //Replace small letter with capital for consistency with XMPlay's output
    for (i = 0; i < 1024; i++) {
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
    int32_t seek_sample = (int32_t)(pos * xmplay_GetGranularity() * vgmstream->sample_rate);

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

    seek_vgmstream(vgmstream, seek_sample);

    decode_pos_samples = seek_sample;

    cpos = (double)decode_pos_samples / vgmstream->sample_rate;
    return cpos;
}

/* decode some sample data */
DWORD WINAPI xmplay_Process(float* buf, DWORD bufsize) {
    int32_t i, done, samples_to_do;
    BOOL do_loop = xmpf_in->GetLooping();
    float *sbuf = buf;


    bufsize /= output_channels;

    samples_to_do = do_loop ? bufsize : length_samples - decode_pos_samples;
    if (samples_to_do > bufsize)
        samples_to_do = bufsize;

    /* decode */
    done = 0;
    while (done < samples_to_do) {
        int to_do = SAMPLE_BUFFER_SIZE;
        if (to_do > samples_to_do - done)
            to_do = samples_to_do - done;

        render_vgmstream(sample_buffer, to_do, vgmstream);

        for (i = 0; i < to_do * output_channels; i++) {
            *sbuf++ = sample_buffer[i] * 1.0f / 32768.0f;
        }

        done += to_do;
    }

    sbuf = buf;

    decode_pos_samples += done;

    return done * output_channels;
}

static DWORD WINAPI xmplay_GetSubSongs(float *length) {
    int subsong_count;

    if (!vgmstream)
        return 0;

    subsong_count = vgmstream->num_streams;
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
        length_samples = vgmstream_get_samples(vgmstream);
        *length = (float)length_samples / (float)vgmstream->sample_rate;
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
