#if 0
#include "api.h"
#include "vgmstream.h"

#define LIBVGMSTREAM_ERROR_GENERIC  -1
#define LIBVGMSTREAM_OK  0


LIBVGMSTREAM_API uint32_t libvgmstream_get_version(void) {
    return (LIBVGMSTREAM_API_VERSION_MAJOR << 24) | (LIBVGMSTREAM_API_VERSION_MINOR << 16) | (LIBVGMSTREAM_API_VERSION_PATCH << 0);
}


/* vgmstream context/handle */
typedef struct {
    
    libvgmstream_config_t cfg; // current config
    libvgmstream_format_t fmt; // format config
    libvgmstream_decoder_t dec; // decoder config
} libvgmstream_internal_t;



/* base init */
LIBVGMSTREAM_API libvgmstream_t* libvgmstream_init(void) {
    libvgmstream_t* lib = NULL;
    libvgmstream_internal_t* priv = NULL;

    lib = calloc(1, sizeof(libvgmstream_t));
    if (!lib) goto fail;

    lib->priv = calloc(1, sizeof(libvgmstream_internal_t));
    if (!lib->priv) goto fail;

    priv = lib->priv;

    //TODO only setup on decode? (but may less error prone if set)
    lib->format = &priv->fmt;
    lib->decoder = &priv->dec;

    return lib;
fail:
    libvgmstream_free(lib);
    return NULL;
}

/* base free */
LIBVGMSTREAM_API void libvgmstream_free(libvgmstream_t* lib) {
    if (!lib)
        return;

    //TODO close stuff in priv
    //if (lib->priv) {
    //    libvgmstream_internal_t* priv = lib->priv;
    //}

    free(lib->priv);
    free(lib);
}

/* current play config */
LIBVGMSTREAM_API void libvgmstream_setup(libvgmstream_t* lib, libvgmstream_config_t* cfg) {
    if (!lib)
        return;
    if (!cfg)
        return;
    libvgmstream_internal_t* priv = lib->priv;

    priv->cfg = *cfg;
    //TODO
}

/* open new file */
LIBVGMSTREAM_API int libvgmstream_open(libvgmstream_t* lib, libvgmstream_options_t* opt) {
    if (!lib)
        return LIBVGMSTREAM_ERROR_GENERIC;
    //if (!opt || !opt->sf || opt->subsong < 0)
    //    return LIBVGMSTREAM_ERROR_GENERIC;

    // close loaded song if any
    libvgmstream_close(lib);
    
    //format_internal_id

    // TODO open new vgmstream, save in priv

    return LIBVGMSTREAM_OK;
}

/* query new vgmstream, without closing current one */
LIBVGMSTREAM_API int libvgmstream_open_info(libvgmstream_t* lib, libvgmstream_options_t* opt, libvgmstream_format_t* fmt) {
    //TODO 
    return LIBVGMSTREAM_ERROR_GENERIC;
}

/* close current vgmstream */
LIBVGMSTREAM_API void libvgmstream_close(libvgmstream_t* lib) {
    //TODO close current vgmstream
}


/* decodes samples */
LIBVGMSTREAM_API int libvgmstream_play(libvgmstream_t* lib) {
    // TODO

    //if (!lib->decoder)
    //    ...

    lib->decoder->done = true;

    return LIBVGMSTREAM_OK;
}

/* vgmstream current decode position */
LIBVGMSTREAM_API int64_t libvgmstream_play_position(libvgmstream_t* lib) {
    // TODO

    return 0;
}

/* vgmstream seek */
LIBVGMSTREAM_API int libvgmstream_seek(libvgmstream_t* lib, int64_t sample) {
    return LIBVGMSTREAM_ERROR_GENERIC;
}


LIBVGMSTREAM_API int libvgmstream_format_describe(libvgmstream_format_t* format, char* dst, int dst_size) {
    return LIBVGMSTREAM_ERROR_GENERIC;
}

LIBVGMSTREAM_API bool libvgmstream_is_valid(const char* filename, libvgmstream_valid_t* cfg) {
    //TODO

    return false;
}


LIBVGMSTREAM_API void libvgmstream_get_title(libvgmstream_t* lib, libvgmstream_title_t* cfg, char* buf, int buf_len) {
    //TODO
}


LIBVGMSTREAM_API void libvgmstream_set_log(libvgmstream_log_t* log) {

}

/*
LIBVGMSTREAM_API libvgmstream_streamfile_t* libvgmstream_streamfile_get_file(const char* filename) {
    STREAMFILE* sf = open_stdio_streamfile(filename);
    //TODO make STREAMFILE compatible with libvgmstream_streamfile_t
    // would need bridge functions (since defs aren't compatible), not ideal since we are calling functions over functions
    // but basically not noticeable performance-wise
}
*/

#endif
