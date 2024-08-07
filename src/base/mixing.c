#include "../vgmstream.h"
#include "../util/channel_mappings.h"
#include "mixing.h"
#include "mixer.h"
#include "mixer_priv.h"
#include "sbuf.h"
#include "../layout/layout.h"
#include <math.h>
#include <limits.h>

//TODO simplify
/**
 * Mixer modifies decoded sample buffer before final output. This is implemented
 * mostly with simplicity in mind rather than performance. Process:
 * - detect if mixing applies at current moment or exit (mini performance optimization)
 * - copy/upgrade buf to float mixbuf if needed
 * - do mixing ops
 * - copy/downgrade mixbuf to original buf if needed
 * 
 * Mixing may add or remove channels. input_channels is the buf's original channels,
 * and output_channels the resulting buf's channels. buf and mixbuf must be
 * as big as max channels (mixing_channels).
 * 
 * Mixing ops are added by a meta (ex. TXTP) or plugin through the API. Non-sensical
 * mixes are ignored (to avoid rechecking every time).
 * 
 * Currently, mixing must be manually enabled before starting to decode, because plugins
 * need to setup bigger bufs when upmixing. (to be changed)
 *
 * segmented/layered layouts handle mixing on their own.
 */

/* ******************************************************************* */

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

void mix_vgmstream(sample_t *outbuf, int32_t sample_count, VGMSTREAM* vgmstream) {
    /* no support or not need to apply */
    if (!mixer_is_active(vgmstream->mixer))
        return;

    int32_t current_pos = get_current_pos(vgmstream, sample_count);

    mixer_process(vgmstream->mixer, outbuf, sample_count, current_pos);
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

    if (!mixer)
        goto fail;

    output_channels = mixer->output_channels;
    if (mixer->output_channels > vgmstream->channels)
        input_channels = mixer->output_channels;
    else
        input_channels = vgmstream->channels;

    if (p_input_channels)  *p_input_channels = input_channels;
    if (p_output_channels) *p_output_channels = output_channels;

    //;VGM_LOG("MIX: channels %i, in=%i, out=%i, mix=%i\n", vgmstream->channels, input_channels, output_channels, data->mixing_channels);
    return;
fail:
    if (p_input_channels)  *p_input_channels = vgmstream->channels;
    if (p_output_channels) *p_output_channels = vgmstream->channels;
    return;
}
