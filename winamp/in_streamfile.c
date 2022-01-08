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

/* a STREAMFILE that operates via STDIOSTREAMFILE but handles Winamp's unicode (in_char) paths */
typedef struct {
    STREAMFILE vt;
    STREAMFILE* stdiosf;
} WINAMP_STREAMFILE;

static STREAMFILE* open_winamp_streamfile_by_file(FILE* infile, const char* path);
//static STREAMFILE* open_winamp_streamfile_by_ipath(const in_char* wpath);

static size_t wasf_read(WINAMP_STREAMFILE* sf, uint8_t* dst, offv_t offset, size_t length) {
    return sf->stdiosf->read(sf->stdiosf, dst, offset, length);
}

static size_t wasf_get_size(WINAMP_STREAMFILE* sf) {
    return sf->stdiosf->get_size(sf->stdiosf);
}

static offv_t wasf_get_offset(WINAMP_STREAMFILE* sf) {
    return sf->stdiosf->get_offset(sf->stdiosf);
}

static void wasf_get_name(WINAMP_STREAMFILE* sf, char* buffer, size_t length) {
    sf->stdiosf->get_name(sf->stdiosf, buffer, length);
}

static STREAMFILE* wasf_open(WINAMP_STREAMFILE* sf, const char* const filename, size_t buffersize) {
    in_char wpath[PATH_LIMIT];

    if (!filename)
        return NULL;

    /* no need to wfdopen here, may use standard IO */
    /* STREAMFILEs carry char/UTF8 names, convert to wchar for Winamp */
    wa_char_to_ichar(wpath, PATH_LIMIT, filename);
    return open_winamp_streamfile_by_ipath(wpath);
}

static void wasf_close(WINAMP_STREAMFILE* sf) {
    /* closes infile_ref + frees in the internal STDIOSTREAMFILE (fclose for wchar is not needed) */
    sf->stdiosf->close(sf->stdiosf);
    free(sf); /* and the current struct */
}

static STREAMFILE* open_winamp_streamfile_by_file(FILE* file, const char* path) {
    WINAMP_STREAMFILE* this_sf = NULL;
    STREAMFILE* stdiosf = NULL;

    this_sf = calloc(1,sizeof(WINAMP_STREAMFILE));
    if (!this_sf) goto fail;

    stdiosf = open_stdio_streamfile_by_file(file, path);
    if (!stdiosf) goto fail;

    this_sf->vt.read = (void*)wasf_read;
    this_sf->vt.get_size = (void*)wasf_get_size;
    this_sf->vt.get_offset = (void*)wasf_get_offset;
    this_sf->vt.get_name = (void*)wasf_get_name;
    this_sf->vt.open = (void*)wasf_open;
    this_sf->vt.close = (void*)wasf_close;

    this_sf->stdiosf = stdiosf;

    return &this_sf->vt;

fail:
    close_streamfile(stdiosf);
    free(this_sf);
    return NULL;
}


STREAMFILE* open_winamp_streamfile_by_ipath(const in_char* wpath) {
    FILE* infile = NULL;
    STREAMFILE* sf;
    char path[PATH_LIMIT];


    /* convert to UTF-8 if needed for internal use */
    wa_ichar_to_char(path,PATH_LIMIT, wpath);

    /* open a FILE from a Winamp (possibly UTF-16) path */
    infile = wa_fopen(wpath);
    if (!infile) {
        /* allow non-existing files in some cases */
        if (!vgmstream_is_virtual_filename(path))
            return NULL;
    }

    sf = open_winamp_streamfile_by_file(infile, path);
    if (!sf) {
        if (infile) fclose(infile);
    }

    return sf;
}
