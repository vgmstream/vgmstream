#include "layout.h"
#include "../vgmstream.h"
#include "../base/decode.h"
#include "../base/mixing.h"
#include "../base/plugins.h"
#include "../base/sbuf.h"
#include "../base/render.h"

#define VGMSTREAM_MAX_LAYERS 255
#define VGMSTREAM_LAYER_SAMPLE_BUFFER 8192


/* Decodes samples for layered streams.
 * Each decoded vgmstream 'layer' (which may have different codecs and number of channels)
 * is mixed into a final buffer, creating a single super-vgmstream. */
void render_vgmstream_layered(sbuf_t* sdst, VGMSTREAM* vgmstream) {
    layered_layout_data* data = vgmstream->layout_data;
    sbuf_t ssrc_tmp;
    sbuf_t* ssrc = &ssrc_tmp;

    int samples_per_frame = VGMSTREAM_LAYER_SAMPLE_BUFFER;
    int samples_this_block = vgmstream->num_samples; /* do all samples if possible */

    //int samples_filled = 0;
    while (sdst->filled < sdst->samples) {
        int ch;

        if (vgmstream->loop_flag && decode_do_loop(vgmstream)) {
            /* handle looping (loop_layout has been called inside) */
            continue;
        }

        int samples_to_do = decode_get_samples_to_do(samples_this_block, samples_per_frame, vgmstream);
        if (samples_to_do > sdst->samples - sdst->filled)
            samples_to_do = sdst->samples - sdst->filled;

        if (samples_to_do <= 0) { /* when decoding more than num_samples */
            VGM_LOG_ONCE("LAYERED: wrong %i samples_to_do (%i filled vs %i samples)\n", samples_to_do, sdst->filled, sdst->samples); 
            goto decode_fail;
        }

        /* decode all layers */
        ch = 0;
        for (int current_layer = 0; current_layer < data->layer_count; current_layer++) {
            VGMSTREAM* vl = data->layers[current_layer];

            // layers may have their own number of channels/format (buf is as big as needed)
            sfmt_t format = mixing_get_input_sample_type(vl);
            sbuf_init(ssrc, format, data->buffer, samples_to_do, vl->channels);

            render_main(ssrc, vl);

            // mix layer samples to main samples
            sbuf_copy_layers(sdst, ssrc, ch, samples_to_do);
            ch += ssrc->channels;
        }

        sdst->filled += samples_to_do;
        vgmstream->current_sample += samples_to_do;
        vgmstream->samples_into_block += samples_to_do;
    }

    return;
decode_fail:
    sbuf_silence_rest(sdst);
}


/* seeks inside the streams */
//TODO: looping doesn't work if seeking after it without setting hit-loop stuff
void seek_layout_layered(VGMSTREAM* vgmstream, int32_t seek_sample) {
    layered_layout_data* data = vgmstream->layout_data;

    for (int layer = 0; layer < data->layer_count; layer++) {
        seek_vgmstream(data->layers[layer], seek_sample);
    }

    vgmstream->current_sample = seek_sample;
    vgmstream->samples_into_block = seek_sample;
}

void loop_layout_layered(VGMSTREAM* vgmstream, int32_t loop_sample) {
    layered_layout_data* data = vgmstream->layout_data;

    for (int layer = 0; layer < data->layer_count; layer++) {
        if (data->external_looping) {
            /* looping is applied over resulting decode, as each layer is its own "solid" block with
             * config and needs 'external' seeking */
            seek_vgmstream(data->layers[layer], loop_sample);
        }
        else {
            /* looping is aplied as internal loops. normally each layer does it automatically, but
             * just calls do_loop manually to behave a bit more controlled, and so that manual
             * calls to do_loop work (used in seek_vgmstream) */
            if (data->layers[layer]->loop_flag) { /* mixing looping and non-looping layers is allowed */
                data->layers[layer]->current_sample = data->layers[layer]->loop_end_sample; /* forces do loop */
                decode_do_loop(data->layers[layer]); /* guaranteed to work should loop_layout be called */
            }
            else {
                /* needed when mixing non-looping layers and installing loop externally */
                seek_vgmstream(data->layers[layer], loop_sample);
            }
        }
    }

    /* could always call seek_vgmstream, but it's not optimized to loop non-config vgmstreams ATM */

    vgmstream->current_sample = loop_sample;
    vgmstream->samples_into_block = loop_sample;
}


layered_layout_data* init_layout_layered(int layer_count) {
    layered_layout_data *data = NULL;

    if (layer_count <= 0 || layer_count > VGMSTREAM_MAX_LAYERS)
        goto fail;

    data = calloc(1, sizeof(layered_layout_data));
    if (!data) goto fail;

    data->layers = calloc(layer_count, sizeof(VGMSTREAM*));
    if (!data->layers) goto fail;

    data->layer_count = layer_count;

    return data;
fail:
    free_layout_layered(data);
    return NULL;
}

bool setup_layout_layered(layered_layout_data* data) {
    int max_input_channels = 0;
    int max_output_channels = 0;
    sfmt_t max_sample_type = SFMT_NONE;

    /* setup each VGMSTREAM (roughly equivalent to vgmstream.c's init_vgmstream_internal stuff) */
    for (int i = 0; i < data->layer_count; i++) {
        if (data->layers[i] == NULL) {
            VGM_LOG("LAYERED: no vgmstream in %i\n", i);
            return false;
        }

        if (data->layers[i]->num_samples <= 0) {
            VGM_LOG("LAYERED: no samples in %i\n", i);
            return false;
        }

        /* different layers may have different input/output channels or sample formats */
        int layer_input_channels, layer_output_channels;
        mixing_info(data->layers[i], &layer_input_channels, &layer_output_channels);

        max_output_channels += layer_output_channels;
        if (max_input_channels < layer_input_channels)
            max_input_channels = layer_input_channels;

        if (i > 0) {
            /* a bit weird, but no matter */
            if (data->layers[i]->sample_rate != data->layers[i-1]->sample_rate) {
                VGM_LOG("LAYERED: layer %i has different sample rate\n", i);
                //TO-DO: setup resampling
            }

#if 0
            /* also weird but less so */
            if (data->layers[i]->coding_type != data->layers[i-1]->coding_type) {
                VGM_LOG("LAYERED: layer %i has different coding type\n", i);
            }
#endif
        }

        sfmt_t current_sample_type = mixing_get_input_sample_type(data->layers[i]);
        if (max_sample_type < current_sample_type && max_sample_type != SFMT_FLT) //float has priority
            max_sample_type = current_sample_type;

        /* loops and other values could be mismatched, but should be handled on allocate */

        /* init mixing */
        mixing_setup(data->layers[i], VGMSTREAM_LAYER_SAMPLE_BUFFER);

        /* allow config if set for fine-tuned parts (usually TXTP only) */
        data->layers[i]->config_enabled = data->layers[i]->config.config_set;

        /* final setup in case the VGMSTREAM was created manually */
        setup_vgmstream(data->layers[i]);
    }

    if (max_output_channels > VGMSTREAM_MAX_CHANNELS || max_input_channels > VGMSTREAM_MAX_CHANNELS)
        return false;

    // needed for codecs like FFMpeg where base vgmstream's sample type is unknown
    data->fmt = max_sample_type;
    int max_sample_size = sfmt_get_sample_size(max_sample_type);

    /* create internal buffer big enough for mixing all layers */
    free(data->buffer);
    data->buffer = malloc(VGMSTREAM_LAYER_SAMPLE_BUFFER * max_input_channels * max_sample_size);
    if (!data->buffer) goto fail;

    data->input_channels = max_input_channels;
    data->output_channels = max_output_channels;

    return true;
fail:
    return false; /* caller is expected to free */
}

void free_layout_layered(layered_layout_data* data) {
    if (!data)
        return;

    for (int i = 0; i < data->layer_count; i++) {
        close_vgmstream(data->layers[i]);
    }
    free(data->layers);
    free(data->buffer);
    free(data);
}

void reset_layout_layered(layered_layout_data* data) {
    if (!data)
        return;

    for (int i = 0; i < data->layer_count; i++) {
        reset_vgmstream(data->layers[i]);
    }
}
