#include "../streamfile.h"

typedef struct {
    STREAMFILE vt;

    STREAMFILE* inner_sf;
    void* data; /* state for custom reads, malloc'ed + copied on open (to re-open streamfiles cleanly) */
    size_t data_size;
    size_t (*read_callback)(STREAMFILE*, uint8_t*, off_t, size_t, void*); /* custom read to modify data before copying into buffer */
    size_t (*size_callback)(STREAMFILE*, void*); /* size when custom reads make data smaller/bigger than underlying streamfile */
    int (*init_callback)(STREAMFILE*, void*); /* init the data struct members somehow, return >= 0 if ok */
    void (*close_callback)(STREAMFILE*, void*); /* close the data struct members somehow */
    /* read doesn't use offv_t since callbacks would need to be modified */
} IO_STREAMFILE;

static size_t io_read(IO_STREAMFILE* sf, uint8_t* dst, offv_t offset, size_t length) {
    return sf->read_callback(sf->inner_sf, dst, (off_t)offset, length, sf->data);
}

static size_t io_get_size(IO_STREAMFILE* sf) {
    if (sf->size_callback)
        return sf->size_callback(sf->inner_sf, sf->data);
    else
        return sf->inner_sf->get_size(sf->inner_sf); /* default */
}

static offv_t io_get_offset(IO_STREAMFILE* sf) {
    return sf->inner_sf->get_offset(sf->inner_sf);  /* default */
}

static void io_get_name(IO_STREAMFILE* sf, char* name, size_t name_size) {
    sf->inner_sf->get_name(sf->inner_sf, name, name_size); /* default */
}

static STREAMFILE* io_open(IO_STREAMFILE* sf, const char* const filename, size_t buf_size) {
    STREAMFILE* new_inner_sf = sf->inner_sf->open(sf->inner_sf,filename,buf_size);
    return open_io_streamfile_ex(new_inner_sf, sf->data, sf->data_size, sf->read_callback, sf->size_callback, sf->init_callback, sf->close_callback);
}

static void io_close(IO_STREAMFILE* sf) {
    if (sf->close_callback)
        sf->close_callback(sf->inner_sf, sf->data);
    sf->inner_sf->close(sf->inner_sf);
    free(sf->data);
    free(sf);
}


STREAMFILE* open_io_streamfile_ex(STREAMFILE* sf, void* data, size_t data_size, void* read_callback, void* size_callback, void* init_callback, void* close_callback) {
    IO_STREAMFILE* this_sf = NULL;

    if (!sf) goto fail;
    if ((data && !data_size) || (!data && data_size)) goto fail;

    this_sf = calloc(1, sizeof(IO_STREAMFILE));
    if (!this_sf) goto fail;

    /* set callbacks and internals */
    this_sf->vt.read = (void*)io_read;
    this_sf->vt.get_size = (void*)io_get_size;
    this_sf->vt.get_offset = (void*)io_get_offset;
    this_sf->vt.get_name = (void*)io_get_name;
    this_sf->vt.open = (void*)io_open;
    this_sf->vt.close = (void*)io_close;
    this_sf->vt.stream_index = sf->stream_index;

    this_sf->inner_sf = sf;
    if (data) {
        this_sf->data = malloc(data_size);
        if (!this_sf->data) goto fail;
        memcpy(this_sf->data, data, data_size);
    }
    this_sf->data_size = data_size;
    this_sf->read_callback = read_callback;
    this_sf->size_callback = size_callback;
    this_sf->init_callback = init_callback;
    this_sf->close_callback = close_callback;

    if (this_sf->init_callback) {
        int ok = this_sf->init_callback(this_sf->inner_sf, this_sf->data);
        if (ok < 0) goto fail;
    }

    return &this_sf->vt;

fail:
    if (this_sf) free(this_sf->data);
    free(this_sf);
    return NULL;
}

STREAMFILE* open_io_streamfile_ex_f(STREAMFILE* sf, void* data, size_t data_size, void* read_callback, void* size_callback, void* init_callback, void* close_callback) {
    STREAMFILE* new_sf = open_io_streamfile_ex(sf, data, data_size, read_callback, size_callback, init_callback, close_callback);
    if (!new_sf)
        close_streamfile(sf);
    return new_sf;
}

STREAMFILE* open_io_streamfile(STREAMFILE* sf, void* data, size_t data_size, void* read_callback, void* size_callback) {
    return open_io_streamfile_ex(sf, data, data_size, read_callback, size_callback, NULL, NULL);
}
STREAMFILE* open_io_streamfile_f(STREAMFILE* sf, void* data, size_t data_size, void* read_callback, void* size_callback) {
    return open_io_streamfile_ex_f(sf, data, data_size, read_callback, size_callback, NULL, NULL);
}
