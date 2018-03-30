#include "layout.h"
#include "../vgmstream.h"

/* TODO: currently only properly handles mono substreams */
/* TODO: there must be a reasonable way to respect the loop settings, as
   the substreams are in their own little world.
   Currently the VGMSTREAMs layers loop internally and the external/base VGMSTREAM
   doesn't actually loop, and would ignore any altered values/loop_flag. */

#define INTERLEAVE_BUF_SIZE 512


void render_vgmstream_layered(sample * buffer, int32_t sample_count, VGMSTREAM * vgmstream) {
    sample interleave_buf[INTERLEAVE_BUF_SIZE];
    int32_t samples_done = 0;
    layered_layout_data *data = vgmstream->layout_data;

    while (samples_done < sample_count) {
        int32_t samples_to_do = INTERLEAVE_BUF_SIZE;
        int c;
        if (samples_to_do > sample_count - samples_done)
            samples_to_do = sample_count - samples_done;

        for (c=0; c < data->layer_count; c++) {
            int32_t i;

            render_vgmstream(interleave_buf, samples_to_do, data->layers[c]);

            for (i=0; i < samples_to_do; i++) {
                buffer[(samples_done+i)*data->layer_count + c] = interleave_buf[i];
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

        //todo only mono at the moment
        if (data->layers[i]->channels != 1)
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
