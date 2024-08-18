#include "layout_utils.h"

#include "../vgmstream.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "../base/plugins.h"


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


/* helper for easier creation of segments */
VGMSTREAM* allocate_segmented_vgmstream(segmented_layout_data* data, int loop_flag, int loop_start_segment, int loop_end_segment) {
    VGMSTREAM* vgmstream = NULL;
    int channel_layout;
    int i, sample_rate;
    int32_t num_samples, loop_start, loop_end;
    coding_t coding_type = data->segments[0]->coding_type;

    /* save data */
    channel_layout = data->segments[0]->channel_layout;
    num_samples = 0;
    loop_start = 0;
    loop_end = 0;
    sample_rate = 0;
    for (i = 0; i < data->segment_count; i++) {
        /* needs get_samples since element may use play settings */
        int32_t segment_samples = vgmstream_get_samples(data->segments[i]);
        int segment_rate = data->segments[i]->sample_rate;

        if (loop_flag && i == loop_start_segment)
            loop_start = num_samples;

        num_samples += segment_samples;

        if (loop_flag && i == loop_end_segment)
            loop_end = num_samples;

        /* inherit first segment's layout but only if all segments' layout match */
        if (channel_layout != 0 && channel_layout != data->segments[i]->channel_layout)
            channel_layout = 0;

        if (sample_rate < segment_rate)
            sample_rate = segment_rate;

        if (coding_type == coding_SILENCE)
            coding_type = data->segments[i]->coding_type;
    }

    /* respect loop_flag even when no loop_end found as it's possible file loops are set outside */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(data->output_channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = data->segments[0]->meta_type;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;
    vgmstream->coding_type = coding_type;
    vgmstream->channel_layout = channel_layout;

    vgmstream->layout_type = layout_segmented;
    vgmstream->layout_data = data;

    return vgmstream;

fail:
    if (vgmstream) vgmstream->layout_data = NULL;
    close_vgmstream(vgmstream);
    return NULL;
}


void blocked_count_samples(VGMSTREAM* vgmstream, STREAMFILE* sf, blocked_counter_t* cfg) {
    if (vgmstream == NULL)
        return;

    int block_samples;
    off_t max_offset = get_streamfile_size(sf);

    vgmstream->next_block_offset = cfg->offset;
    do {
        block_update(vgmstream->next_block_offset, vgmstream);

        if (vgmstream->current_block_samples < 0 || vgmstream->current_block_size == 0xFFFFFFFF)
            break;

        if (vgmstream->current_block_samples) {
            block_samples = vgmstream->current_block_samples;
        }
        else {
            switch(vgmstream->coding_type) {
                case coding_PCM16LE:
                case coding_PCM16_int:  block_samples = pcm16_bytes_to_samples(vgmstream->current_block_size, 1); break;
                case coding_PCM8_int:
                case coding_PCM8_U_int: block_samples = pcm8_bytes_to_samples(vgmstream->current_block_size, 1); break;
                case coding_XBOX_IMA_mono:
                case coding_XBOX_IMA:   block_samples = xbox_ima_bytes_to_samples(vgmstream->current_block_size, 1); break;
                case coding_NGC_DSP:    block_samples = dsp_bytes_to_samples(vgmstream->current_block_size, 1); break;
                case coding_PSX:        block_samples = ps_bytes_to_samples(vgmstream->current_block_size,1); break;
                default:
                    VGM_LOG("BLOCKED: missing codec\n");
                    return;
            }
        }

        vgmstream->num_samples += block_samples;
    }
    while (vgmstream->next_block_offset < max_offset);

    block_update(cfg->offset, vgmstream); /* reset */
}
