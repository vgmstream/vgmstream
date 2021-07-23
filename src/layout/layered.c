#include "layout.h"
#include "../vgmstream.h"
#include "../decode.h"
#include "../mixing.h"
#include "../plugins.h"

#define VGMSTREAM_MAX_LAYERS 255
#define VGMSTREAM_LAYER_SAMPLE_BUFFER 8192


/* Decodes samples for layered streams.
 * Similar to flat layout, but decoded vgmstream are mixed into a final buffer, each vgmstream
 * may have different codecs and number of channels, creating a single super-vgmstream.
 * Usually combined with custom streamfiles to handle data interleaved in weird ways. */
void render_vgmstream_layered(sample_t* outbuf, int32_t sample_count, VGMSTREAM* vgmstream) {
    int samples_written = 0;
    layered_layout_data* data = vgmstream->layout_data;
    int samples_per_frame, samples_this_block;

    samples_per_frame = VGMSTREAM_LAYER_SAMPLE_BUFFER;
    samples_this_block = vgmstream->num_samples; /* do all samples if possible */

    while (samples_written < sample_count) {
        int samples_to_do;
        int layer, ch;


        if (vgmstream->loop_flag && vgmstream_do_loop(vgmstream)) {
            /* handle looping (loop_layout has been called below) */
            continue;
        }

        samples_to_do = get_vgmstream_samples_to_do(samples_this_block, samples_per_frame, vgmstream);
        if (samples_to_do > sample_count - samples_written)
            samples_to_do = sample_count - samples_written;

        if (samples_to_do <= 0) { /* when decoding more than num_samples */
            VGM_LOG_ONCE("LAYERED: samples_to_do 0\n");
            goto decode_fail;
        }

        /* decode all layers */
        ch = 0;
        for (layer = 0; layer < data->layer_count; layer++) {
            int s, layer_ch, layer_channels;

            /* layers may have its own number of channels */
            mixing_info(data->layers[layer], NULL, &layer_channels);

            render_vgmstream(
                    data->buffer,
                    samples_to_do,
                    data->layers[layer]);

            /* mix layer samples to main samples */
            for (layer_ch = 0; layer_ch < layer_channels; layer_ch++) {
                for (s = 0; s < samples_to_do; s++) {
                    size_t layer_sample = s*layer_channels + layer_ch;
                    size_t buffer_sample = (samples_written+s)*data->output_channels + ch;

                    outbuf[buffer_sample] = data->buffer[layer_sample];
                }
                ch++;
            }
        }


        samples_written += samples_to_do;
        vgmstream->current_sample += samples_to_do;
        vgmstream->samples_into_block += samples_to_do;
    }

    return;
decode_fail:
    memset(outbuf + samples_written * data->output_channels, 0, (sample_count - samples_written) * data->output_channels * sizeof(sample_t));
}


void seek_layout_layered(VGMSTREAM* vgmstream, int32_t seek_sample) {
    int layer;
    layered_layout_data* data = vgmstream->layout_data;

    for (layer = 0; layer < data->layer_count; layer++) {
        seek_vgmstream(data->layers[layer], seek_sample);
    }

    vgmstream->current_sample = seek_sample;
    vgmstream->samples_into_block = seek_sample;
}

void loop_layout_layered(VGMSTREAM* vgmstream, int32_t loop_sample) {
    int layer;
    layered_layout_data* data = vgmstream->layout_data;


    for (layer = 0; layer < data->layer_count; layer++) {
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
                vgmstream_do_loop(data->layers[layer]); /* guaranteed to work should loop_layout be called */
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

    data->layer_count = layer_count;

    data->layers = calloc(layer_count, sizeof(VGMSTREAM*));
    if (!data->layers) goto fail;

    return data;
fail:
    free_layout_layered(data);
    return NULL;
}

int setup_layout_layered(layered_layout_data* data) {
    int i, max_input_channels = 0, max_output_channels = 0;
    sample_t *outbuf_re = NULL;


    /* setup each VGMSTREAM (roughly equivalent to vgmstream.c's init_vgmstream_internal stuff) */
    for (i = 0; i < data->layer_count; i++) {
        int layer_input_channels, layer_output_channels;

        if (data->layers[i] == NULL) {
            VGM_LOG("LAYERED: no vgmstream in %i\n", i);
            goto fail;
        }

        if (data->layers[i]->num_samples <= 0) {
            VGM_LOG("LAYERED: no samples in %i\n", i);
            goto fail;
        }

        /* different layers may have different input/output channels */
        mixing_info(data->layers[i], &layer_input_channels, &layer_output_channels);

        max_output_channels += layer_output_channels;
        if (max_input_channels < layer_input_channels)
            max_input_channels = layer_input_channels;

        if (i > 0) {
            /* a bit weird, but no matter */
            if (data->layers[i]->sample_rate != data->layers[i-1]->sample_rate) {
                VGM_LOG("LAYERED: layer %i has different sample rate\n", i);
            }

            /* also weird */
            if (data->layers[i]->coding_type != data->layers[i-1]->coding_type) {
                VGM_LOG("LAYERED: layer %i has different coding type\n", i);
            }
        }

        /* loops and other values could be mismatched, but should be handled on allocate */

        /* init mixing */
        mixing_setup(data->layers[i], VGMSTREAM_LAYER_SAMPLE_BUFFER);

        /* allow config if set for fine-tuned parts (usually TXTP only) */
        data->layers[i]->config_enabled = data->layers[i]->config.config_set;

        /* final setup in case the VGMSTREAM was created manually */
        setup_vgmstream(data->layers[i]);
    }

    if (max_output_channels > VGMSTREAM_MAX_CHANNELS || max_input_channels > VGMSTREAM_MAX_CHANNELS)
        goto fail;

    /* create internal buffer big enough for mixing */
    outbuf_re = realloc(data->buffer, VGMSTREAM_LAYER_SAMPLE_BUFFER*max_input_channels*sizeof(sample_t));
    if (!outbuf_re) goto fail;
    data->buffer = outbuf_re;

    data->input_channels = max_input_channels;
    data->output_channels = max_output_channels;

    return 1;
fail:
    return 0; /* caller is expected to free */
}

void free_layout_layered(layered_layout_data *data) {
    int i;

    if (!data)
        return;

    if (data->layers) {
        for (i = 0; i < data->layer_count; i++) {
            close_vgmstream(data->layers[i]);
        }
        free(data->layers);
    }
    free(data->buffer);
    free(data);
}

void reset_layout_layered(layered_layout_data *data) {
    int i;

    if (!data)
        return;

    for (i = 0; i < data->layer_count; i++) {
        reset_vgmstream(data->layers[i]);
    }
}

/* helper for easier creation of layers */
VGMSTREAM* allocate_layered_vgmstream(layered_layout_data* data) {
    VGMSTREAM* vgmstream = NULL;
    int i, channels, loop_flag, sample_rate, external_looping;
    int32_t num_samples, loop_start, loop_end;
    int delta = 1024;
    coding_t coding_type = data->layers[0]->coding_type;

    /* get data */
    channels = data->output_channels;

    num_samples = 0;
    loop_flag = 1;
    loop_start = data->layers[0]->loop_start_sample;
    loop_end = data->layers[0]->loop_end_sample;
    external_looping = 0;
    sample_rate = 0;

    for (i = 0; i < data->layer_count; i++) {
        int32_t layer_samples = vgmstream_get_samples(data->layers[i]);
        int layer_loop = data->layers[i]->loop_flag;
        int32_t layer_loop_start = data->layers[i]->loop_start_sample;
        int32_t layer_loop_end = data->layers[i]->loop_end_sample;
        int layer_rate = data->layers[i]->sample_rate;

        /* internal has own config (and maybe looping), looping now must be done on layout level
         * (instead of on each layer, that is faster) */
        if (data->layers[i]->config_enabled) {
            loop_flag = 0;
            layer_loop = 0;
            external_looping = 1;
        }

        /* all layers should share loop pointsto consider looping enabled,
         * but allow some leeway (ex. Dragalia Lost bgm+vocals ~12 samples) */
        if (!layer_loop
                || !(loop_start >= layer_loop_start - delta && loop_start <= layer_loop_start + delta)
                || !(loop_end >= layer_loop_end - delta && loop_start <= layer_loop_end + delta)) {
            loop_flag = 0;
            loop_start = 0;
            loop_end = 0;
        }

        if (num_samples < layer_samples) /* max */
            num_samples = layer_samples;

        if (sample_rate < layer_rate)
            sample_rate = layer_rate;

        if (coding_type == coding_SILENCE)
            coding_type = data->layers[i]->coding_type;
    }

    data->external_looping = external_looping;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = data->layers[0]->meta_type;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;
    vgmstream->coding_type = coding_type;

    vgmstream->layout_type = layout_layered;
    vgmstream->layout_data = data;

    return vgmstream;

fail:
    if (vgmstream) vgmstream->layout_data = NULL;
    close_vgmstream(vgmstream);
    return NULL;
}
