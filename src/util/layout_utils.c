#include "layout_utils.h"

#include "../vgmstream.h"
#include "../layout/layout.h"

bool layered_add_codec(VGMSTREAM* vs, int layers, int layer_channels) {
    if (!vs || !vs->codec_data) {
        goto fail;
    }

    if (!layer_channels)
        layer_channels = 1;
    if (!layers)
        layers = vs->channels / layer_channels;

    int i;
    layered_layout_data* data;

    switch(vs->layout_type) {
        case layout_segmented: //to be improved
            goto fail;

        case layout_layered:
            data = vs->layout_data;

            i = data->curr_layer;
            break;

        default:
            data = init_layout_layered(layers);
            if (!data) goto fail;
            vs->layout_data = data;
            vs->layout_type = layout_layered;

            i = 0;
            break;
    }

    data->layers[i] = allocate_vgmstream(layer_channels, vs->loop_flag);
    if (!data->layers[i]) goto fail;

    data->layers[i]->meta_type = vs->meta_type;
    data->layers[i]->sample_rate = vs->sample_rate;
    data->layers[i]->num_samples = vs->num_samples;
    data->layers[i]->loop_start_sample = vs->loop_start_sample;
    data->layers[i]->loop_end_sample = vs->loop_end_sample;

    data->layers[i]->codec_data = vs->codec_data;
    if (!data->layers[i]->codec_data) goto fail;
    data->layers[i]->coding_type = vs->coding_type;
    data->layers[i]->layout_type = layout_none;

    vs->codec_data = NULL; /* moved to layer, don't hold it */

    data->curr_layer++;

    return true;
fail:
    return false;
}

bool layered_add_done(VGMSTREAM* vs) {
    //TODO: some extra checks/setup?

    if (!setup_layout_layered(vs->layout_data))
        goto fail;

    return true;
fail:
    return false;
}
