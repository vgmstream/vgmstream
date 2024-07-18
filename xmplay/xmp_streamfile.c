#include "xmp_vgmstream.h"

/* a STREAMFILE that operates via XMPlay's XMPFUNC_FILE+XMPFILE */
typedef struct _XMPLAY_STREAMFILE {
    STREAMFILE sf;              /* callbacks */
    XMPFILE infile;             /* actual FILE */
    XMPFUNC_FILE* xmpf_file;    /* helper */
    char name[0x8000];          /* path limit */
    off_t offset;               /* current offset */
    int internal_xmpfile;       /* infile was not supplied externally and can be closed */
} XMPLAY_STREAMFILE;


static size_t xmpsf_read(XMPLAY_STREAMFILE* sf, uint8_t* dst, offv_t offset, size_t length) {
    size_t read;

    if (sf->offset != offset) {
        if (sf->xmpf_file->Seek(sf->infile, offset))
            sf->offset = offset;
        else
            sf->offset = sf->xmpf_file->Tell(sf->infile);
    }

    read = sf->xmpf_file->Read(sf->infile, dst, length);
    if (read > 0)
        sf->offset += read;

    return read;
}

static off_t xmpsf_get_size(XMPLAY_STREAMFILE* sf) {
    return sf->xmpf_file->GetSize(sf->infile);
}

static off_t xmpsf_get_offset(XMPLAY_STREAMFILE* sf) {
    return sf->xmpf_file->Tell(sf->infile);
}

static void xmpsf_get_name(XMPLAY_STREAMFILE* sf, char* buffer, size_t length) {
    snprintf(buffer, length, "%s", sf->name);
    buffer[length - 1] = '\0';
}

static STREAMFILE* xmpsf_open(XMPLAY_STREAMFILE* sf, const char* const filename, size_t buffersize) {
    XMPFILE newfile;

    if (!filename)
        return NULL;

    newfile = sf->xmpf_file->Open(filename);
    if (!newfile) return NULL;

    return open_xmplay_streamfile_by_xmpfile(newfile, sf->xmpf_file, filename, true); /* internal XMPFILE */
}

static void xmpsf_close(XMPLAY_STREAMFILE* sf) {
    /* Close XMPFILE, but only if we opened it (ex. for subfiles inside metas).
     * Otherwise must be left open as other parts of XMPlay need it and would crash. */
    if (sf->internal_xmpfile) {
        sf->xmpf_file->Close(sf->infile);
    }

    free(sf);
}

STREAMFILE* open_xmplay_streamfile_by_xmpfile(XMPFILE infile, XMPFUNC_FILE* xmpf_file, const char* path, bool internal) {
    XMPLAY_STREAMFILE* this_sf = calloc(1, sizeof(XMPLAY_STREAMFILE));
    if (!this_sf) return NULL;

    this_sf->sf.read = (void*)xmpsf_read;
    this_sf->sf.get_size = (void*)xmpsf_get_size;
    this_sf->sf.get_offset = (void*)xmpsf_get_offset;
    this_sf->sf.get_name = (void*)xmpsf_get_name;
    this_sf->sf.open = (void*)xmpsf_open;
    this_sf->sf.close = (void*)xmpsf_close;
    this_sf->infile = infile;
    this_sf->offset = 0;

    snprintf(this_sf->name, sizeof(this_sf->name), "%s", path);
    this_sf->name[sizeof(this_sf->name) - 1] = '\0';

    this_sf->internal_xmpfile = internal;
    this_sf->xmpf_file = xmpf_file;

    return &this_sf->sf; /* pointer to STREAMFILE start = rest of the custom data follows */
}
