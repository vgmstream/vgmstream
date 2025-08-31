#include <math.h>
#include <limits.h>
#include "../vgmstream.h"
#include "../util/channel_mappings.h"
#include "../layout/layout.h"
#include "mixing.h"
#include "mixer.h"
#include "mixer_priv.h"
#include "sbuf.h"
#include "codec_info.h"

/* Wrapper/helpers for vgmstream's "mixer", which does main sample buffer transformations */

static int32_t get_current_pos(VGMSTREAM* vgmstream, int32_t sample_count) {
    int32_t current_pos;

    if (vgmstream->config_enabled) {
        return vgmstream->pstate.play_position;
    }

    if (vgmstream->loop_flag && vgmstream->loop_count > 0) {
        int loop_pre = vgmstream->loop_start_sample; /* samples before looping */
        int loop_into = (vgmstream->current_sample - vgmstream->loop_start_sample); /* samples after loop */
        int loop_samples = (vgmstream->loop_end_sample - vgmstream->loop_start_sample); /* looped section */

        current_pos = loop_pre + (loop_samples * vgmstream->loop_count) + loop_into - sample_count;
    }
    else {
        current_pos = (vgmstream->current_sample - sample_count);
    }

    return current_pos;
}

void mix_vgmstream(sbuf_t* sbuf, VGMSTREAM* vgmstream) {
    /* no support or not need to apply */
    if (!mixer_is_active(vgmstream->mixer))
        return;

    int32_t current_pos = get_current_pos(vgmstream, sbuf->filled);

    mixer_process(vgmstream->mixer, sbuf, current_pos);
}

/* ******************************************************************* */

static int fix_layered_channel_layout(VGMSTREAM* vgmstream) {
    mixer_t* mixer = vgmstream->mixer;
    layered_layout_data* layout_data;
    uint32_t prev_cl;

    if (vgmstream->channel_layout || vgmstream->layout_type != layout_layered)
        return 0;
  
    layout_data = vgmstream->layout_data;

    /* mainly layer-v (in cases of layers-within-layers should cascade) */
    if (mixer->output_channels != layout_data->layers[0]->channels)
        return 0;

    /* check all layers share layout (implicitly works as a channel check, if not 0) */
    prev_cl = layout_data->layers[0]->channel_layout;
    if (prev_cl == 0)
        return 0;

    for (int i = 1; i < layout_data->layer_count; i++) {
        uint32_t layer_cl = layout_data->layers[i]->channel_layout;
        if (prev_cl != layer_cl)
            return 0;
        
        prev_cl = layer_cl;
    }

    vgmstream->channel_layout = prev_cl;
    return 1;
}

/* channel layout + down/upmixing = ?, salvage what we can */
static void fix_channel_layout(VGMSTREAM* vgmstream) {
    mixer_t* mixer = vgmstream->mixer;

    if (fix_layered_channel_layout(vgmstream))
        goto done;

    /* segments should share channel layout automatically */

    /* a bit wonky but eh... */
    if (vgmstream->channel_layout && vgmstream->channels != mixer->output_channels) {
        vgmstream->channel_layout = 0;
    }

done:
    ((VGMSTREAM*)vgmstream->start_vgmstream)->channel_layout = vgmstream->channel_layout;
}


void mixing_setup(VGMSTREAM* vgmstream, int32_t max_sample_count) {
    mixer_t* mixer = vgmstream->mixer;

    if (!mixer)
        return;

    /* special value to not actually enable anything (used to query values) */
    if (max_sample_count <= 0)
        return;

    /* create or alter internal buffer */
    float* mixbuf_re = realloc(mixer->mixbuf, max_sample_count * mixer->mixing_channels * sizeof(float));
    if (!mixbuf_re) goto fail;

    mixer->mixbuf = mixbuf_re;
    mixer->active = true;

    fix_channel_layout(vgmstream);

    /* since data exists on its own memory and pointer is already set
     * there is no need to propagate to start_vgmstream */

    /* segments/layers are independant from external buffers and may always mix */

    return;
fail:
    return;
}

void mixing_info(VGMSTREAM* vgmstream, int* p_input_channels, int* p_output_channels) {
    mixer_t* mixer = vgmstream->mixer;
    int input_channels, output_channels;

    if (!mixer) {
        if (p_input_channels)  *p_input_channels = vgmstream->channels;
        if (p_output_channels) *p_output_channels = vgmstream->channels;
        return;
    }

    output_channels = mixer->output_channels;
    if (mixer->output_channels > vgmstream->channels)
        input_channels = mixer->output_channels;
    else
        input_channels = vgmstream->channels;

    if (p_input_channels)  *p_input_channels = input_channels;
    if (p_output_channels) *p_output_channels = output_channels;
}

sfmt_t mixing_get_input_sample_type(VGMSTREAM* vgmstream) {
    // TODO: on layered/segments, detect biggest value and use that (ex. if one of the layers uses flt > flt)

    if (vgmstream->layout_type == layout_layered) {
        layered_layout_data* data = vgmstream->layout_data;
        if (data)
            return data->fmt;
    }

    if (vgmstream->layout_type == layout_segmented) {
        segmented_layout_data* data = vgmstream->layout_data;
        if (data)
            return data->fmt;
    }

    const codec_info_t* codec_info = codec_get_info(vgmstream);
    if (codec_info) {
        if (codec_info->sample_type)
            return codec_info->sample_type;
        if (codec_info->get_sample_type)
            return codec_info->get_sample_type(vgmstream);
    }

    // codecs with FLT should have codec_info so default to standard PCM16
    switch(vgmstream->coding_type) {
        default:
            return SFMT_S16;
    }
}

sfmt_t mixing_get_output_sample_type(VGMSTREAM* vgmstream) {
    sfmt_t input_fmt = mixing_get_input_sample_type(vgmstream);

    mixer_t* mixer = vgmstream->mixer;
    if (!mixer)
        return input_fmt;

    if (mixer->force_type)
        return mixer->force_type;

    return input_fmt;
}
