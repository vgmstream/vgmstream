#include "api_internal.h"

static libstreamfile_t* libstreamfile_from_streamfile(STREAMFILE* sf);

/* libstreamfile_t for external use, as a default implementation calling some internal SF */

typedef struct {
    int64_t offset;
    int64_t size;
    STREAMFILE* sf;
    char name[PATH_LIMIT];
} libsf_priv_t;

static int libsf_read(void* user_data, uint8_t* dst, int dst_size) {
    libsf_priv_t* priv = user_data;
    if (!priv || !dst)
        return 0;

    int bytes = priv->sf->read(priv->sf, dst, priv->offset, dst_size);
    priv->offset += bytes;

    return bytes;
}

static int64_t libsf_seek(void* user_data, int64_t offset, int whence) {
    libsf_priv_t* priv = user_data;
    if (!priv)
        return -1;

    switch (whence) {
        case LIBSTREAMFILE_SEEK_SET: /* absolute */
            break;
        case LIBSTREAMFILE_SEEK_CUR: /* relative to current */
            offset += priv->offset;
            break;
        case LIBSTREAMFILE_SEEK_END: /* relative to file end (should be negative) */
            offset += priv->size;
            break;
        default:
            break;
    }

    /* clamp offset like fseek */
    if (offset > priv->size)
        offset = priv->size;
    else if (offset < 0)
        offset = 0;

    /* main seek */
    priv->offset = offset;
    return 0;
}

static int64_t libsf_get_size(void* user_data) {
    libsf_priv_t* priv = user_data;
    if (!priv)
        return 0;
    return priv->size;
}

static const char* libsf_get_name(void* user_data) {
    libsf_priv_t* priv = user_data;
    if (!priv)
        return NULL;

    if (priv->name[0] == '\0') {
        priv->sf->get_name(priv->sf, priv->name, sizeof(priv->name));
    }

    return priv->name;
}

static libstreamfile_t* libsf_open(void* user_data, const char* filename) {
    libsf_priv_t* priv = user_data;
    if (!priv || !priv->sf || !filename)
        return NULL;

    STREAMFILE* sf = priv->sf->open(priv->sf, filename, 0);
    if (!sf)
        return NULL;

    libstreamfile_t* libsf = libstreamfile_from_streamfile(sf);
    if (!libsf) {
        close_streamfile(sf);
        return NULL;
    }

    return libsf;
}

static void libsf_close(libstreamfile_t* libsf) {
    if (!libsf)
        return;

    libsf_priv_t* priv = libsf->user_data;
    if (priv && priv->sf) {
        priv->sf->close(priv->sf);
    }
    free(priv);
    free(libsf);
}

static libstreamfile_t* libstreamfile_from_streamfile(STREAMFILE* sf) {
    if (!sf)
        return NULL;

    libstreamfile_t* libsf = NULL;
    libsf_priv_t* priv = NULL;

    libsf = calloc(1, sizeof(libstreamfile_t));
    if (!libsf) goto fail;

    libsf->read = libsf_read;
    libsf->seek = libsf_seek;
    libsf->get_size = libsf_get_size;
    libsf->get_name = libsf_get_name;
    libsf->open = libsf_open;
    libsf->close = libsf_close;

    libsf->user_data = calloc(1, sizeof(libsf_priv_t));
    if (!libsf->user_data) goto fail;

    priv = libsf->user_data;
    priv->sf = sf;
    priv->size = get_streamfile_size(sf);

    return libsf;
fail:
    libsf_close(libsf);
    return NULL;
}


LIBVGMSTREAM_API libstreamfile_t* libstreamfile_open_from_stdio(const char* filename) {
    STREAMFILE* sf = open_stdio_streamfile(filename);
    if (!sf)
        return NULL;

    libstreamfile_t* libsf = libstreamfile_from_streamfile(sf);
    if (!libsf) {
        close_streamfile(sf);
        return NULL;
    }

    return libsf;
}

LIBVGMSTREAM_API libstreamfile_t* libstreamfile_open_from_file(void* file_, const char* filename) {
    FILE* file = file_;
    STREAMFILE* sf = open_stdio_streamfile_by_file(file, filename);
    if (!sf)
        return NULL;

    libstreamfile_t* libsf = libstreamfile_from_streamfile(sf);
    if (!libsf) {
        close_streamfile(sf);
        return NULL;
    }

    return libsf;
}
