#include "../vgmstream.h"
#include "../util/channel_mappings.h"
#include "mixing.h"
#include "mixer_priv.h"
#include "mixer.h"
#include "sbuf.h"
#include <math.h>
#include <limits.h>

/**
 * Mixer modifies decoded sample buffer before final output. This is implemented
 * with simplicity in mind rather than performance. Process:
 * - detect if mixing applies at current moment or exit (mini performance optimization)
 * - copy/upgrade buf to float mixbuf if needed
 * - do mixing ops
 * - copy/downgrade mixbuf to original buf if needed
 * 
 * Mixing ops are added by a meta (ex. TXTP) or plugins through API. Non-sensical config
 * is ignored on add (to avoid rechecking every time).
 *
 * Mixing may add or remove channels or change sample format. external buf and internal mixbuf
 * are expected to be as big as needed. Currently, mixing must be manually enabled before starting
 * to decode, because plugins need to setup appropriate bufs. (to be changed)
 *
 * segmented/layered layouts handle mixing vgmstream sample bufs on their own.
 */

mixer_t* mixer_init(int channels) {
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

void mixer_free(mixer_t* mixer) {
    if (!mixer) return;

    free(mixer->mixbuf);
    free(mixer);
}

void mixer_update_channel(mixer_t* mixer) {
    if (!mixer) return;

    /* lame hack for dual stereo, but dual stereo is pretty hack-ish to begin with */
    mixer->mixing_channels++;
    mixer->output_channels++;
}

bool mixer_is_active(mixer_t* mixer) {
    /* no support or not need to apply */
    if (!mixer || !mixer->active)
        return false;

    if (mixer->chain_count > 0)
        return true;

    if (mixer->force_type != SFMT_NONE)
        return true;

    return false;
}

// TODO: probably could be pre-initialized
static void setup_mixbuf(mixer_t* mixer, sbuf_t* sbuf) {
    sbuf_t* smix = &mixer->smix;

    // mixbuf (float) can be interpreted as F16, for 1:1 mapping with PCM16 (and possibly less rounding errors with mixops)
    // for PCM24 regular float seems ok and 1:1 as well
    if (sbuf->fmt == SFMT_S16)
        sbuf_init(smix, SFMT_F16, mixer->mixbuf, sbuf->filled, sbuf->channels);
    else
        sbuf_init(smix, sbuf->fmt, mixer->mixbuf, sbuf->filled, sbuf->channels);

    // remix to temp buf (somehow using float buf rather than int32 is faster?)
    sbuf_copy_segments(smix, sbuf, sbuf->filled);
}

static void setup_outbuf(mixer_t* mixer, sbuf_t* sbuf) {
    sbuf_t* smix = &mixer->smix; //TODO: probably could be pre-initialized

    // setup + remix to output buf (buf is expected to be big enough to handle config)
    sbuf->channels = mixer->output_channels;
    sbuf->filled = 0;
    smix->channels = mixer->output_channels;
    if (mixer->force_type) {
        sbuf->fmt = mixer->force_type;
    }

    sbuf_copy_segments(sbuf, smix, smix->filled);
}

void mixer_process(mixer_t* mixer, sbuf_t* sbuf, int32_t current_pos) {

    // external
    //if (!mixer_is_active(mixer))
    //    return;

#if 0
    // TODO: not possible to copy from src to dst directly at the moment, since src doubles as dst
    // optimize copy only ops to skip temp buffer
    if (mixer->chain_count == 0 && mixer->force_type != SFMT_NONE) {
        ...
    }
#endif

    // try to skip if no fades apply (set but does nothing yet) + only has fades 
    // (could be done in mix op but avoids upgrading bufs in some cases)
    if (mixer->has_fade) {
        //;VGM_LOG("MIX: fade test %i, %i\n", data->has_non_fade, mixer_op_fade_is_active(data, current_pos, current_pos + sample_count));
        if (!mixer->has_non_fade && !mixer_op_fade_is_active(mixer, current_pos, current_pos + sbuf->filled))
            return;
    }

    mixer->current_subpos = current_pos;

    setup_mixbuf(mixer, sbuf);

    // apply mixing ops in order. channels in mixers may increase or decrease per op (set in sbuf)
    // - 2ch w/ "1+2,1u" = ch1+ch2, ch1(add and push rest) = 3ch: ch1' ch1+ch2 ch2
    // - 2ch w/ "1u"     = downmix to 1ch (current_channels decreases once)
    for (int m = 0; m < mixer->chain_count; m++) {
        mix_op_t* mix = &mixer->chain[m];

        //TO-DO: set callback
        switch(mix->type) {
            case MIX_SWAP:      mixer_op_swap(mixer, mix); break;
            case MIX_ADD:       mixer_op_add(mixer, mix); break;
            case MIX_VOLUME:    mixer_op_volume(mixer, mix); break;
            case MIX_LIMIT:     mixer_op_limit(mixer, mix); break;
            case MIX_UPMIX:     mixer_op_upmix(mixer, mix); break;
            case MIX_DOWNMIX:   mixer_op_downmix(mixer, mix); break;
            case MIX_KILLMIX:   mixer_op_killmix(mixer, mix); break;
            case MIX_FADE:      mixer_op_fade(mixer, mix);
            default:
                break;
        }
    }

    setup_outbuf(mixer, sbuf);
}
