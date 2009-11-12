// XMPlay input plugin header (c) 2004-2005 Ian Luck
// new plugins can be submitted to plugins@xmplay.com

#include <wtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef XMPIN_FACE
#define XMPIN_FACE 1 // "face"
#endif

typedef void *XMPFILE;

#define XMPIN_FLAG_CANSTREAM    1 // can stream files (play while downloading from the 'net)
#define XMPIN_FLAG_OWNFILE              2 // can process files without "XMPFILE" routines
#define XMPIN_FLAG_NOXMPFILE    4 // never use "XMPFILE" routines (implies XMPIN_FLAG_OWNFILE)

// Note all texts are UTF-8 on WinNT based systems, and ANSI on Win9x
#define Utf2Uni(src,slen,dst,dlen) MultiByteToWideChar(CP_UTF8,0,src,slen,dst,dlen) // convert UTF-8 to Unicode

typedef struct {
        DWORD flags; // XMPIN_FLAG_xxx
        char *name; // plugin name
        char *exts; // supported file extensions (description\0ext1/ext2/etc...)

        void (WINAPI *About)(HWND win); // (OPTIONAL)
        void (WINAPI *Config)(HWND win); // present config options to user (OPTIONAL)
        BOOL (WINAPI *CheckFile)(char *filename, BYTE *buf, DWORD length); // verify file
        BOOL (WINAPI *GetFileInfo)(char *filename, XMPFILE file, float *length, char *tags[7]); // get track info
                //tags: 0=title,1=artist,2=album,3=year,4=track,5=genre,6=comment

        // playback stuff
        DWORD (WINAPI *Open)(char *filename, XMPFILE file); // open a file
        void (WINAPI *Close)(); // close file
        DWORD (WINAPI *GetFormat)(DWORD *chan, DWORD *res); // return sample rate (OPTIONAL: mutually exclusive with SetFormat)
        void (WINAPI *SetFormat)(DWORD rate, DWORD chan); // (OPTIONAL: mutually exclusive with GetFormat)
        BOOL (WINAPI *GetTags)(char *tags[7]); // get title elements, return TRUE to delay (OPTIONAL)
        void (WINAPI *GetInfoText)(char *format, char *length); // get main panel info text
        void (WINAPI *GetGeneralInfo)(char *buf); // get General info window text
        void (WINAPI *GetMessage)(char *buf); // get Message info text (OPTIONAL)
        double (WINAPI *SetPosition)(DWORD pos); // seek (pos=0x800000nn=subsong nn)
        double (WINAPI *GetGranularity)(); // seeking granularity
        DWORD (WINAPI *GetBuffering)(); // get buffering progress (OPTIONAL)
        DWORD (WINAPI *Process)(float *buf, DWORD count); // decode some sample data
        BOOL (WINAPI *WriteFile)(char *filename); // write file to disk (OPTIONAL)

#if XMPIN_FACE>=1 // "face 1" additions
        void (WINAPI *GetSamples)(char *buf); // get Samples info text (OPTIONAL)
        DWORD (WINAPI *GetSubSongs)(float *length); // get number (and total length) of sub-songs (OPTIONAL)
#endif
} XMPIN;


#define XMPFILE_TYPE_MEMORY             0 // file in memory
#define XMPFILE_TYPE_FILE               1 // local file
#define XMPFILE_TYPE_NETFILE    2 // file on the 'net
#define XMPFILE_TYPE_NETSTREAM  3 // 'net stream (indeterminate length)

#define XMPCONFIG_NET_BUFFER    0
#define XMPCONFIG_NET_RESTRICT  1
#define XMPCONFIG_NET_RECONNECT 2
#define XMPCONFIG_NET_NOPROXY   3

#define XMPINFO_MAIN                    1 // main window info area
#define XMPINFO_GENERAL                 2 // General info window
#define XMPINFO_MESSAGE                 4 // Message info window
#define XMPINFO_SAMPLES                 8 // Samples info window

typedef struct {
        struct { // file functions
                DWORD (WINAPI *GetType)(XMPFILE file); // return XMPFILE_TYPE_xxx
                DWORD (WINAPI *GetSize)(XMPFILE file); // file size
                void *(WINAPI *GetMemory)(XMPFILE file); // memory location (XMPFILE_TYPE_MEMORY)
                DWORD (WINAPI *Read)(XMPFILE file, void *buf, DWORD len); // read from file
                BOOL (WINAPI *Seek)(XMPFILE file, DWORD pos); // seek in file
                DWORD (WINAPI *Tell)(XMPFILE file); // get current file pos
                // net-only stuff
                void (WINAPI *NetSetRate)(XMPFILE file, DWORD rate); // set bitrate in bytes/sec (decides buffer size)
                BOOL (WINAPI *NetIsActive)(XMPFILE file); // connection is still up?
                BOOL (WINAPI *NetPreBuf)(XMPFILE file); // pre-buffer data
                DWORD (WINAPI *NetAvailable)(XMPFILE file); // get amount of data ready to go
#if XMPIN_FACE>=1
                // additional file opening stuff
                XMPFILE (WINAPI *Open)(char *filename); // open a file (local file/archive only, no 'net)
                void (WINAPI *Close)(XMPFILE file); // close an opened file
#endif
        } file;

        struct { // tag functions - return new strings in native form (UTF8/ANSI)
                char *(WINAPI *Ansi)(char *tag, int len); // ANSI string (len=-1=null terminated)
                char *(WINAPI *Unicode)(WCHAR *tag, int len); // Unicode string
                char *(WINAPI *Utf8)(char *tag, int len); // UTF-8 string
        } tag;

        DWORD (WINAPI *GetConfig)(DWORD option); // get a config (XMPCONFIG_xxx) value
        BOOL (WINAPI *CheckCancel)(); // user wants to cancel?
        void (WINAPI *SetLength)(float length, BOOL seekable); // set track length (-1=unchanged) and if it's seekable
        void (WINAPI *SetGain)(DWORD mode, float gain); // set replaygain (mode 0=track, 1=album)
        BOOL (WINAPI *UpdateTitle)(char *track); // set track title (NULL=refresh tags/title)
#if XMPIN_FACE>=1
        void (WINAPI *RefreshInfo)(DWORD mode); // refresh info displays (XMPINFO_xxx flags)
#endif
} XMPIN_FUNCS;


#ifdef __cplusplus
}
#endif
