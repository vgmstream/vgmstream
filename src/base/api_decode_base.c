#include "api_internal.h"
#if LIBVGMSTREAM_ENABLE

#define INTERNAL_BUF_SAMPLES  1024


LIBVGMSTREAM_API uint32_t libvgmstream_get_version(void) {
    return (LIBVGMSTREAM_API_VERSION_MAJOR << 24) | (LIBVGMSTREAM_API_VERSION_MINOR << 16) | (LIBVGMSTREAM_API_VERSION_PATCH << 0);
}


LIBVGMSTREAM_API libvgmstream_t* libvgmstream_init(void) {
    libvgmstream_t* lib = NULL;
    libvgmstream_priv_t* priv = NULL;

    lib = calloc(1, sizeof(libvgmstream_t));
    if (!lib) goto fail;

    lib->priv = calloc(1, sizeof(libvgmstream_priv_t));
    if (!lib->priv) goto fail;

    priv = lib->priv;

    //TODO only setup on decode? (but may less error prone if always set)
    lib->format = &priv->fmt;
    lib->decoder = &priv->dec;

    priv->cfg.loop_count = 1; //TODO: loop 0 means no loop (improve detection)

    return lib;
fail:
    libvgmstream_free(lib);
    return NULL;
}


LIBVGMSTREAM_API void libvgmstream_free(libvgmstream_t* lib) {
    if (!lib)
        return;

    libvgmstream_priv_t* priv = lib->priv;
    if (priv) {
        close_vgmstream(priv->vgmstream);
        free(priv->buf.data);
    }

    free(priv);
    free(lib);
}


LIBVGMSTREAM_API void libvgmstream_setup(libvgmstream_t* lib, libvgmstream_config_t* cfg) {
    if (!lib || !lib->priv)
        return;

    libvgmstream_priv_t* priv = lib->priv;
    if (!cfg) {
        memset(&priv->cfg , 0, sizeof(libvgmstream_config_t));
        priv->cfg.loop_count = 1; //TODO: loop 0 means no loop (improve detection)
    }
    else {
        priv->cfg = *cfg;
    }

    //TODO validate, etc
}


void libvgmstream_priv_reset(libvgmstream_priv_t* priv, bool reset_buf) {
    //memset(&priv->cfg, 0, sizeof(libvgmstream_config_t)); //config is always valid
    memset(&priv->fmt, 0, sizeof(libvgmstream_format_t));
    memset(&priv->dec, 0, sizeof(libvgmstream_decoder_t));
    //memset(&priv->pos, 0, sizeof(libvgmstream_priv_position_t)); //position info is updated on open

    if (reset_buf) {
        free(priv->buf.data);
        memset(&priv->buf, 0, sizeof(libvgmstream_priv_buf_t));
    }

    priv->pos.current = 0;
    priv->decode_done = false;
}

#endif
