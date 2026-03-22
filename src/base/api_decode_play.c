#include "api_internal.h"
#include "mixing.h"
#include "render.h"
#include "../util/log.h"


static bool reset_buf(libvgmstream_priv_t* priv) {
    // state reset
    priv->buf.consumed = 0;

    if (priv->buf.initialized)
        return true;

    // calc input/output values to reserve buf (should be as big as input or output)
    int input_channels = 0, output_channels = 0;
    vgmstream_mixing_enable(priv->vgmstream, 0, &input_channels, &output_channels); //query

    int max_channels = input_channels;
    if (max_channels < output_channels)
        max_channels = output_channels;

    sfmt_t input_sfmt = mixing_get_input_sample_type(priv->vgmstream);
    sfmt_t output_sfmt = mixing_get_output_sample_type(priv->vgmstream);
    int input_sample_size = sfmt_get_sample_size(input_sfmt);
    int output_sample_size = sfmt_get_sample_size(output_sfmt);

    int max_sample_size = input_sample_size;
    if (max_sample_size < output_sample_size)
        max_sample_size = output_sample_size;

    priv->buf.max_samples = INTERNAL_BUF_SAMPLES;
    priv->buf.sample_size = output_sample_size;
    priv->buf.channels = output_channels;

    int max_bytes = priv->buf.max_samples * max_sample_size * max_channels;
    priv->buf.data = malloc(max_bytes);
    if (!priv->buf.data) return false;

    priv->buf.initialized = true;
    return true;
}

// update info based on last render
static void update_decoder_info(libvgmstream_priv_t* priv) {

    // mark done if buf reaches EOF (may also happen after seek)
    if (!priv->pos.play_forever) {
        priv->pos.current += priv->sbuf.filled;
        priv->decode_done = (priv->pos.current >= priv->pos.play_samples);
    }

    sbuf_t* sbuf = &priv->sbuf;
    int sample_size = sfmt_get_sample_size(sbuf->fmt);
    priv->dec.buf = sbuf->buf;
    priv->dec.buf_samples = sbuf->filled;
    priv->dec.buf_bytes = priv->dec.buf_samples * sample_size * sbuf->channels;
    priv->dec.done = priv->decode_done;
}

LIBVGMSTREAM_API int libvgmstream_render(libvgmstream_t* lib) {
    if (!lib || !lib->priv)
        return LIBVGMSTREAM_ERROR_GENERIC;

    libvgmstream_priv_t* priv = lib->priv;

    // setup if not called (mainly to make sure mixing is enabled) //TODO: handle internally
    // (for cases where _open_stream is called but not _setup)
    if (!priv->setup_done) {
        api_apply_config(priv);
    }

    if (priv->decode_done)
        return LIBVGMSTREAM_ERROR_GENERIC;

    if (!reset_buf(priv))
        return LIBVGMSTREAM_ERROR_GENERIC;

    // requested samples, may return different max
    int to_get = priv->buf.max_samples;
    if (!priv->pos.play_forever && to_get + priv->pos.current > priv->pos.play_samples)
        to_get = priv->pos.play_samples - priv->pos.current;

    // default sbuf, may change during render
    sfmt_t sfmt = mixing_get_input_sample_type(priv->vgmstream);
    sbuf_init(&priv->sbuf, sfmt, priv->buf.data, to_get, priv->vgmstream->channels);

    render_main(&priv->sbuf, priv->vgmstream);

    update_decoder_info(priv);

    return LIBVGMSTREAM_OK;
}


/* _play decodes a single frame, while this copies partially that frame until frame is over */
LIBVGMSTREAM_API int libvgmstream_fill(libvgmstream_t* lib, void* buf, int buf_samples) {
    if (!lib || !lib->priv || !buf || !buf_samples)
        return LIBVGMSTREAM_ERROR_GENERIC;

    libvgmstream_priv_t* priv = lib->priv;

    bool done = false;
    int buf_copied = 0;
    while (buf_copied < buf_samples) {

        // decode if no samples in internal buf
        if (priv->buf.consumed >= priv->sbuf.filled) {
            if (priv->decode_done) {
                done = true;
                break;
            }

            int err = libvgmstream_render(lib);
            if (err < 0) return err;
        }

        // copy from partial decode src to partial dst
        int buf_left = buf_samples - buf_copied;
        int copy_samples = priv->sbuf.filled - priv->buf.consumed;
        if (copy_samples > buf_left)
            copy_samples = buf_left;

        int copy_bytes = priv->buf.sample_size * priv->buf.channels * copy_samples;
        int skip_bytes = priv->buf.sample_size * priv->buf.channels * priv->buf.consumed;
        int copied_bytes = priv->buf.sample_size * priv->buf.channels * buf_copied;

        memcpy( ((uint8_t*)buf) + copied_bytes, ((uint8_t*)priv->sbuf.buf) + skip_bytes, copy_bytes);
        priv->buf.consumed += copy_samples;

        buf_copied += copy_samples;
    }

    // detect EOF, to avoid another call to _fill that returns with 0 samples
    if (!done && priv->decode_done && priv->buf.consumed >= priv->sbuf.filled) {
        done = true;
    }

    // TODO improve
    priv->dec.buf = buf;
    priv->dec.buf_samples = buf_copied;
    priv->dec.buf_bytes = buf_copied * priv->buf.sample_size * priv->buf.channels;
    priv->dec.done = done;

    // since _fill is used mainly for fixed bufs, blank samples after EOF in case caller only handles exactly buf_samples
    if (done) {
        int buf_left = buf_samples - buf_copied;
        int bytes_bytes = buf_left * priv->buf.sample_size * priv->buf.channels;
        memset( ((uint8_t*)buf) + (priv->dec.buf_bytes), 0, bytes_bytes);
    }

    // the return value is meant to be a result code where >= 0 is ok (returned samples in decoder->buf_samples)
    // but maybe returning samples is more intuitive
    return 0;
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

    // update flags just in case
    sfmt_t sfmt = mixing_get_input_sample_type(priv->vgmstream);
    sbuf_init(&priv->sbuf, sfmt, priv->buf.data, 0, priv->vgmstream->channels);

    update_decoder_info(priv);
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
