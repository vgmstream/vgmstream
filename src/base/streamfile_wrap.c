#include "../streamfile.h"

//todo stream_index: copy? pass? funtion? external?
//todo use realnames on reopen? simplify?
//todo use safe string ops, this ain't easy

typedef struct {
    STREAMFILE vt;

    STREAMFILE* inner_sf;
} WRAP_STREAMFILE;

static size_t wrap_read(WRAP_STREAMFILE* sf, uint8_t* dst, offv_t offset, size_t length) {
    return sf->inner_sf->read(sf->inner_sf, dst, offset, length); /* default */
}
static size_t wrap_get_size(WRAP_STREAMFILE* sf) {
    return sf->inner_sf->get_size(sf->inner_sf); /* default */
}
static offv_t wrap_get_offset(WRAP_STREAMFILE* sf) {
    return sf->inner_sf->get_offset(sf->inner_sf); /* default */
}
static void wrap_get_name(WRAP_STREAMFILE* sf, char* name, size_t name_size) {
    sf->inner_sf->get_name(sf->inner_sf, name, name_size); /* default */
}

static STREAMFILE* wrap_open(WRAP_STREAMFILE* sf, const char* const filename, size_t buf_size) {
    return sf->inner_sf->open(sf->inner_sf, filename, buf_size); /* default (don't call open_wrap_streamfile) */
}

static void wrap_close(WRAP_STREAMFILE* sf) {
    //sf->inner_sf->close(sf->inner_sf); /* don't close */
    free(sf);
}

STREAMFILE* open_wrap_streamfile(STREAMFILE* sf) {
    WRAP_STREAMFILE* this_sf = NULL;

    if (!sf) return NULL;

    this_sf = calloc(1, sizeof(WRAP_STREAMFILE));
    if (!this_sf) return NULL;

    /* set callbacks and internals */
    this_sf->vt.read = (void*)wrap_read;
    this_sf->vt.get_size = (void*)wrap_get_size;
    this_sf->vt.get_offset = (void*)wrap_get_offset;
    this_sf->vt.get_name = (void*)wrap_get_name;
    this_sf->vt.open = (void*)wrap_open;
    this_sf->vt.close = (void*)wrap_close;
    this_sf->vt.stream_index = sf->stream_index;

    this_sf->inner_sf = sf;

    return &this_sf->vt;
}
STREAMFILE* open_wrap_streamfile_f(STREAMFILE* sf) {
    STREAMFILE* new_sf = open_wrap_streamfile(sf);
    if (!new_sf)
        close_streamfile(sf);
    return new_sf;
}
