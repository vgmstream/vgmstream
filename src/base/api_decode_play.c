#include "api_internal.h"
#include "mixing.h"

#if LIBVGMSTREAM_ENABLE


static bool reset_buf(libvgmstream_priv_t* priv) {
    // state reset
    priv->buf.samples = 0;
    priv->buf.bytes = 0;
    priv->buf.consumed = 0;

    if (priv->buf.initialized)
        return true;

    // calc input/output values to reserve buf (should be as big as input or output)
    int input_channels = 0, output_channels = 0;
    vgmstream_mixing_enable(priv->vgmstream, 0, &input_channels, &output_channels); //query

    int min_channels = input_channels;
    if (min_channels < output_channels)
        min_channels = output_channels;

    sfmt_t input_sfmt = mixing_get_input_sample_type(priv->vgmstream);
    sfmt_t output_sfmt = mixing_get_output_sample_type(priv->vgmstream);
    int input_sample_size = sfmt_get_sample_size(input_sfmt);
    int output_sample_size = sfmt_get_sample_size(output_sfmt);

    int min_sample_size = input_sample_size;
    if (min_sample_size < output_sample_size)
        min_sample_size = output_sample_size;

    priv->buf.max_samples = INTERNAL_BUF_SAMPLES;
    priv->buf.sample_size = output_sample_size;
    priv->buf.channels = output_channels;

    int max_bytes = priv->buf.max_samples * min_sample_size * min_channels;
    priv->buf.data = malloc(max_bytes);
    if (!priv->buf.data) return false;

    priv->buf.initialized = true;
    return true;
}

static void update_buf(libvgmstream_priv_t* priv, int samples_done) {
    priv->buf.samples = samples_done;
    priv->buf.bytes = samples_done * priv->buf.sample_size * priv->buf.channels;
    //priv->buf.consumed = 0; //external

    if (!priv->pos.play_forever) {
        priv->decode_done = (priv->pos.current >= priv->pos.play_samples);
        priv->pos.current += samples_done;
    }
}


// update decoder info based on last render, though at the moment it's all fixed
static void update_decoder_info(libvgmstream_priv_t* priv, int samples_done) {

    // output copy
    priv->dec.buf = priv->buf.data;
    priv->dec.buf_bytes = priv->buf.bytes;
    priv->dec.buf_samples = priv->buf.samples;
    priv->dec.done = priv->decode_done;
}

LIBVGMSTREAM_API int libvgmstream_render(libvgmstream_t* lib) {
    if (!lib || !lib->priv)
        return LIBVGMSTREAM_ERROR_GENERIC;

    libvgmstream_priv_t* priv = lib->priv;
    if (priv->decode_done)
        return LIBVGMSTREAM_ERROR_GENERIC;

    if (!reset_buf(priv))
        return LIBVGMSTREAM_ERROR_GENERIC;

    int to_get = priv->buf.max_samples;
    if (!priv->pos.play_forever && to_get + priv->pos.current > priv->pos.play_samples)
        to_get = priv->pos.play_samples - priv->pos.current;

    int decoded = render_vgmstream(priv->buf.data, to_get, priv->vgmstream);
    update_buf(priv, decoded);
    update_decoder_info(priv, decoded);

    return LIBVGMSTREAM_OK;
}


/* _play decodes a single frame, while this copies partially that frame until frame is over */
LIBVGMSTREAM_API int libvgmstream_fill(libvgmstream_t* lib, void* buf, int buf_samples) {
    if (!lib || !lib->priv || !buf || !buf_samples)
        return LIBVGMSTREAM_ERROR_GENERIC;

    libvgmstream_priv_t* priv = lib->priv;
    if (priv->decode_done)
        return LIBVGMSTREAM_ERROR_GENERIC;

    if (priv->buf.consumed >= priv->buf.samples) {
        int err = libvgmstream_render(lib);
        if (err < 0) return err;
    }

    int copy_samples = priv->buf.samples;
    if (copy_samples > buf_samples)
        copy_samples = buf_samples;
    int copy_bytes = priv->buf.sample_size * priv->buf.channels * copy_samples;
    int skip_bytes = priv->buf.sample_size * priv->buf.channels * priv->buf.consumed;

    memcpy(buf, ((uint8_t*)priv->buf.data) + skip_bytes, copy_bytes);
    priv->buf.consumed += copy_samples;

    return copy_samples;
}


LIBVGMSTREAM_API int64_t libvgmstream_get_play_position(libvgmstream_t* lib) {
    if (!lib || !lib->priv)
        return LIBVGMSTREAM_ERROR_GENERIC;

    libvgmstream_priv_t* priv = lib->priv;
    if (!priv->vgmstream)
        return LIBVGMSTREAM_ERROR_GENERIC;

    return priv->vgmstream->pstate.play_position;
}


LIBVGMSTREAM_API void libvgmstream_seek(libvgmstream_t* lib, int64_t sample) {
    if (!lib || !lib->priv)
        return;

    libvgmstream_priv_t* priv = lib->priv;
    if (!priv->vgmstream)
        return;

    seek_vgmstream(priv->vgmstream, sample);

    priv->pos.current = priv->vgmstream->pstate.play_position;
}


LIBVGMSTREAM_API void libvgmstream_reset(libvgmstream_t* lib) {
    if (!lib || !lib->priv)
        return;

    libvgmstream_priv_t* priv = lib->priv;
    if (priv->vgmstream) {
        reset_vgmstream(priv->vgmstream);
    }
    libvgmstream_priv_reset(priv, false);
}

#endif
