#include "../vgmstream.h"
#include "../util/channel_mappings.h"
#include "mixing.h"
#include "mixing_priv.h"
#include "sbuf.h"
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
    mixer_t* mixer = vgmstream->mixer;

    /* no support or not need to apply */
    if (!mixer || !mixer->active || mixer->chain_count == 0)
        return;

    int32_t current_pos = get_current_pos(vgmstream, sample_count);

    mixer_process(mixer, outbuf, sample_count, current_pos);
}

/* ******************************************************************* */

void* mixer_init(int channels) {
    mixer_t* mixer = calloc(1, sizeof(mixer_t));
    if (!mixer) goto fail;

    mixer->chain_size = VGMSTREAM_MAX_MIXING; /* fixed array for now */
    mixer->mixing_channels = channels;
    mixer->output_channels = channels;
    mixer->input_channels = channels;

    return mixer;

fail:
    mixer_free(mixer);
    return NULL;
}

void mixer_free(void* _mixer) {
    mixer_t* mixer = _mixer;
    if (!mixer) return;

    free(mixer->mixbuf);
    free(mixer);
}

void mixer_update_channel(void* _mixer) {
    mixer_t* mixer = _mixer;
    if (!mixer) return;

    /* lame hack for dual stereo, but dual stereo is pretty hack-ish to begin with */
    mixer->mixing_channels++;
    mixer->output_channels++;
}

bool mixer_is_active(void* _mixer) {
    mixer_t* mixer = _mixer;

    /* no support or not need to apply */
    if (!mixer || !mixer->active || mixer->chain_count == 0)
        return false;

    return true;
}


void mixer_process(void* _mixer, sample_t *outbuf, int32_t sample_count, int32_t current_pos) {
    mixer_t* mixer = _mixer;

    /* no support or not need to apply */
    if (!mixer || !mixer->active || mixer->chain_count == 0)
        return;

    /* try to skip if no fades apply (set but does nothing yet) + only has fades 
     * (could be done in mix op but avoids upgrading bufs in some cases) */
    mixer->current_subpos = 0;
    if (mixer->has_fade) {
        //;VGM_LOG("MIX: fade test %i, %i\n", data->has_non_fade, mixer_op_fade_is_active(data, current_pos, current_pos + sample_count));
        if (!mixer->has_non_fade && !mixer_op_fade_is_active(mixer, current_pos, current_pos + sample_count))
            return;

        //;VGM_LOG("MIX: fade pos=%i\n", current_pos);
        mixer->current_subpos = current_pos;
    }

    // upgrade buf for mixing (somehow using float buf rather than int32 is faster?)
    sbuf_copy_s16_to_f32(mixer->mixbuf, outbuf, sample_count, mixer->input_channels);

    /* apply mixing ops in order. Some ops change total channels they may change around:
     * - 2ch w/ "1+2,1u" = ch1+ch2, ch1(add and push rest) = 3ch: ch1' ch1+ch2 ch2
     * - 2ch w/ "1u"     = downmix to 1ch (current_channels decreases once)
     */
    mixer->current_channels = mixer->input_channels;
    for (int m = 0; m < mixer->chain_count; m++) {
        mix_op_t* mix = &mixer->chain[m];

        //TODO: set callback
        switch(mix->type) {
            case MIX_SWAP:      mixer_op_swap(mixer, sample_count, mix); break;
            case MIX_ADD:       mixer_op_add(mixer, sample_count, mix); break;
            case MIX_VOLUME:    mixer_op_volume(mixer, sample_count, mix); break;
            case MIX_LIMIT:     mixer_op_limit(mixer, sample_count, mix); break;
            case MIX_UPMIX:     mixer_op_upmix(mixer, sample_count, mix); break;
            case MIX_DOWNMIX:   mixer_op_downmix(mixer, sample_count, mix); break;
            case MIX_KILLMIX:   mixer_op_killmix(mixer, sample_count, mix); break;
            case MIX_FADE:      mixer_op_fade(mixer, sample_count, mix);
            default:
                break;
        }
    }

    /* downgrade mix to original output */
    sbuf_copy_f32_to_s16(outbuf, mixer->mixbuf, sample_count, mixer->output_channels);
}

/* ******************************************************************* */

static int fix_layered_channel_layout(VGMSTREAM* vgmstream) {
    mixer_t* data = vgmstream->mixer;
    layered_layout_data* layout_data;
    uint32_t prev_cl;

    if (vgmstream->channel_layout || vgmstream->layout_type != layout_layered)
        return 0;
  
    layout_data = vgmstream->layout_data;

    /* mainly layer-v (in cases of layers-within-layers should cascade) */
    if (data->output_channels != layout_data->layers[0]->channels)
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
    mixer_t* data = vgmstream->mixer;

    if (fix_layered_channel_layout(vgmstream))
        goto done;

    /* segments should share channel layout automatically */

    /* a bit wonky but eh... */
    if (vgmstream->channel_layout && vgmstream->channels != data->output_channels) {
        vgmstream->channel_layout = 0;
    }

done:
    ((VGMSTREAM*)vgmstream->start_vgmstream)->channel_layout = vgmstream->channel_layout;
}


void mixing_setup(VGMSTREAM* vgmstream, int32_t max_sample_count) {
    mixer_t* data = vgmstream->mixer;

    if (!data)
        return;

    /* special value to not actually enable anything (used to query values) */
    if (max_sample_count <= 0)
        return;

    /* create or alter internal buffer */
    float *mixbuf_re = realloc(data->mixbuf, max_sample_count * data->mixing_channels * sizeof(float));
    if (!mixbuf_re) goto fail;

    data->mixbuf = mixbuf_re;
    data->active = true;

    fix_channel_layout(vgmstream);

    /* since data exists on its own memory and pointer is already set
     * there is no need to propagate to start_vgmstream */

    /* segments/layers are independant from external buffers and may always mix */

    return;
fail:
    return;
}

void mixing_info(VGMSTREAM* vgmstream, int* p_input_channels, int* p_output_channels) {
    mixer_t* data = vgmstream->mixer;
    int input_channels, output_channels;

    if (!data)
        goto fail;

    output_channels = data->output_channels;
    if (data->output_channels > vgmstream->channels)
        input_channels = data->output_channels;
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
