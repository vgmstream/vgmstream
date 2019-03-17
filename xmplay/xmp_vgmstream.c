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

#include "../src/vgmstream.h"
#include "xmpin.h"


#ifndef VERSION
#include "version.h"
#endif
#ifndef VERSION
#define VERSION "(unknown version)"
#endif

/* ************************************* */

/* XMPlay extension list, only needed to associate extensions in Windows */
/*  todo: as of v3.8.2.17, any more than ~1000 will crash XMplay's file list screen (but not using the non-native Winamp plugin...) */
#define EXTENSION_LIST_SIZE   1000 /* (0x2000 * 2) */
#define XMPLAY_MAX_PATH  32768

/* XMPlay function library */
static XMPFUNC_IN *xmpfin;
static XMPFUNC_MISC *xmpfmisc;
static XMPFUNC_FILE *xmpffile;

char working_extension_list[EXTENSION_LIST_SIZE] = {0};

/* plugin config */
double fade_seconds = 10.0;
double fade_delay_seconds = 10.0;
double loop_count = 2.0;
int disable_subsongs = 1;

/* plugin state */
VGMSTREAM * vgmstream = NULL;
int framesDone, framesLength;
int stream_length_samples = 0;
int fade_samples = 0;

int current_subsong = 0;
//XMPFILE current_file = NULL;
//char current_fn[XMPLAY_MAX_PATH] = {0};

static int shownerror = 0; /* init error */

/* ************************************* */

/* a STREAMFILE that operates via XMPlay's XMPFUNC_FILE+XMPFILE */
typedef struct _XMPLAY_STREAMFILE {
    STREAMFILE sf;          /* callbacks */
    XMPFILE infile;         /* actual FILE */
    char name[PATH_LIMIT];
    off_t offset;           /* current offset */
    int internal_xmpfile;   /* infile was not supplied externally and can be closed */
} XMPLAY_STREAMFILE;

static STREAMFILE *open_xmplay_streamfile_by_xmpfile(XMPFILE file, const char *path, int internal);

static size_t xmpsf_read(XMPLAY_STREAMFILE *this, uint8_t *dest, off_t offset, size_t length) {
    size_t read;

    if (this->offset != offset) {
        if (xmpffile->Seek(this->infile, offset))
            this->offset = offset;
        else
            this->offset = xmpffile->Tell(this->infile);
    }

    read = xmpffile->Read(this->infile, dest, length);
    if (read > 0)
        this->offset += read;

    return read;
}

static off_t xmpsf_get_size(XMPLAY_STREAMFILE *this) {
    return xmpffile->GetSize(this->infile);
}

static off_t xmpsf_get_offset(XMPLAY_STREAMFILE *this) {
    return xmpffile->Tell(this->infile);
}

static void xmpsf_get_name(XMPLAY_STREAMFILE *this, char *buffer, size_t length) {
    strncpy(buffer, this->name, length);
    buffer[length-1] = '\0';
}

static STREAMFILE *xmpsf_open(XMPLAY_STREAMFILE *this, const char *const filename, size_t buffersize) {
    XMPFILE newfile;

    if (!filename)
        return NULL;

    newfile = xmpffile->Open(filename);
    if (!newfile) return NULL;

    return open_xmplay_streamfile_by_xmpfile(newfile, filename, 1); /* internal XMPFILE */
}

static void xmpsf_close(XMPLAY_STREAMFILE *this) {
    /* Close XMPFILE, but only if we opened it (ex. for subfiles inside metas).
     * Otherwise must be left open as other parts of XMPlay need it and would crash. */
    if (this->internal_xmpfile) {
        xmpffile->Close(this->infile);
    }

    free(this);
}

static STREAMFILE *open_xmplay_streamfile_by_xmpfile(XMPFILE infile, const char *path, int internal) {
    XMPLAY_STREAMFILE *streamfile = calloc(1,sizeof(XMPLAY_STREAMFILE));
    if (!streamfile) return NULL;

    streamfile->sf.read = (void*)xmpsf_read;
    streamfile->sf.get_size = (void*)xmpsf_get_size;
    streamfile->sf.get_offset = (void*)xmpsf_get_offset;
    streamfile->sf.get_name = (void*)xmpsf_get_name;
    streamfile->sf.open = (void*)xmpsf_open;
    streamfile->sf.close = (void*)xmpsf_close;
    streamfile->infile = infile;
    streamfile->offset = 0;
    strncpy(streamfile->name, path, sizeof(streamfile->name));

    streamfile->internal_xmpfile = internal;

    return &streamfile->sf; /* pointer to STREAMFILE start = rest of the custom data follows */
}

VGMSTREAM *init_vgmstream_xmplay(XMPFILE file, const char *path, int subsong) {
    STREAMFILE *streamfile = NULL;
    VGMSTREAM *vgmstream = NULL;

    streamfile = open_xmplay_streamfile_by_xmpfile(file, path, 0); /* external XMPFILE */
    if (!streamfile) return NULL;

    streamfile->stream_index = subsong;
    vgmstream = init_vgmstream_from_STREAMFILE(streamfile);
    if (!vgmstream) goto fail;

    return vgmstream;

fail:
    xmpsf_close((XMPLAY_STREAMFILE *)streamfile);
    return NULL;
}

/* ************************************* */

#if 0
/* get the tags as an array of "key\0value\0", NULL-terminated */
static char *get_tags(VGMSTREAM * infostream) {
    char *tags;
    size_t tag_number = 20; // ?

    tags = (char*)xmpfmisc->Alloc(tag_number+1);

    for (...) {
        ...
    }

    tags[tag_number]=0; // terminating NULL
    return tags; /* assuming XMPlay free()s this, since it Alloc()s it */
}
#endif

/* Adds ext to XMPlay's extension list. */
static int add_extension(int length, char * dst, const char * ext) {
    int ext_len;
    int i;

    if (length <= 1)
        return 0;

    ext_len = strlen(ext);

    /* check if end reached or not enough room to add */
    if (ext_len+2 > length-2) {
        dst[0]='\0';
        return 0;
    }

    /* copy new extension + null terminate */
    for (i=0; i < ext_len; i++)
        dst[i] = ext[i];
    dst[i]='/';
    dst[i+1]='\0';
    return i+1;
}

/* Creates XMPlay's extension list, a single string with 2 nulls.
 * Extensions must be in this format: "Description\0extension1/.../extensionN" */
static void build_extension_list() {
    const char ** ext_list;
    size_t ext_list_len;
    int i, written;

    written = sprintf(working_extension_list, "%s%c", "vgmstream files",'\0');

    ext_list = vgmstream_get_formats(&ext_list_len);

    for (i=0; i < ext_list_len; i++) {
        written += add_extension(EXTENSION_LIST_SIZE-written, working_extension_list + written, ext_list[i]);
    }
    working_extension_list[written-1] = '\0'; /* remove last "/" */
}

/* ************************************* */

/* info for the "about" button in plugin options */
void WINAPI xmplay_About(HWND win) {
    MessageBox(win,
            "vgmstream plugin " VERSION " " __DATE__ "\n"
            "by hcs, FastElbja, manakoAT, bxaimc, snakemeat, soneek, kode54, bnnm and many others\n"
            "\n"
            "XMPlay plugin by unknownfile, PSXGamerPro1, kode54\n"
            "\n"
            "https://github.com/kode54/vgmstream/\n"
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

    if (length && infostream->sample_rate) {
        int stream_length_samples = get_vgmstream_play_samples(loop_count, fade_seconds, fade_delay_seconds, infostream);
        float *lens = (float*)xmpfmisc->Alloc(sizeof(float));
        lens[0] = (float)stream_length_samples / (float)infostream->sample_rate;
        *length = lens;
    }

    subsong_count = infostream->num_streams;
    if (disable_subsongs || subsong_count == 0)
        subsong_count = 1;

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

    framesDone = 0;
    stream_length_samples = get_vgmstream_play_samples(loop_count, fade_seconds, fade_delay_seconds, vgmstream);
    fade_samples = (int)(fade_seconds * vgmstream->sample_rate);
    framesLength = stream_length_samples - fade_samples;

    //strncpy(current_fn,filename,XMPLAY_MAX_PATH);
    //current_file = file;
    //current_subsong = 0;


    if (stream_length_samples) {
        float length = (float)stream_length_samples / (float)vgmstream->sample_rate;
        xmpfin->SetLength(length, TRUE);
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
    form->chan = vgmstream->channels;
    form->rate = vgmstream->sample_rate;
}

/* get tags, return NULL to delay title update (OPTIONAL) */
char * WINAPI xmplay_GetTags() {
    return NULL; //get_tags(vgmstream);
}

/* main panel info text (short file info) */
void WINAPI xmplay_GetInfoText(char* format, char* length) {
    int rate, samples, bps;
    const char* fmt;
    int t, tmin, tsec;

    if (!format)
        return;
    if (!vgmstream)
        return;

    rate = vgmstream->sample_rate;
    samples = vgmstream->num_samples;
    bps = get_vgmstream_average_bitrate(vgmstream) / 1000;
    fmt = get_vgmstream_coding_description(vgmstream->coding_type);

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
    for (i = 0; i < 1024; i++) {
        int tag_done = 0;

        if (description[i] == '\0')
            break;

        if (description[i] == ':' && !tag_done) { /* to ignore multiple ':' in a line*/
            description[i] = ' ';
            description[i+1] = '\t';
            tag_done = 1;
        }

        if (description[i] == '\n') {
            description[i] = '\r';
            tag_done = 0;
        }
    }

    sprintf(buf,"vgmstream\t\r%s\r", description);
}

/* get the seeking granularity in seconds */
double WINAPI xmplay_GetGranularity() {
    return 0.001; /* can seek in milliseconds */
}

/* seek to a position (in granularity units), return new position or -1 = failed */
double WINAPI xmplay_SetPosition(DWORD pos) {
    double cpos = (double)framesDone / (double)vgmstream->sample_rate;
    double time = pos * xmplay_GetGranularity();

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

    if (time < cpos) {
        reset_vgmstream(vgmstream);
        cpos = 0.0;
    }

    while (cpos < time) {
        INT16 buffer[1024];
        long max_sample_count = 1024 / vgmstream->channels;
        long samples_to_skip = (long)((time - cpos) * vgmstream->sample_rate);
        if (samples_to_skip > max_sample_count)
            samples_to_skip = max_sample_count;
        if (!samples_to_skip)
            break;
        render_vgmstream(buffer, (int)samples_to_skip, vgmstream);
        cpos += (double)samples_to_skip / (double)vgmstream->sample_rate;
    }

    framesDone = (int32_t)(cpos * vgmstream->sample_rate);

    return cpos;
}

/* decode some sample data */
DWORD WINAPI xmplay_Process(float* buf, DWORD bufsize) {
    INT16 sample_buffer[1024];
    UINT32 i, j, todo, done;

    BOOL doLoop = xmpfin->GetLooping();
    float *sbuf = buf;
    UINT32 samplesTodo;

    bufsize /= vgmstream->channels;

    samplesTodo = doLoop ? bufsize : stream_length_samples - framesDone;
    if (samplesTodo > bufsize)
        samplesTodo = bufsize;

    /* decode */
    done = 0;
    while (done < samplesTodo) {
        todo = 1024 / vgmstream->channels;
        if (todo > samplesTodo - done)
            todo = samplesTodo - done;

        render_vgmstream(sample_buffer, todo, vgmstream);

        for (i = 0, j = todo * vgmstream->channels; i < j; ++i) {
            *sbuf++ = sample_buffer[i] * 1.0f / 32768.0f;
        }
        done += todo;
    }

    sbuf = buf;

    /* fade */
    if (!doLoop && framesDone + done > framesLength) {
        long fadeStart = (framesLength > framesDone) ? framesLength : framesDone;
        long fadeEnd = (framesDone + done) > stream_length_samples ? stream_length_samples : (framesDone + done);
        long fadePos;

        float fadeScale = (float)(stream_length_samples - fadeStart) / fade_samples;
        float fadeStep = 1.0f / fade_samples;

        sbuf += (fadeStart - framesDone) * vgmstream->channels;
        j = vgmstream->channels;

        for (fadePos = fadeStart; fadePos < fadeEnd; ++fadePos) {
            for (i = 0; i < j; ++i) {
                sbuf[i] = sbuf[i] * fadeScale;
            }
            sbuf += j;

            fadeScale -= fadeStep;
            if (fadeScale <= 0.0f)
                break;
        }
        done = (int)(fadePos - framesDone);
    }

    framesDone += done;

    return done * vgmstream->channels;
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
        int stream_length_samples;

        /* not good for vgmstream as would mean re-parsing many times */
        //int i;
        //for (i = 0; i < subsong_count; i++) {
        //    float subsong_length = ...
        //    *length += subsong_length;
        //}

        /* simply use the current length */ //todo just use 0?
        stream_length_samples = get_vgmstream_play_samples(loop_count, fade_seconds, fade_delay_seconds, vgmstream);
        *length = (float)stream_length_samples / (float)vgmstream->sample_rate;
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

    xmpfin = (XMPFUNC_IN*)faceproc(XMPFUNC_IN_FACE);
    xmpfmisc = (XMPFUNC_MISC*)faceproc(XMPFUNC_MISC_FACE);
    xmpffile = (XMPFUNC_FILE*)faceproc(XMPFUNC_FILE_FACE);

    build_extension_list();

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
