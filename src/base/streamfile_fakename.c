#include "../streamfile.h"
#include "../util/vgmstream_limits.h"

typedef struct {
    STREAMFILE vt;

    STREAMFILE* inner_sf;
    char fakename[PATH_LIMIT];
    int fakename_len;
} FAKENAME_STREAMFILE;

static size_t fakename_read(FAKENAME_STREAMFILE* sf, uint8_t* dst, offv_t offset, size_t length) {
    return sf->inner_sf->read(sf->inner_sf, dst, offset, length); /* default */
}

static size_t fakename_get_size(FAKENAME_STREAMFILE* sf) {
    return sf->inner_sf->get_size(sf->inner_sf); /* default */
}

static offv_t fakename_get_offset(FAKENAME_STREAMFILE* sf) {
    return sf->inner_sf->get_offset(sf->inner_sf); /* default */
}

static void fakename_get_name(FAKENAME_STREAMFILE* sf, char* name, size_t name_size) {
    int copy_size = sf->fakename_len + 1;
    if (copy_size > name_size)
        copy_size = name_size;
    memcpy(name, sf->fakename, copy_size);
    name[copy_size - 1] = '\0';
}

static STREAMFILE* fakename_open(FAKENAME_STREAMFILE* sf, const char* const filename, size_t buf_size) {
    /* detect re-opening the file */
    if (strcmp(filename, sf->fakename) == 0) {
        STREAMFILE* new_inner_sf;
        char original_filename[PATH_LIMIT];

        sf->inner_sf->get_name(sf->inner_sf, original_filename, PATH_LIMIT);
        new_inner_sf = sf->inner_sf->open(sf->inner_sf, original_filename, buf_size);
        return open_fakename_streamfile(new_inner_sf, sf->fakename, NULL);
    }
    else {
        return sf->inner_sf->open(sf->inner_sf, filename, buf_size);
    }
}

static void fakename_close(FAKENAME_STREAMFILE* sf) {
    sf->inner_sf->close(sf->inner_sf);
    free(sf);
}


STREAMFILE* open_fakename_streamfile(STREAMFILE* sf, const char* fakename, const char* fakeext) {
    FAKENAME_STREAMFILE* this_sf = NULL;

    if (!sf || (!fakename && !fakeext)) return NULL;

    this_sf = calloc(1, sizeof(FAKENAME_STREAMFILE));
    if (!this_sf) return NULL;

    /* set callbacks and internals */
    this_sf->vt.read = (void*)fakename_read;
    this_sf->vt.get_size = (void*)fakename_get_size;
    this_sf->vt.get_offset = (void*)fakename_get_offset;
    this_sf->vt.get_name = (void*)fakename_get_name;
    this_sf->vt.open = (void*)fakename_open;
    this_sf->vt.close = (void*)fakename_close;
    this_sf->vt.stream_index = sf->stream_index;

    this_sf->inner_sf = sf;

    /* copy passed name or retain current, and swap extension if expected */
    if (fakename) {
        strcpy(this_sf->fakename, fakename);
    } else {
        sf->get_name(sf, this_sf->fakename, PATH_LIMIT);
    }

    if (fakeext) {
        char* ext = strrchr(this_sf->fakename, '.');
        if (ext != NULL) {
            ext[1] = '\0'; /* truncate past dot */
        } else {
            strcat(this_sf->fakename, "."); /* no extension = add dot */
        }
        strcat(this_sf->fakename, fakeext);
    }

    this_sf->fakename_len = strlen(this_sf->fakename);

    return &this_sf->vt;
}

STREAMFILE* open_fakename_streamfile_f(STREAMFILE* sf, const char* fakename, const char* fakeext) {
    STREAMFILE* new_sf = open_fakename_streamfile(sf, fakename, fakeext);
    if (!new_sf)
        close_streamfile(sf);
    return new_sf;
}
