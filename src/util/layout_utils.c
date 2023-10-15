#include "layout_utils.h"

#include "../vgmstream.h"
#include "../layout/layout.h"
#include "../coding/coding.h"


typedef VGMSTREAM* (*init_vgmstream_t)(STREAMFILE*);

bool layered_add_subfile(VGMSTREAM* vs, int layers, int layer_channels, STREAMFILE* sf, uint32_t offset, uint32_t size, const char* ext, init_vgmstream_t init_vgmstream) {
    int i;
    layered_layout_data* data;
    STREAMFILE* temp_sf;

    if (!vs) {
        goto fail;
    }

    if (!layer_channels)
        layer_channels = 1;
    if (!layers)
        layers = vs->channels / layer_channels;

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

    temp_sf = setup_subfile_streamfile(sf, offset, size, ext);
    if (!temp_sf) goto fail;

    data->layers[i] = init_vgmstream(temp_sf);
    close_streamfile(temp_sf);
    if (!data->layers[i]) goto fail;

    data->curr_layer++;

    return true;
fail:
    return false;
}


static bool layered_add_internal(VGMSTREAM* vs, int layers, int layer_channels, STREAMFILE* sf) {
    int i;
    layered_layout_data* data;

    if (!vs) {
        goto fail;
    }

    if (!layer_channels)
        layer_channels = 1;
    if (!layers)
        layers = vs->channels / layer_channels;

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
    data->layers[i]->coding_type = vs->coding_type;

    data->layers[i]->layout_type = layout_none;
    data->layers[i]->interleave_block_size = vs->interleave_block_size;
    if (vs->interleave_block_size)
        data->layers[i]->layout_type = layout_interleave;

    vs->codec_data = NULL; /* moved to layer, don't hold it */

    if (sf) {
        if (!vgmstream_open_stream(data->layers[i], sf, 0x00))
            goto fail;
    }

    data->curr_layer++;

    return true;
fail:
    return false;
}

bool layered_add_sf(VGMSTREAM* vs, int layers, int layer_channels, STREAMFILE* sf) {
    return layered_add_internal(vs, layers, layer_channels, sf);
}

bool layered_add_codec(VGMSTREAM* vs, int layers, int layer_channels) {
    if (!vs->codec_data)
        return false;

    return layered_add_internal(vs, layers, layer_channels, NULL);
}


bool layered_add_done(VGMSTREAM* vs) {
    //TODO: some extra checks/setup?

    if (!setup_layout_layered(vs->layout_data))
        goto fail;

    if (!vs->coding_type) {
        layered_layout_data* data = vs->layout_data;
        vs->coding_type = data->layers[0]->coding_type;
    }

    return true;
fail:
    return false;
}
