#include "api_internal.h"
#include "mixing.h"



LIBVGMSTREAM_API uint32_t libvgmstream_get_version(void) {
    return (LIBVGMSTREAM_API_VERSION_MAJOR << 24) | (LIBVGMSTREAM_API_VERSION_MINOR << 16) | (LIBVGMSTREAM_API_VERSION_PATCH << 0);
}

LIBVGMSTREAM_API libvgmstream_t* libvgmstream_create(libstreamfile_t* libsf, int subsong, libvgmstream_config_t* vcfg) {
    libvgmstream_t* vgmstream = libvgmstream_init();
    libvgmstream_setup(vgmstream, vcfg);
    int err = libvgmstream_open_stream(vgmstream, libsf, subsong);
    if (err < 0) {
        libvgmstream_free(vgmstream);
        return NULL;
    }

    return vgmstream;
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

// TODO: allow calling after load

LIBVGMSTREAM_API void libvgmstream_setup(libvgmstream_t* lib, libvgmstream_config_t* cfg) {
    if (!lib || !lib->priv)
        return;

    libvgmstream_priv_t* priv = lib->priv;

    // Can only apply once b/c some options modify the internal mixing chain and there is no clean way to
    // reset the stream when txtp also manipulates it (maybe could add some flag per mixing item)
    if (priv->setup_done)
        return;

    // allow overwritting current config, though will only apply to next load
    //if (priv->config_loaded)
    //    return;

    if (!cfg) {
        memset(&priv->cfg , 0, sizeof(libvgmstream_config_t));
        priv->config_loaded = false;
    }
    else {

        priv->cfg = *cfg;
        priv->config_loaded = true;
    }

    //TODO validate, etc (for now most incorrect values are ignored)

    // apply now if possible to update format info
    if (priv->vgmstream) {
        api_apply_config(priv);
    }
}


void libvgmstream_priv_reset(libvgmstream_priv_t* priv, bool full) {
    //memset(&priv->cfg, 0, sizeof(libvgmstream_config_t)); //config is always valid
    //memset(&priv->pos, 0, sizeof(libvgmstream_priv_position_t)); //position info is updated on open
    memset(&priv->dec, 0, sizeof(libvgmstream_decoder_t));
    
    if (full) {
        free(priv->buf.data); //TODO 
        memset(&priv->buf, 0, sizeof(libvgmstream_priv_buf_t));
        memset(&priv->fmt, 0, sizeof(libvgmstream_format_t));
    }
    else {
        priv->buf.consumed = priv->buf.samples;
    }

    priv->pos.current = 0;
    priv->decode_done = false;
}

libvgmstream_sfmt_t api_get_output_sample_type(libvgmstream_priv_t* priv) {
    VGMSTREAM* v = priv->vgmstream;
    sfmt_t format = mixing_get_output_sample_type(v);
    switch(format) {
        case SFMT_S16: return LIBVGMSTREAM_SFMT_PCM16;
        case SFMT_FLT: return LIBVGMSTREAM_SFMT_FLOAT;
        case SFMT_S32: return LIBVGMSTREAM_SFMT_PCM32;
        case SFMT_O24: return LIBVGMSTREAM_SFMT_PCM24;
         
        // internal use only, shouldn't happen (misconfigured, see prepare_mixing)
        case SFMT_S24:
        case SFMT_F16:
        default:
            return 0x00;
    }
}

int api_get_sample_size(libvgmstream_sfmt_t sample_format) {
    switch(sample_format) {
        case LIBVGMSTREAM_SFMT_FLOAT:
        case LIBVGMSTREAM_SFMT_PCM32:
            return 0x04;
        case LIBVGMSTREAM_SFMT_PCM24:
            return 0x03;
        case LIBVGMSTREAM_SFMT_PCM16:
        default:
            return 0x02;
    }
}
