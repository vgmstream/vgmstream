#include "api_internal.h"
#if LIBVGMSTREAM_ENABLE

static libstreamfile_t* libstreamfile_from_streamfile(STREAMFILE* sf);

/* libstreamfile_t for external use, as a default implementation calling some internal SF */

typedef struct {
    int64_t offset;
    int64_t size;
    STREAMFILE* inner_sf;
    char name[PATH_LIMIT];
} libsf_data_t;

static int libsf_read(void* user_data, uint8_t* dst, int dst_size) {
    libsf_data_t* data = user_data;
    if (!data || !dst)
        return 0;

    int bytes = data->inner_sf->read(data->inner_sf, dst, data->offset, dst_size);
    data->offset += bytes;

    return bytes;
}

static int64_t libsf_seek(void* user_data, int64_t offset, int whence) {
    libsf_data_t* data = user_data;
    if (!data)
        return -1;

    switch (whence) {
        case LIBSTREAMFILE_SEEK_SET: /* absolute */
            break;
        case LIBSTREAMFILE_SEEK_CUR: /* relative to current */
            offset += data->offset;
            break;
        case LIBSTREAMFILE_SEEK_END: /* relative to file end (should be negative) */
            offset += data->size;
            break;
        default:
            break;
    }

    /* clamp offset like fseek */
    if (offset > data->size)
        offset = data->size;
    else if (offset < 0)
        offset = 0;

    /* main seek */
    data->offset = offset;
    return 0;
}

static int64_t libsf_get_size(void* user_data) {
    libsf_data_t* data = user_data;
    if (!data)
        return 0;
    return data->size;
}

static const char* libsf_get_name(void* user_data) {
    libsf_data_t* data = user_data;
    if (!data)
        return NULL;

    if (data->name[0] == '\0') {
        data->inner_sf->get_name(data->inner_sf, data->name, sizeof(data->name));
    }

    return data->name;
}

static libstreamfile_t* libsf_open(void* user_data, const char* filename) {
    libsf_data_t* data = user_data;
    if (!data || !data->inner_sf || !filename)
        return NULL;

    STREAMFILE* sf = data->inner_sf->open(data->inner_sf, filename, 0);
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

    libsf_data_t* data = libsf->user_data;
    if (data && data->inner_sf) {
        data->inner_sf->close(data->inner_sf);
    }
    free(data);
    free(libsf);
}

static libstreamfile_t* libstreamfile_from_streamfile(STREAMFILE* sf) {
    if (!sf)
        return NULL;

    libstreamfile_t* libsf = NULL;
    libsf_data_t* data = NULL;

    libsf = calloc(1, sizeof(libstreamfile_t));
    if (!libsf) goto fail;

    libsf->read = libsf_read;
    libsf->seek = libsf_seek;
    libsf->get_size = libsf_get_size;
    libsf->get_name = libsf_get_name;
    libsf->open = libsf_open;
    libsf->close = libsf_close;

    libsf->user_data = calloc(1, sizeof(libsf_data_t));
    if (!libsf->user_data) goto fail;

    data = libsf->user_data;
    data->inner_sf = sf;
    data->size = get_streamfile_size(sf);

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
#endif
