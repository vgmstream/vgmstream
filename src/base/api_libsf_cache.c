#include "api_internal.h"

/* libstreamfile_t for external use, that caches some external libsf */

/* value can be adjusted freely but 8k is a good enough compromise. */
#define CACHE_DEFAULT_BUFFER_SIZE 0x8000

typedef struct {
    libstreamfile_t* libsf;

    int64_t offset;             /* last read offset (info) */
    int64_t buf_offset;         /* current buffer data start */
    uint8_t* buf;               /* data buffer */
    size_t buf_size;            /* max buffer size */
    size_t valid_size;          /* current buffer size */
    size_t file_size;           /* buffered file size */

    char name[PATH_LIMIT];
} cache_priv_t;

static int cache_read(void* user_data, uint8_t* dst, int dst_size) {
    cache_priv_t* priv = user_data;
    size_t read_total = 0;
    if (!dst || dst_size <= 0)
        return 0;

    /* is the part of the requested length in the buffer? */
    if (priv->offset >= priv->buf_offset && priv->offset < priv->buf_offset + priv->valid_size) {
        size_t buf_limit;
        int buf_into = (int)(priv->offset - priv->buf_offset);

        buf_limit = priv->valid_size - buf_into;
        if (buf_limit > dst_size)
            buf_limit = dst_size;

        memcpy(dst, priv->buf + buf_into, buf_limit);
        read_total += buf_limit;
        dst_size -= buf_limit;
        priv->offset += buf_limit;
        dst += buf_limit;
    }


    /* read the rest of the requested length */
    while (dst_size > 0) {
        size_t buf_limit;

        /* ignore requests at EOF */
        if (priv->offset >= priv->file_size) {
            //offset = priv->file_size; /* seems fseek doesn't clamp offset */
            //VGM_ASSERT_ONCE(offset > sf->file_size, "STDIO: reading over file_size 0x%x @ 0x%lx + 0x%x\n", sf->file_size, offset, length);
            break;
        }

        /* position to new offset */
        priv->libsf->seek(priv, priv->offset, 0);

        /* fill the buffer (offset now is beyond buf_offset) */
        priv->buf_offset = priv->offset;
        priv->valid_size = priv->libsf->read(priv, priv->buf, priv->buf_size);

        /* decide how much must be read this time */
        if (dst_size > priv->buf_size)
            buf_limit = priv->buf_size;
        else
            buf_limit = dst_size;

        /* give up on partial reads (EOF) */
        if (priv->valid_size < buf_limit) {
            memcpy(dst, priv->buf, priv->valid_size);
            priv->offset += priv->valid_size;
            read_total += priv->valid_size;
            break;
        }

        /* use the new buffer */
        memcpy(dst, priv->buf, buf_limit);
        priv->offset += buf_limit;
        read_total += buf_limit;
        dst_size -= buf_limit;
        dst += buf_limit;
    }

    return read_total;
}

static int64_t cache_seek(void* user_data, int64_t offset, int whence) {
    cache_priv_t* priv = user_data;

    switch (whence) {
        case LIBSTREAMFILE_SEEK_SET: /* absolute */
            break;
        case LIBSTREAMFILE_SEEK_CUR: /* relative to current */
            offset += priv->offset;
            break;
        case LIBSTREAMFILE_SEEK_END: /* relative to file end (should be negative) */
            offset += priv->file_size;
            break;
        default:
            break;
    }

    /* clamp offset like fseek */
    if (offset > priv->file_size)
        offset = priv->file_size;
    else if (offset < 0)
        offset = 0;

    /* main seek */
    priv->offset = offset;
    return 0;
}

static int64_t cache_get_size(void* user_data) {
    cache_priv_t* priv = user_data;
    return priv->file_size;
}

static const char* cache_get_name(void* user_data) {
    cache_priv_t* priv = user_data;
    return priv->name;
}

static libstreamfile_t* cache_open(void* user_data, const char* filename) {
    cache_priv_t* priv = user_data;
    if (!priv || !priv->libsf || !filename)
        return NULL;

    libstreamfile_t* inner_libsf = priv->libsf->open(priv->libsf->user_data, filename);
    if (!inner_libsf)
        return NULL;

    libstreamfile_t* libsf = libstreamfile_open_buffered(inner_libsf);
    if (!libsf) {
        libstreamfile_close(inner_libsf);
        return NULL;
    }

    return libsf;
}

static void cache_close(libstreamfile_t* libsf) {
    if (!libsf)
        return;

    cache_priv_t* priv = libsf->user_data;
    if (priv && priv->libsf) {
        libstreamfile_close(priv->libsf);
    }
    if (priv) {
        free(priv->buf);
    }
    free(priv);
    free(libsf);
}


LIBVGMSTREAM_API libstreamfile_t* libstreamfile_open_buffered(libstreamfile_t* ext_libsf) {
    if (!ext_libsf)
        return NULL;
    // not selectable since vgmstream's read patterns don't really fit one buf size
    int buf_size = CACHE_DEFAULT_BUFFER_SIZE;

    libstreamfile_t* libsf = NULL;
    cache_priv_t* priv = NULL;

    libsf = calloc(1, sizeof(libstreamfile_t));
    if (!libsf) goto fail;

    libsf->read = cache_read;
    libsf->seek = cache_seek;
    libsf->get_size = cache_get_size;
    libsf->get_name = cache_get_name;
    libsf->open = cache_open;
    libsf->close = cache_close;

    libsf->user_data = calloc(1, sizeof(cache_priv_t));
    if (!libsf->user_data) goto fail;

    priv = libsf->user_data;
    priv->libsf = ext_libsf;
    priv->buf_size = buf_size;
    priv->buf = calloc(buf_size, sizeof(uint8_t));
    if (!priv->buf) goto fail;

    priv->file_size = priv->libsf->get_size(priv->libsf->user_data);

    snprintf(priv->name, sizeof(priv->name), "%s", priv->libsf->get_name(priv->libsf->user_data));
    priv->name[sizeof(priv->name) - 1] = '\0';

    return libsf;
fail:
    cache_close(libsf);
    return NULL;
}
