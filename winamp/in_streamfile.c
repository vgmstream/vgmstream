/**
 * streamfile for Winamp
 */
#include <stdio.h>
#include <io.h>
#include "in_vgmstream.h"


/* ************************************* */
/* IN_UNICODE                            */
/* ************************************* */

/* opens a utf16 (unicode) path */
static FILE* wa_fopen(const in_char* wpath) {
#ifdef UNICODE_INPUT_PLUGIN
    return _wfopen(wpath, L"rb");
#else
    return fopen(wpath, "rb");
#endif
}

/* in theory fdopen and _wfdopen are the same, except flags is a wchar */
#if 0
/* dupes a utf16 (unicode) file */
static FILE* wa_fdopen(int fd) {
#ifdef UNICODE_INPUT_PLUGIN
    return _wfdopen(fd,L"rb");
#else
    return fdopen(fd,"rb");
#endif
}
#endif

/* ************************************* */
/* IN_STREAMFILE                         */
/* ************************************* */

/* a SF that operates via STDIO but handles Winamp's unicode (in_char) paths */

libstreamfile_t* open_winamp_streamfile_by_ipath(const in_char* wpath);

static int wa_read(void* user_data, uint8_t* dst, int64_t offset, int length) {
    libstreamfile_t* sf = user_data;
    return sf->read(sf->user_data, dst, offset, length);
}

static int64_t wa_get_size(void* user_data) {
    libstreamfile_t* sf = user_data;
    return sf->get_size(sf->user_data);
}

static const char* wa_get_name(void* user_data) {
    libstreamfile_t* sf = user_data;
    return sf->get_name(sf->user_data);
}

static libstreamfile_t* wa_open(void* user_data, const char* const filename) {
    in_char wpath[WINAMP_PATH_LIMIT];

    if (!filename)
        return NULL;

    /* no need to wfdopen here, may use standard IO */
    /* STREAMFILEs carry char/UTF8 names, convert to wchar for Winamp */
    wa_char_to_ichar(wpath, WINAMP_PATH_LIMIT, filename);
    return open_winamp_streamfile_by_ipath(wpath);
}

static void wa_close(libstreamfile_t* libsf) {
    if (!libsf)
        return;

    libstreamfile_t* sf = libsf->user_data;
    if (sf) {
        sf->close(sf); // fclose for wchar is not needed
    }
    free(libsf);
}

static libstreamfile_t* open_winamp_streamfile_by_file(FILE* file, const char* path) {
    libstreamfile_t* libsf = NULL;

    libsf = calloc(1, sizeof(libstreamfile_t));
    if (!libsf) goto fail;

    libsf->read = wa_read;
    libsf->get_size = wa_get_size;
    libsf->get_name = wa_get_name;
    libsf->open = wa_open;
    libsf->close = wa_close;

    libsf->user_data = libstreamfile_open_from_file(file, path);
    if (!libsf->user_data) goto fail;

    return libsf;
fail:
    wa_close(libsf);
    return NULL;
}

libstreamfile_t* open_winamp_streamfile_by_ipath(const in_char* wpath) {
    FILE* infile = NULL;
    libstreamfile_t* sf;
    char path[WINAMP_PATH_LIMIT];

    /* convert to UTF-8 if needed for internal use */
    wa_ichar_to_char(path, WINAMP_PATH_LIMIT, wpath);

    /* open a FILE from a Winamp (possibly UTF-16) path */
    infile = wa_fopen(wpath);
    if (!infile) {
        /* allow non-existing files in some cases */
        if (!libvgmstream_is_virtual_filename(path))
            return NULL;
    }

    sf = open_winamp_streamfile_by_file(infile, path);
    if (!sf) {
        if (infile)
            fclose(infile);
    }

    return sf;
}
