#include "../streamfile.h"
#include "../util/vgmstream_limits.h"

typedef struct {
    STREAMFILE vt;

    STREAMFILE* inner_sf;
    offv_t start;
    size_t size;
} CLAMP_STREAMFILE;

static size_t clamp_read(CLAMP_STREAMFILE* sf, uint8_t* dst, offv_t offset, size_t length) {
    offv_t inner_offset = sf->start + offset;
    size_t clamp_length = length;

    if (offset + length > sf->size) {
        if (offset >= sf->size)
            clamp_length = 0;
        else
            clamp_length = sf->size - offset;
    }

    return sf->inner_sf->read(sf->inner_sf, dst, inner_offset, clamp_length);
}

static size_t clamp_get_size(CLAMP_STREAMFILE* sf) {
    return sf->size;
}

static offv_t clamp_get_offset(CLAMP_STREAMFILE* sf) {
    return sf->inner_sf->get_offset(sf->inner_sf) - sf->start;
}

static void clamp_get_name(CLAMP_STREAMFILE* sf, char* name, size_t name_size) {
    sf->inner_sf->get_name(sf->inner_sf, name, name_size); /* default */
}

static STREAMFILE* clamp_open(CLAMP_STREAMFILE* sf, const char* const filename, size_t buf_size) {
    char original_filename[PATH_LIMIT];
    STREAMFILE* new_inner_sf = NULL;

    new_inner_sf = sf->inner_sf->open(sf->inner_sf,filename,buf_size);
    sf->inner_sf->get_name(sf->inner_sf, original_filename, PATH_LIMIT);

    /* detect re-opening the file */
    if (strcmp(filename, original_filename) == 0) {
        return open_clamp_streamfile(new_inner_sf, sf->start, sf->size); /* clamp again */
    } else {
        return new_inner_sf;
    }
}

static void clamp_close(CLAMP_STREAMFILE* sf) {
    sf->inner_sf->close(sf->inner_sf);
    free(sf);
}


STREAMFILE* open_clamp_streamfile(STREAMFILE* sf, offv_t start, size_t size) {
    CLAMP_STREAMFILE* this_sf = NULL;

    if (!sf || size == 0) return NULL;
    if (start + size > get_streamfile_size(sf)) return NULL;

    this_sf = calloc(1, sizeof(CLAMP_STREAMFILE));
    if (!this_sf) return NULL;

    /* set callbacks and internals */
    this_sf->vt.read = (void*)clamp_read;
    this_sf->vt.get_size = (void*)clamp_get_size;
    this_sf->vt.get_offset = (void*)clamp_get_offset;
    this_sf->vt.get_name = (void*)clamp_get_name;
    this_sf->vt.open = (void*)clamp_open;
    this_sf->vt.close = (void*)clamp_close;
    this_sf->vt.stream_index = sf->stream_index;

    this_sf->inner_sf = sf;
    this_sf->start = start;
    this_sf->size = size;

    return &this_sf->vt;
}

STREAMFILE* open_clamp_streamfile_f(STREAMFILE* sf, offv_t start, size_t size) {
    STREAMFILE* new_sf = open_clamp_streamfile(sf, start, size);
    if (!new_sf)
        close_streamfile(sf);
    return new_sf;
}
