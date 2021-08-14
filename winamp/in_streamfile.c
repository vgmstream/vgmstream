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

/* ************************************* */
/* IN_STREAMFILE                         */
/* ************************************* */

/* a STREAMFILE that operates via STDIOSTREAMFILE but handles Winamp's unicode (in_char) paths */
typedef struct {
    STREAMFILE sf;
    STREAMFILE *stdiosf;
    FILE *infile_ref; /* pointer to the infile in stdiosf (partially handled by stdiosf) */
} WINAMP_STREAMFILE;

static STREAMFILE *open_winamp_streamfile_by_file(FILE *infile, const char * path);
//static STREAMFILE *open_winamp_streamfile_by_ipath(const in_char *wpath);

static size_t wasf_read(WINAMP_STREAMFILE* sf, uint8_t* dest, off_t offset, size_t length) {
    return sf->stdiosf->read(sf->stdiosf, dest, offset, length);
}

static off_t wasf_get_size(WINAMP_STREAMFILE* sf) {
    return sf->stdiosf->get_size(sf->stdiosf);
}

static off_t wasf_get_offset(WINAMP_STREAMFILE* sf) {
    return sf->stdiosf->get_offset(sf->stdiosf);
}

static void wasf_get_name(WINAMP_STREAMFILE* sf, char* buffer, size_t length) {
    sf->stdiosf->get_name(sf->stdiosf, buffer, length);
}

static STREAMFILE *wasf_open(WINAMP_STREAMFILE* sf, const char* const filename, size_t buffersize) {
    in_char wpath[PATH_LIMIT];

    if (!filename)
        return NULL;

#if !defined (__ANDROID__) && !defined (_MSC_VER)
    /* When enabling this for MSVC it'll seemingly work, but there are issues possibly related to underlying
     * IO buffers when using dup(), noticeable by re-opening the same streamfile with small buffer sizes
     * (reads garbage). This reportedly causes issues in Android too */
    {
        char name[PATH_LIMIT];
        sf->stdiosf->get_name(sf->stdiosf, name, PATH_LIMIT);
        /* if same name, duplicate the file descriptor we already have open */ //unsure if all this is needed
        if (sf->infile_ref && !strcmp(name,filename)) {
            int new_fd;
            FILE *new_file;

            if (((new_fd = dup(fileno(sf->infile_ref))) >= 0) && (new_file = wa_fdopen(new_fd))) {
                STREAMFILE *new_sf = open_winamp_streamfile_by_file(new_file, filename);
                if (new_sf)
                    return new_sf;
                fclose(new_file);
            }
            if (new_fd >= 0 && !new_file)
                close(new_fd); /* fdopen may fail when opening too many files */

            /* on failure just close and try the default path (which will probably fail a second time) */
        }
    }
#endif

    /* STREAMFILEs carry char/UTF8 names, convert to wchar for Winamp */
    wa_char_to_ichar(wpath, PATH_LIMIT, filename);
    return open_winamp_streamfile_by_ipath(wpath);
}

static void wasf_close(WINAMP_STREAMFILE* sf) {
    /* closes infile_ref + frees in the internal STDIOSTREAMFILE (fclose for wchar is not needed) */
    sf->stdiosf->close(sf->stdiosf);
    free(sf); /* and the current struct */
}

static STREAMFILE *open_winamp_streamfile_by_file(FILE* file, const char* path) {
    WINAMP_STREAMFILE* this_sf = NULL;
    STREAMFILE* stdiosf = NULL;

    this_sf = calloc(1,sizeof(WINAMP_STREAMFILE));
    if (!this_sf) goto fail;

    stdiosf = open_stdio_streamfile_by_file(file, path);
    if (!stdiosf) goto fail;

    this_sf->sf.read = (void*)wasf_read;
    this_sf->sf.get_size = (void*)wasf_get_size;
    this_sf->sf.get_offset = (void*)wasf_get_offset;
    this_sf->sf.get_name = (void*)wasf_get_name;
    this_sf->sf.open = (void*)wasf_open;
    this_sf->sf.close = (void*)wasf_close;

    this_sf->stdiosf = stdiosf;
    this_sf->infile_ref = file;

    return &this_sf->sf; /* pointer to STREAMFILE start = rest of the custom data follows */

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

    sf = open_winamp_streamfile_by_file(infile,path);
    if (!sf) {
        if (infile) fclose(infile);
    }

    return sf;
}
