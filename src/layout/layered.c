#include "layout.h"
#include "../vgmstream.h"

/* TODO: there must be a reasonable way to respect the loop settings, as
   the substreams are in their own little world.
   Currently the VGMSTREAMs layers loop internally and the external/base VGMSTREAM
   doesn't actually loop, and would ignore any altered values/loop_flag. */

#define LAYER_BUF_SIZE 512
#define LAYER_MAX_CHANNELS 6 /* at least 2, but let's be generous */


void render_vgmstream_layered(sample * buffer, int32_t sample_count, VGMSTREAM * vgmstream) {
    sample interleave_buf[LAYER_BUF_SIZE*LAYER_MAX_CHANNELS];
    int32_t samples_done = 0;
    layered_layout_data *data = vgmstream->layout_data;

    while (samples_done < sample_count) {
        int32_t samples_to_do = LAYER_BUF_SIZE;
        int layer;

        if (samples_to_do > sample_count - samples_done)
            samples_to_do = sample_count - samples_done;

        for (layer = 0; layer < data->layer_count; layer++) {
            int s,l_ch;
            int layer_channels = data->layers[layer]->channels;

            render_vgmstream(interleave_buf, samples_to_do, data->layers[layer]);

            for (l_ch = 0; l_ch < layer_channels; l_ch++) {
                for (s = 0; s < samples_to_do; s++) {
                    size_t layer_sample = s*layer_channels + l_ch;
                    size_t buffer_sample = (samples_done+s)*vgmstream->channels + (layer*layer_channels+l_ch);

                    buffer[buffer_sample] = interleave_buf[layer_sample];
                }
            }

        }

        samples_done += samples_to_do;
    }
}


layered_layout_data* init_layout_layered(int layer_count) {
    layered_layout_data *data = NULL;

    if (layer_count <= 0 || layer_count > 255)
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
    int i;

    /* setup each VGMSTREAM (roughly equivalent to vgmstream.c's init_vgmstream_internal stuff) */
    for (i = 0; i < data->layer_count; i++) {
        if (!data->layers[i])
            goto fail;

        if (data->layers[i]->num_samples <= 0)
            goto fail;

        if (data->layers[i]->channels > LAYER_MAX_CHANNELS)
            goto fail;

        if (i > 0) {
            /* a bit weird, but no matter */
            if (data->layers[i]->sample_rate != data->layers[i-1]->sample_rate) {
                VGM_LOG("layered layout: layer %i has different sample rate\n", i);
            }

            /* also weird */
            if (data->layers[i]->coding_type != data->layers[i-1]->coding_type) {
                VGM_LOG("layered layout: layer %i has different coding type\n", i);
            }
        }

        //todo could check if layers'd loop match vs main, etc

        /* save start things so we can restart for seeking/looping */
        memcpy(data->layers[i]->start_ch,data->layers[i]->ch,sizeof(VGMSTREAMCHANNEL)*data->layers[i]->channels);
        memcpy(data->layers[i]->start_vgmstream,data->layers[i],sizeof(VGMSTREAM));
    }

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
