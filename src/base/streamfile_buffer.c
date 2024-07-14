#include "../streamfile.h"
#include "../util/log.h"


typedef struct {
    STREAMFILE vt;

    STREAMFILE* inner_sf;
    offv_t offset;          /* last read offset (info) */
    offv_t buf_offset;      /* current buffer data start */
    uint8_t* buf;           /* data buffer */
    size_t buf_size;        /* max buffer size */
    size_t valid_size;      /* current buffer size */
    size_t file_size;       /* buffered file size */
} BUFFER_STREAMFILE;


static size_t buffer_read(BUFFER_STREAMFILE* sf, uint8_t* dst, offv_t offset, size_t length) {
    size_t read_total = 0;

    if (!dst || length <= 0 || offset < 0)
        return 0;

    /* is the part of the requested length in the buffer? */
    if (offset >= sf->buf_offset && offset < sf->buf_offset + sf->valid_size) {
        size_t buf_limit;
        int buf_into = (int)(offset - sf->buf_offset);

        buf_limit = sf->valid_size - buf_into;
        if (buf_limit > length)
            buf_limit = length;

        memcpy(dst, sf->buf + buf_into, buf_limit);
        read_total += buf_limit;
        length -= buf_limit;
        offset += buf_limit;
        dst += buf_limit;
    }

#ifdef VGM_DEBUG_OUTPUT
    if (offset < sf->buf_offset) {
        //VGM_LOG("buffer: rebuffer, requested %x vs %x (sf %x)\n", (uint32_t)offset, (uint32_t)sf->buf_offset, (uint32_t)sf);
    }
#endif

    /* read the rest of the requested length */
    while (length > 0) {
        size_t buf_limit;

        /* ignore requests at EOF */
        if (offset >= sf->file_size) {
            //offset = sf->file_size; /* seems fseek doesn't clamp offset */
            VGM_ASSERT_ONCE(offset > sf->file_size, "buffer: reading over file_size 0x%x @ 0x%x + 0x%x\n", sf->file_size, (uint32_t)offset, length);
            break;
        }

        /* fill the buffer (offset now is beyond buf_offset) */
        sf->buf_offset = offset;
        sf->valid_size = sf->inner_sf->read(sf->inner_sf, sf->buf, sf->buf_offset, sf->buf_size);

        /* decide how much must be read this time */
        if (length > sf->buf_size)
            buf_limit = sf->buf_size;
        else
            buf_limit = length;

        /* give up on partial reads (EOF) */
        if (sf->valid_size < buf_limit) {
            memcpy(dst, sf->buf, sf->valid_size);
            offset += sf->valid_size;
            read_total += sf->valid_size;
            break;
        }

        /* use the new buffer */
        memcpy(dst, sf->buf, buf_limit);
        offset += buf_limit;
        read_total += buf_limit;
        length -= buf_limit;
        dst += buf_limit;
    }

    sf->offset = offset; /* last fread offset */
    return read_total;
}

static size_t buffer_get_size(BUFFER_STREAMFILE* sf) {
    return sf->file_size; /* cache */
}

static offv_t buffer_get_offset(BUFFER_STREAMFILE* sf) {
    return sf->offset; /* cache */
}

static void buffer_get_name(BUFFER_STREAMFILE* sf, char* name, size_t name_size) {
    sf->inner_sf->get_name(sf->inner_sf, name, name_size); /* default */
}

static STREAMFILE* buffer_open(BUFFER_STREAMFILE* sf, const char* const filename, size_t buf_size) {
    STREAMFILE* new_inner_sf = sf->inner_sf->open(sf->inner_sf,filename,buf_size);
    return open_buffer_streamfile(new_inner_sf, buf_size); /* original buffer size is preferable? */
}

static void buffer_close(BUFFER_STREAMFILE* sf) {
    sf->inner_sf->close(sf->inner_sf);
    free(sf->buf);
    free(sf);
}


STREAMFILE* open_buffer_streamfile(STREAMFILE* sf, size_t buf_size) {
    BUFFER_STREAMFILE* this_sf = NULL;

    if (!sf) goto fail;

    if (buf_size == 0)
        buf_size = STREAMFILE_DEFAULT_BUFFER_SIZE;

    this_sf = calloc(1, sizeof(BUFFER_STREAMFILE));
    if (!this_sf) goto fail;

    /* set callbacks and internals */
    this_sf->vt.read = (void*)buffer_read;
    this_sf->vt.get_size = (void*)buffer_get_size;
    this_sf->vt.get_offset = (void*)buffer_get_offset;
    this_sf->vt.get_name = (void*)buffer_get_name;
    this_sf->vt.open = (void*)buffer_open;
    this_sf->vt.close = (void*)buffer_close;
    this_sf->vt.stream_index = sf->stream_index;

    this_sf->inner_sf = sf;
    this_sf->buf_size = buf_size;
    this_sf->buf = calloc(buf_size, sizeof(uint8_t));
    if (!this_sf->buf) goto fail;

    this_sf->file_size = sf->get_size(sf);

    return &this_sf->vt;

fail:
    if (this_sf) free(this_sf->buf);
    free(this_sf);
    return NULL;
}
STREAMFILE* open_buffer_streamfile_f(STREAMFILE* sf, size_t buffer_size) {
    STREAMFILE* new_sf = open_buffer_streamfile(sf, buffer_size);
    if (!new_sf)
        close_streamfile(sf);
    return new_sf;
}
