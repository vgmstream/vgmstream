#include <stdio.h>
#include "xmp_vgmstream.h"

/* a SF that operates via XMPlay's XMPFUNC_FILE+XMPFILE */
typedef struct {
    XMPFILE infile;             // actual FILE
    XMPFUNC_FILE* xmpf_file;    // helper
    char name[0x4000];
    int64_t offset;
    int64_t size;
    bool internal_xmpfile;       // infile was not supplied externally and can be closed
} xmplay_priv_t;

static int xmpsf_read(void* user_data, uint8_t* dst, int64_t offset, int length) {
    xmplay_priv_t* priv = user_data;
    size_t read;

    if (priv->offset != offset) {
        if (priv->xmpf_file->Seek(priv->infile, offset))
            priv->offset = offset;
        else
            priv->offset = priv->xmpf_file->Tell(priv->infile);
    }

    read = priv->xmpf_file->Read(priv->infile, dst, length);
    if (read > 0)
        priv->offset += read;

    return read;
}

static int64_t xmpsf_get_size(void* user_data) {
    xmplay_priv_t* priv = user_data;
    return priv->size;
}

static const char* xmpsf_get_name(void* user_data) {
    xmplay_priv_t* priv = user_data;
    return priv->name;
}

static libstreamfile_t* xmpsf_open(void* user_data, const char* filename) {
    xmplay_priv_t* priv = user_data;

    if (!filename)
        return NULL;

    XMPFILE newfile = priv->xmpf_file->Open(filename);
    if (!newfile) return NULL;

    return open_xmplay_streamfile_by_xmpfile(newfile, priv->xmpf_file, filename, true); /* internal XMPFILE */
}

static void xmpsf_close(libstreamfile_t* libsf) {
    if (!libsf)
        return;

    xmplay_priv_t* priv = libsf->user_data;

    /* Close XMPFILE, but only if we opened it (ex. for subfiles inside metas).
     * Otherwise must be left open as other parts of XMPlay need it and would crash. */
    if (priv && priv->internal_xmpfile) {
        priv->xmpf_file->Close(priv->infile);
    }

    free(libsf);
}

libstreamfile_t* open_xmplay_streamfile_by_xmpfile(XMPFILE infile, XMPFUNC_FILE* xmpf_file, const char* path, bool internal) {

    xmplay_priv_t* priv = NULL;
    libstreamfile_t* libsf = calloc(1, sizeof(libstreamfile_t));
    if (!libsf) goto fail;

    libsf->read = xmpsf_read;
    libsf->get_size = xmpsf_get_size;
    libsf->get_name = xmpsf_get_name;
    libsf->open = xmpsf_open;
    libsf->close = xmpsf_close;

    libsf->user_data = calloc(1, sizeof(xmplay_priv_t));
    if (!libsf->user_data) goto fail;

    priv = libsf->user_data;
    priv->internal_xmpfile = internal;
    priv->infile = infile;
    priv->xmpf_file = xmpf_file;
    priv->size = priv->xmpf_file->GetSize(priv->infile);

    snprintf(priv->name, sizeof(priv->name), "%s", path);
    priv->name[sizeof(priv->name) - 1] = '\0';

    return libsf;
fail:
    xmpsf_close(libsf);
    return NULL;
}
