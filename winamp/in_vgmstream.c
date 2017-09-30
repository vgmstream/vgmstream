/**
 * vgmstream for Winamp
 */

/* Normally Winamp opens unicode files by their DOS 8.3 name. #define this to use wchar_t filenames,
 * which must be opened with _wfopen in a WINAMP_STREAMFILE (needed for dual files like .pos).
 * Only for Winamp paths, other parts would need #define UNICODE for Windows. */
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
#include "ipc_pe.h"
#include "resource.h"


#ifndef VERSION
#include "../version.h"
#endif
#ifndef VERSION
#define VERSION "(unknown version)"
#endif

#define PLUGIN_DESCRIPTION "vgmstream plugin " VERSION " " __DATE__

/* ************************************* */

/* config */
#define CONFIG_APP_NAME "vgmstream plugin"
#define CONFIG_INI_NAME "plugin.ini"

#define DEFAULT_FADE_SECONDS "10.00"
#define DEFAULT_FADE_DELAY_SECONDS "0.00"
#define DEFAULT_LOOP_COUNT "2.00"
#define DEFAULT_THREAD_PRIORITY 3
#define DEFAULT_LOOP_FOREVER 0
#define DEFAULT_IGNORE_LOOP 0
#define DEFAULT_DISABLE_SUBSONGS 0

#define FADE_SECONDS_INI_ENTRY "fade_seconds"
#define FADE_DELAY_SECONDS_INI_ENTRY "fade_delay"
#define LOOP_COUNT_INI_ENTRY "loop_count"
#define THREAD_PRIORITY_INI_ENTRY "thread_priority"
#define LOOP_FOREVER_INI_ENTRY "loop_forever"
#define IGNORE_LOOP_INI_ENTRY "ignore_loop"
#define DISABLE_SUBSONGS_INI_ENTRY "disable_subsongs"

char *priority_strings[] = {"Idle","Lowest","Below Normal","Normal","Above Normal","Highest (not recommended)","Time Critical (not recommended)"};
int priority_values[] = {THREAD_PRIORITY_IDLE,THREAD_PRIORITY_LOWEST,THREAD_PRIORITY_BELOW_NORMAL,THREAD_PRIORITY_NORMAL,THREAD_PRIORITY_ABOVE_NORMAL,THREAD_PRIORITY_HIGHEST,THREAD_PRIORITY_TIME_CRITICAL};

/* ************************************* */

/* plugin main (declared at the bottom of this file) */
In_Module input_module;
DWORD WINAPI __stdcall decode(void *arg);

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
int disable_subsongs;

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

in_char lastfn[PATH_LIMIT] = {0}; /* name of the currently playing file */

/* ************************************* */
//todo safe ops
//todo there must be a better way to handle unicode...
#ifdef UNICODE_INPUT_PLUGIN
#define wa_strcpy wcscpy
#define wa_strncpy wcsncpy
#define wa_strcat wcscat
#define wa_strlen wcslen
#define wa_strchr wcschr
#define wa_sscanf swscanf
#define wa_snprintf _snwprintf
#define wa_fileinfo fileinfoW
#define wa_IPC_PE_INSERTFILENAME IPC_PE_INSERTFILENAMEW
#define wa_L(x) L ##x
#else
#define wa_strcpy strcpy
#define wa_strncpy strncpy
#define wa_strcat strcat
#define wa_strlen strlen
#define wa_strchr strchr
#define wa_sscanf sscanf
#define wa_snprintf snprintf
#define wa_fileinfo fileinfo
#define wa_IPC_PE_INSERTFILENAME IPC_PE_INSERTFILENAME
#define wa_L(x) x
#endif

/* converts from utf16 to utf8 (if unicode is active) */
static void wa_wchar_to_char(char *dst, size_t dstsize, const in_char *wsrc) {
#ifdef UNICODE_INPUT_PLUGIN
    /* converto to UTF8 codepage, default separate bytes, source wstr, wstr lenght,  */
    //int size_needed = WideCharToMultiByte(CP_UTF8,0, src,-1, NULL,0, NULL, NULL);
    WideCharToMultiByte(CP_UTF8,0, wsrc,-1, dst,dstsize, NULL, NULL);
#else
    strcpy(dst,wsrc);
#endif
}

/* converts from utf8 to utf16 (if unicode is active) */
static void wa_char_to_wchar(in_char *wdst, size_t wdstsize, const char *src) {
#ifdef UNICODE_INPUT_PLUGIN
    //int size_needed = MultiByteToWideChar(CP_UTF8,0, src,-1, NULL,0);
    MultiByteToWideChar(CP_UTF8,0, src,-1, wdst,wdstsize);
#else
    strcpy(wdst,src);
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



/* a STREAMFILE that operates via STDIOSTREAMFILE but handles Winamp's unicode (in_char) paths */
typedef struct _WINAMP_STREAMFILE {
    STREAMFILE sf;
    STREAMFILE *stdiosf;
    FILE *infile_ref; /* pointer to the infile in stdiosf */
} WINAMP_STREAMFILE;

static STREAMFILE *open_winamp_streamfile_by_file(FILE *infile, const char * path);
static STREAMFILE *open_winamp_streamfile_by_wpath(const in_char *wpath);

static size_t wasf_read(WINAMP_STREAMFILE *streamfile, uint8_t *dest, off_t offset, size_t length) {
    return streamfile->stdiosf->read(streamfile->stdiosf,dest,offset,length);
}

static off_t wasf_get_size(WINAMP_STREAMFILE *streamfile) {
    return streamfile->stdiosf->get_size(streamfile->stdiosf);
}

static off_t wasf_get_offset(WINAMP_STREAMFILE *streamfile) {
    return streamfile->stdiosf->get_offset(streamfile->stdiosf);
}

static void wasf_get_name(WINAMP_STREAMFILE *streamfile, char *buffer, size_t length) {
    return streamfile->stdiosf->get_name(streamfile->stdiosf, buffer, length);
}

static void wasf_get_realname(WINAMP_STREAMFILE *streamfile, char *buffer, size_t length) {
    return streamfile->stdiosf->get_realname(streamfile->stdiosf, buffer, length);
}

static STREAMFILE *wasf_open(WINAMP_STREAMFILE *streamFile, const char *const filename, size_t buffersize) {
    int newfd;
    FILE *newfile;
    STREAMFILE *newstreamFile;
    in_char wpath[PATH_LIMIT];
    char name[PATH_LIMIT];

    if (!filename)
        return NULL;

    /* if same name, duplicate the file pointer we already have open */ //unsure if all this is needed
    streamFile->stdiosf->get_name(streamFile->stdiosf, name, PATH_LIMIT);
    if (!strcmp(name,filename)) {
        if (((newfd = dup(fileno(streamFile->infile_ref))) >= 0) &&
            (newfile = wa_fdopen(newfd)))
        {
            newstreamFile = open_winamp_streamfile_by_file(newfile,filename);
            if (newstreamFile) {
                return newstreamFile;
            }
            // failure, close it and try the default path (which will probably fail a second time)
            fclose(newfile);
        }
    }

    /* STREAMFILEs carry char/UTF8 names, convert to wchar for Winamp */
    wa_char_to_wchar(wpath,PATH_LIMIT, filename);
    return open_winamp_streamfile_by_wpath(wpath);
}

static void wasf_close(WINAMP_STREAMFILE *streamfile) {
    /* closes infile_ref + frees in the internal STDIOSTREAMFILE (fclose for wchar is not needed) */
    streamfile->stdiosf->close(streamfile->stdiosf);
    free(streamfile); /* and the current struct */
}

static STREAMFILE *open_winamp_streamfile_by_file(FILE *infile, const char * path) {
    WINAMP_STREAMFILE *streamfile = NULL;
    STREAMFILE *stdiosf = NULL;

    streamfile = calloc(1,sizeof(WINAMP_STREAMFILE));
    if (!streamfile) goto fail;

    stdiosf = open_stdio_streamfile_by_file(infile,path);
    if (!stdiosf) goto fail;

    streamfile->sf.read = (void*)wasf_read;
    streamfile->sf.get_size = (void*)wasf_get_size;
    streamfile->sf.get_offset = (void*)wasf_get_offset;
    streamfile->sf.get_name = (void*)wasf_get_name;
    streamfile->sf.get_realname = (void*)wasf_get_realname;
    streamfile->sf.open = (void*)wasf_open;
    streamfile->sf.close = (void*)wasf_close;

    streamfile->stdiosf = stdiosf;
    streamfile->infile_ref = infile;

    return &streamfile->sf; /* pointer to STREAMFILE start = rest of the custom data follows */

fail:
    close_streamfile(stdiosf);
    free(streamfile);
    return NULL;
}


static STREAMFILE *open_winamp_streamfile_by_wpath(const in_char *wpath) {
    FILE *infile = NULL;
    STREAMFILE *streamFile;
    char path[PATH_LIMIT];

    /* open a FILE from a Winamp (possibly UTF-16) path */
    infile = wa_fopen(wpath);
    if (!infile) return NULL;

    /* convert to UTF-8 if needed for internal use */
    wa_wchar_to_char(path,PATH_LIMIT, wpath);

    streamFile = open_winamp_streamfile_by_file(infile,path);
    if (!streamFile) {
        fclose(infile);
    }

    return streamFile;
}

/* opens vgmstream for winamp */
static VGMSTREAM* init_vgmstream_winamp(const in_char *fn, int stream_index) {
    VGMSTREAM * vgmstream = NULL;

    //return init_vgmstream(fn);

    /* manually init streamfile to pass the stream index */
    STREAMFILE *streamFile = open_winamp_streamfile_by_wpath(fn); //open_stdio_streamfile(fn);
    if (streamFile) {
        streamFile->stream_index = stream_index;
        vgmstream = init_vgmstream_from_STREAMFILE(streamFile);
        close_streamfile(streamFile);
    }

    return vgmstream;
}

/* ************************************* */

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


    if (disable_subsongs || vgmstream->num_streams <= 1 || (vgmstream->num_streams > 1 && stream_index > 0))
        return 0; /* no split if no subsongs or playing a subsong */

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

    /* autoplay doesn't always advance to the first unpacked track, manually fails too */
    //SendMessage(input_module.hMainWindow,WM_WA_IPC,playlist_index,IPC_SETPLAYLISTPOS);
    //SendMessage(input_module.hMainWindow,WM_WA_IPC,0,IPC_STARTPLAY);


    return 1;
}

/* parses a modified filename ('fakename') extracting tags parameters (NULL tag for first = filename) */
static int parse_fn_string(const in_char * fn, const in_char * tag, in_char * dst, int dst_size) {
    in_char *end;

    end = wa_strchr(fn,'|');
    if (tag==NULL) {
        wa_strcpy(dst,fn);
        if (end)
            dst[end - fn] = '\0';
        return 1;
    }

    //todo actually find + read tags
    dst[0] = '\0';
    return 0;
}
static int parse_fn_int(const in_char * fn, const in_char * tag, int * num) {
    in_char * start = wa_strchr(fn,'|');

    //todo actually find + read tags
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
    if (GetModuleHandle("xmplay.exe"))
        return 1;
    if (GetModuleHandle("xmp-wadsp.dll"))
        return 1;
    if (GetModuleHandle("xmp-wma.dll"))
        return 1;

    return 0;
}

/* ************************************* */

/* Winamp INI reader */
static void GetINIFileName(char * iniFile) {
    /* if we're running on a newer winamp version that better supports
     * saving of settings to a per-user directory, use that directory - if not
     * then just revert to the old behaviour */

    if(IsWindow(input_module.hMainWindow) && SendMessage(input_module.hMainWindow, WM_WA_IPC,0,IPC_GETVERSION) >= 0x5000) {
        char * iniDir = (char *)SendMessage(input_module.hMainWindow, WM_WA_IPC, 0, IPC_GETINIDIRECTORY);
        strncpy(iniFile, iniDir, PATH_LIMIT);

        strncat(iniFile, "\\Plugins\\", PATH_LIMIT);
        /* can't be certain that \Plugins already exists in the user dir */
        CreateDirectory(iniFile,NULL);
        strncat(iniFile, CONFIG_INI_NAME, PATH_LIMIT);
    }
    else {
        char * lastSlash;

        GetModuleFileName(NULL, iniFile, PATH_LIMIT);
        lastSlash = strrchr(iniFile, '\\');

        *(lastSlash + 1) = 0;
        strncat(iniFile, "Plugins\\" CONFIG_INI_NAME,PATH_LIMIT);
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
static void get_title(in_char * dst, int dst_size, const in_char * fn, VGMSTREAM * infostream) {
    in_char *basename;
    in_char buffer[PATH_LIMIT];
    in_char filename[PATH_LIMIT];
    int stream_index = 0;

    parse_fn_string(fn, NULL, filename,PATH_LIMIT);
    parse_fn_int(fn, wa_L("$s"), &stream_index);

    basename = (in_char*)filename + wa_strlen(filename); /* find end */
    while (*basename != '\\' && basename >= filename) /* and find last "\" */
        basename--;
    basename++;
    wa_strcpy(dst,basename);

    /* show stream subsong number */
    if (stream_index > 0) {
        wa_snprintf(buffer,PATH_LIMIT, wa_L("#%i"), stream_index);
        wa_strcat(dst,buffer);
    }

    /* show name, but not for the base stream */
    if (infostream && infostream->stream_name[0] != '\0' && stream_index > 0) {
        in_char stream_name[PATH_LIMIT];
        wa_char_to_wchar(stream_name, PATH_LIMIT, infostream->stream_name);
        wa_snprintf(buffer,PATH_LIMIT, wa_L(" (%s)"), stream_name);
        wa_strcat(dst,buffer);
    }
}

/* ***************************************** */

/* about dialog */
void winamp_About(HWND hwndParent) {
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
void winamp_Init() {
    char iniFile[PATH_LIMIT];
    char buf[256];
    int consumed;


    GetINIFileName(iniFile);

    thread_priority = GetPrivateProfileInt(CONFIG_APP_NAME,THREAD_PRIORITY_INI_ENTRY,DEFAULT_THREAD_PRIORITY,iniFile);
    if (thread_priority < 0 || thread_priority > 6) {
        sprintf(buf,"%d",DEFAULT_THREAD_PRIORITY);
        WritePrivateProfileString(CONFIG_APP_NAME,THREAD_PRIORITY_INI_ENTRY,buf,iniFile);
        thread_priority = DEFAULT_THREAD_PRIORITY;
    }

    GetPrivateProfileString(CONFIG_APP_NAME,FADE_SECONDS_INI_ENTRY,DEFAULT_FADE_SECONDS,buf,sizeof(buf),iniFile);
    if (sscanf(buf,"%lf%n",&fade_seconds,&consumed)<1 || consumed!=strlen(buf) || fade_seconds < 0) {
        WritePrivateProfileString(CONFIG_APP_NAME,FADE_SECONDS_INI_ENTRY,DEFAULT_FADE_SECONDS,iniFile);
        sscanf(DEFAULT_FADE_SECONDS,"%lf",&fade_seconds);
    }

    GetPrivateProfileString(CONFIG_APP_NAME,FADE_DELAY_SECONDS_INI_ENTRY,DEFAULT_FADE_DELAY_SECONDS,buf,sizeof(buf),iniFile);
    if (sscanf(buf,"%lf%n",&fade_delay_seconds,&consumed)<1 || consumed!=strlen(buf)) {
        WritePrivateProfileString(CONFIG_APP_NAME,FADE_DELAY_SECONDS_INI_ENTRY,DEFAULT_FADE_DELAY_SECONDS,iniFile);
        sscanf(DEFAULT_FADE_DELAY_SECONDS,"%lf",&fade_delay_seconds);
    }

    GetPrivateProfileString(CONFIG_APP_NAME,LOOP_COUNT_INI_ENTRY,DEFAULT_LOOP_COUNT,buf,sizeof(buf),iniFile);
    if (sscanf(buf,"%lf%n",&loop_count,&consumed)!=1 || consumed!=strlen(buf) || loop_count < 0) {
        WritePrivateProfileString(CONFIG_APP_NAME,LOOP_COUNT_INI_ENTRY,DEFAULT_LOOP_COUNT,iniFile);
        sscanf(DEFAULT_LOOP_COUNT,"%lf",&loop_count);
    }

    loop_forever = GetPrivateProfileInt(CONFIG_APP_NAME,LOOP_FOREVER_INI_ENTRY,DEFAULT_LOOP_FOREVER,iniFile);
    ignore_loop = GetPrivateProfileInt(CONFIG_APP_NAME,IGNORE_LOOP_INI_ENTRY,DEFAULT_IGNORE_LOOP,iniFile);
    if (loop_forever && ignore_loop) {
        sprintf(buf,"%d",DEFAULT_LOOP_FOREVER);
        WritePrivateProfileString(CONFIG_APP_NAME,LOOP_FOREVER_INI_ENTRY,buf,iniFile);
        loop_forever = DEFAULT_LOOP_FOREVER;

        sprintf(buf,"%d",DEFAULT_IGNORE_LOOP);
        WritePrivateProfileString(CONFIG_APP_NAME,IGNORE_LOOP_INI_ENTRY,buf,iniFile);
        ignore_loop = DEFAULT_IGNORE_LOOP;
    }

    disable_subsongs = GetPrivateProfileInt(CONFIG_APP_NAME,DISABLE_SUBSONGS_INI_ENTRY,DEFAULT_DISABLE_SUBSONGS,iniFile);
    //if (disable_subsongs < 0) {
    //    sprintf(buf,"%d",DEFAULT_DISABLE_SUBSONGS);
    //    WritePrivateProfileString(CONFIG_APP_NAME,DISABLE_SUBSONGS_INI_ENTRY,buf,iniFile);
    //    disable_subsongs = DEFAULT_DISABLE_SUBSONGS;
    //}

    /* XMPlay with in_vgmstream doesn't support most IPC_x messages so no playlist manipulation */
    if (is_xmplay()) {
        disable_subsongs = 1;
    }

    /* dynamically make a list of supported extensions */
    build_extension_list();
}

/* called at program quit */
void winamp_Quit() {
}

/* called before extension checks, to allow detection of mms://, etc */
int winamp_IsOurFile(const in_char *fn) {
    return 0; /* we don't recognize protocols */
}

/* request to start playing a file */
int winamp_Play(const in_char *fn) {
    int max_latency;
    in_char filename[PATH_LIMIT];
    int stream_index = 0;

    if (vgmstream)
        return 1; // TODO: this should either pop up an error box or close the file

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
    if (ignore_loop)
        vgmstream->loop_flag = 0;

    /* save original name */
    wa_strncpy(lastfn,fn,PATH_LIMIT);

    /* open the output plugin */
    max_latency = input_module.outMod->Open(vgmstream->sample_rate,vgmstream->channels, 16, 0, 0);
    if (max_latency < 0) {
        close_vgmstream(vgmstream);
        vgmstream = NULL;
        return 1;
    }

    /* set info display */ //TODO: actual bitrate
    input_module.SetInfo(get_vgmstream_average_bitrate(vgmstream)/1000,vgmstream->sample_rate/1000,vgmstream->channels,1);

    /* setup visualization */
    input_module.SAVSAInit(max_latency,vgmstream->sample_rate);
    input_module.VSASetInfo(vgmstream->sample_rate,vgmstream->channels);

    /* reset internals */
    decode_abort = 0;
    seek_needed_samples = -1;
    decode_pos_ms = 0;
    decode_pos_samples = 0;
    paused = 0;
    stream_length_samples = get_vgmstream_play_samples(loop_count,fade_seconds,fade_delay_seconds,vgmstream);
    fade_samples = (int)(fade_seconds * vgmstream->sample_rate);

    /* start */
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
void winamp_Pause() {
    paused = 1;
    input_module.outMod->Pause(1);
}

/* unpause stream */
void winamp_UnPause() {
    paused = 0;
    input_module.outMod->Pause(0);
}

/* return 1 if paused, 0 if not */
int winamp_IsPaused() {
    return paused;
}

/* stop (unload) stream */
void winamp_Stop() {
    if (decode_thread_handle != INVALID_HANDLE_VALUE) {
        decode_abort = 1;

        /* arbitrary wait length */
        if (WaitForSingleObject(decode_thread_handle,1000) == WAIT_TIMEOUT) {
            TerminateThread(decode_thread_handle,0); // TODO: error?
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
    return stream_length_samples * 1000LL / vgmstream->sample_rate;
}

/* current output time in ms */
int winamp_GetOutputTime() {
    return decode_pos_ms + (input_module.outMod->GetOutputTime()-input_module.outMod->GetWrittenTime());
}

/* seeks to point in stream (in ms) */
void winamp_SetOutputTime(int time_in_ms) {
    if (!vgmstream)
        return;

    seek_needed_samples = (long long)time_in_ms * vgmstream->sample_rate / 1000LL;
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

    concatn(sizeof(description),description,PLUGIN_DESCRIPTION "\n\n");

    if (!fn || !*fn) {
        /* no filename = current playing file */
        if (!vgmstream)
            return 0;

        describe_vgmstream(vgmstream,description,sizeof(description));
    }
    else {
        /* some other file in playlist given by filename */
        VGMSTREAM * infostream = NULL;
        in_char filename[PATH_LIMIT];
        int stream_index = 0;

        /* check for info encoded in the filename */
        parse_fn_string(fn, NULL, filename,PATH_LIMIT);
        parse_fn_int(fn, wa_L("$s"), &stream_index);

        infostream = init_vgmstream_winamp(filename, stream_index);
        if (!infostream)
            return 0;

        describe_vgmstream(infostream,description,sizeof(description));

        close_vgmstream(infostream);
        infostream = NULL;
    }

    MessageBox(hwnd,description,"Stream info",MB_OK);
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
        VGMSTREAM * infostream = NULL;
        in_char filename[PATH_LIMIT];
        int stream_index = 0;

        /* check for info encoded in the filename */
        parse_fn_string(fn, NULL, filename,PATH_LIMIT);
        parse_fn_int(fn, wa_L("$s"), &stream_index);

        infostream = init_vgmstream_winamp(filename, stream_index);
        if (!infostream) return;

        if (title) {
            get_title(title,GETFILEINFO_TITLE_LENGTH, fn, infostream);
        }

        if (length_in_ms) {
            *length_in_ms = -1000;
            if (infostream) {
                int num_samples = get_vgmstream_play_samples(loop_count,fade_seconds,fade_delay_seconds,infostream);
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

/* the decode thread */
DWORD WINAPI __stdcall decode(void *arg) {
    /* channel count shouldn't change during decode */
    int max_buffer_samples = sizeof(sample_buffer)/sizeof(sample_buffer[0])/2/vgmstream->channels;
    int max_samples = get_vgmstream_play_samples(loop_count,fade_seconds,fade_delay_seconds,vgmstream);

    while (!decode_abort) {
        int samples_to_do;
        int l;

        if (decode_pos_samples + max_buffer_samples > stream_length_samples
                && (!loop_forever || !vgmstream->loop_flag))
            samples_to_do = stream_length_samples - decode_pos_samples;
        else
            samples_to_do = max_buffer_samples;

        /* play 'till the end of this seek, or note if we're done seeking */
        if (seek_needed_samples != -1) {
            /* reset if we need to seek backwards */
            if (seek_needed_samples < decode_pos_samples) {
                reset_vgmstream(vgmstream);

                if (ignore_loop)
                    vgmstream->loop_flag = 0;

                decode_pos_samples = 0;
                decode_pos_ms = 0;
            }

            /* adjust seeking past file, can happen using the right (->) key
             * (should be done here and not in SetOutputTime due to threads/race condicions) */
            if (seek_needed_samples > max_samples) {
                seek_needed_samples = max_samples;
            }

            /* adjust max samples to seek */
            if (decode_pos_samples < seek_needed_samples) {
                samples_to_do = seek_needed_samples - decode_pos_samples;
                if (samples_to_do > max_buffer_samples)
                    samples_to_do = max_buffer_samples;
            }
            else {
                seek_needed_samples = -1;
            }

            input_module.outMod->Flush((int)decode_pos_ms);
        }

        l = (samples_to_do*vgmstream->channels*2) << (input_module.dsp_isactive()?1:0);

        if (samples_to_do == 0) {
            input_module.outMod->CanWrite();    /* ? */
            if (!input_module.outMod->IsPlaying()) {
                PostMessage(input_module.hMainWindow, WM_WA_MPEG_EOF, 0,0); /* end */
                return 0;
            }
            Sleep(10);
        }
        else if (seek_needed_samples != -1) {
            render_vgmstream(sample_buffer,samples_to_do,vgmstream);

            decode_pos_samples += samples_to_do;
            decode_pos_ms = decode_pos_samples * 1000LL / vgmstream->sample_rate;
        }
        else if (input_module.outMod->CanWrite() >= l) {
            /* let vgmstream do its thing */
            render_vgmstream(sample_buffer,samples_to_do,vgmstream);

            /* fade! */
            if (vgmstream->loop_flag && fade_samples > 0 && !loop_forever) {
                int samples_into_fade = decode_pos_samples - (stream_length_samples - fade_samples);
                if (samples_into_fade + samples_to_do > 0) {
                    int j,k;
                    for (j=0; j < samples_to_do; j++, samples_into_fade++) {
                        if (samples_into_fade > 0) {
                            double fadedness = (double)(fade_samples-samples_into_fade)/fade_samples;
                            for (k=0; k < vgmstream->channels; k++) {
                                sample_buffer[j*vgmstream->channels+k] =
                                    (short)(sample_buffer[j*vgmstream->channels+k]*fadedness);
                            }
                        }
                    }
                }
            }

            input_module.SAAddPCMData((char*)sample_buffer,vgmstream->channels,16,decode_pos_ms);
            input_module.VSAAddPCMData((char*)sample_buffer,vgmstream->channels,16,decode_pos_ms);
            decode_pos_samples += samples_to_do;
            decode_pos_ms = decode_pos_samples*1000LL/vgmstream->sample_rate;
            if (input_module.dsp_isactive())
                l = input_module.dsp_dosamples(sample_buffer,samples_to_do,16,vgmstream->channels,vgmstream->sample_rate) * 2 * vgmstream->channels;

            input_module.outMod->Write((char*)sample_buffer,l);
        }   /* if we can write enough */
        else {
            Sleep(20);
        }
    }   /* main loop */

    return 0;
}

/* config dialog handler */
INT_PTR CALLBACK configDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    char buf[256];
    char iniFile[PATH_LIMIT];
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

            if (disable_subsongs)
                CheckDlgButton(hDlg,IDC_DISABLE_SUBSONGS,BST_CHECKED);

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
                        WritePrivateProfileString(CONFIG_APP_NAME,THREAD_PRIORITY_INI_ENTRY,buf,iniFile);

                        fade_seconds = temp_fade_seconds;
                        sprintf(buf,"%.2lf",fade_seconds);
                        WritePrivateProfileString(CONFIG_APP_NAME,FADE_SECONDS_INI_ENTRY,buf,iniFile);

                        fade_delay_seconds = temp_fade_delay_seconds;
                        sprintf(buf,"%.2lf",fade_delay_seconds);
                        WritePrivateProfileString(CONFIG_APP_NAME,FADE_DELAY_SECONDS_INI_ENTRY,buf,iniFile);

                        loop_count = temp_loop_count;
                        sprintf(buf,"%.2lf",loop_count);
                        WritePrivateProfileString(CONFIG_APP_NAME,LOOP_COUNT_INI_ENTRY,buf,iniFile);

                        loop_forever = (IsDlgButtonChecked(hDlg,IDC_LOOP_FOREVER) == BST_CHECKED);
                        sprintf(buf,"%d",loop_forever);
                        WritePrivateProfileString(CONFIG_APP_NAME,LOOP_FOREVER_INI_ENTRY,buf,iniFile);

                        ignore_loop = (IsDlgButtonChecked(hDlg,IDC_IGNORE_LOOP) == BST_CHECKED);
                        sprintf(buf,"%d",ignore_loop);
                        WritePrivateProfileString(CONFIG_APP_NAME,IGNORE_LOOP_INI_ENTRY,buf,iniFile);

                        disable_subsongs = (IsDlgButtonChecked(hDlg,IDC_DISABLE_SUBSONGS) == BST_CHECKED);
                        sprintf(buf,"%d",disable_subsongs);
                        WritePrivateProfileString(CONFIG_APP_NAME,DISABLE_SUBSONGS_INI_ENTRY,buf,iniFile);
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

                    CheckDlgButton(hDlg,IDC_DISABLE_SUBSONGS,BST_UNCHECKED);
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
void winamp_Config(HWND hwndParent) {
    /* defined in resource.rc */
    DialogBox(input_module.hDllInstance, (const char *)IDD_CONFIG, hwndParent, configDlgProc);
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
    1, /* UsesOutputPlug flag */
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

__declspec( dllexport ) In_Module * winampGetInModule2() {
    return &input_module;
}
