#include "api_internal.h"
/* STREAMFILE for internal use, that bridges calls to external libstreamfile_t */


static STREAMFILE* open_api_streamfile_internal(libstreamfile_t* libsf, bool external_libsf);


typedef struct {
    STREAMFILE vt;

    libstreamfile_t* libsf;
    bool external_libsf; //TODO: improve
} API_STREAMFILE;

static size_t api_read(API_STREAMFILE* sf, uint8_t* dst, offv_t offset, size_t length) {
    void* user_data = sf->libsf->user_data;

    return sf->libsf->read(user_data, dst, offset, length);
}

static size_t api_get_size(API_STREAMFILE* sf) {
    void* user_data = sf->libsf->user_data;

    return sf->libsf->get_size(user_data);
}

static offv_t api_get_offset(API_STREAMFILE* sf) {
    return 0; //sf->libsf->get_offset(sf->inner_sf); /* default */
}

static void api_get_name(API_STREAMFILE* sf, char* name, size_t name_size) {
    void* user_data = sf->libsf->user_data;

    if (!name || !name_size)
        return;
    name[0] = '\0';

    const char* external_name = sf->libsf->get_name(user_data);
    if (!external_name)
        return;

    snprintf(name, name_size, "%s", external_name);
    name[name_size - 1] = '\0';
}

static STREAMFILE* api_open(API_STREAMFILE* sf, const char* filename, size_t buf_size) {
    libstreamfile_t* libsf = sf->libsf->open(sf->libsf->user_data, filename);
    STREAMFILE* new_sf = open_api_streamfile_internal(libsf, false);

    if (!new_sf) {
        libstreamfile_close(libsf);
    }

    return new_sf;
}

static void api_close(API_STREAMFILE* sf) {
    if (sf && !sf->external_libsf) {
        sf->libsf->close(sf->libsf);
    }
    free(sf);
}

static STREAMFILE* open_api_streamfile_internal(libstreamfile_t* libsf, bool external_libsf) {
    API_STREAMFILE* this_sf = NULL;

    if (!libsf)
        return NULL;

    this_sf = calloc(1, sizeof(API_STREAMFILE));
    if (!this_sf) return NULL;

    /* set callbacks and internals */
    this_sf->vt.read = (void*)api_read;
    this_sf->vt.get_size = (void*)api_get_size;
    this_sf->vt.get_offset = (void*)api_get_offset;
    this_sf->vt.get_name = (void*)api_get_name;
    this_sf->vt.open = (void*)api_open;
    this_sf->vt.close = (void*)api_close;
    //this_sf->vt.stream_index = sf->stream_index;

    this_sf->libsf = libsf;
    this_sf->external_libsf = external_libsf;

    return &this_sf->vt;
}

STREAMFILE* open_api_streamfile(libstreamfile_t* libsf) {
    return open_api_streamfile_internal(libsf, true);
}
