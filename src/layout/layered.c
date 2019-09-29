#include "layout.h"
#include "../vgmstream.h"
#include "../mixing.h"


/* NOTE: if loop settings change the layered vgmstreams must be notified (preferably using vgmstream_force_loop) */
#define VGMSTREAM_MAX_LAYERS 255
#define VGMSTREAM_LAYER_SAMPLE_BUFFER 8192


/* Decodes samples for layered streams.
 * Similar to interleave layout, but decodec samples are mixed from complete vgmstreams, each
 * with custom codecs and different number of channels, creating a single super-vgmstream.
 * Usually combined with custom streamfiles to handle data interleaved in weird ways. */
void render_vgmstream_layered(sample_t * outbuf, int32_t sample_count, VGMSTREAM * vgmstream) {
    int samples_written = 0;
    layered_layout_data *data = vgmstream->layout_data;


    while (samples_written < sample_count) {
        int samples_to_do = VGMSTREAM_LAYER_SAMPLE_BUFFER;
        int layer, ch = 0;

        if (samples_to_do > sample_count - samples_written)
            samples_to_do = sample_count - samples_written;

        for (layer = 0; layer < data->layer_count; layer++) {
            int s, layer_ch, layer_channels;

            /* each layer will handle its own looping/mixing internally */

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
        /* needed for info (ex. for mixing) */
        vgmstream->current_sample = data->layers[0]->current_sample;
        vgmstream->loop_count = data->layers[0]->loop_count;
        //vgmstream->samples_into_block = 0; /* handled in each layer */
    }
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
            VGM_LOG("layered: no vgmstream in %i\n", i);
            goto fail;
        }

        if (data->layers[i]->num_samples <= 0) {
            VGM_LOG("layered: no samples in %i\n", i);
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
                VGM_LOG("layered: layer %i has different sample rate\n", i);
            }

            /* also weird */
            if (data->layers[i]->coding_type != data->layers[i-1]->coding_type) {
                VGM_LOG("layered: layer %i has different coding type\n", i);
            }
        }

        /* loops and other values could be mismatched but hopefully not */


        setup_vgmstream(data->layers[i]); /* final setup in case the VGMSTREAM was created manually */

        mixing_setup(data->layers[i], VGMSTREAM_LAYER_SAMPLE_BUFFER); /* init mixing */
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
VGMSTREAM *allocate_layered_vgmstream(layered_layout_data* data) {
    VGMSTREAM *vgmstream = NULL;
    int i, channels, loop_flag;

    /* get data */
    channels = data->output_channels;
    loop_flag = 1;
    for (i = 0; i < data->layer_count; i++) {
        if (loop_flag && !data->layers[i]->loop_flag)
            loop_flag = 0;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = data->layers[0]->meta_type;
    vgmstream->sample_rate = data->layers[0]->sample_rate;
    vgmstream->num_samples = data->layers[0]->num_samples;
    vgmstream->loop_start_sample = data->layers[0]->loop_start_sample;
    vgmstream->loop_end_sample = data->layers[0]->loop_end_sample;
    vgmstream->coding_type = data->layers[0]->coding_type;

    vgmstream->layout_type = layout_layered;
    vgmstream->layout_data = data;

    return vgmstream;

fail:
    if (vgmstream) vgmstream->layout_data = NULL;
    close_vgmstream(vgmstream);
    return NULL;
}
