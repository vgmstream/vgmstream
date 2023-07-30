#ifndef _MIXING_FADE_H_
#define _MIXING_FADE_H_

#include "mixing_priv.h"
#include <math.h>
#include <limits.h>

#define MIXING_PI   3.14159265358979323846f


static inline int is_fade_active(mixing_data *data, int32_t current_start, int32_t current_end) {
    int i;

    for (i = 0; i < data->mixing_count; i++) {
        mix_command_data *mix = &data->mixing_chain[i];
        int32_t fade_start, fade_end;
        float vol_start = mix->vol_start;

        if (mix->command != MIX_FADE)
            continue;

        /* check is current range falls within a fade
         * (assuming fades were already optimized on add) */
        if (mix->time_pre < 0 && vol_start == 1.0) {
            fade_start = mix->time_start; /* ignore unused */
        }
        else {
            fade_start = mix->time_pre < 0 ? 0 : mix->time_pre;
        }
        fade_end = mix->time_post < 0 ? INT_MAX : mix->time_post;
        
        //;VGM_LOG("MIX: fade test, tp=%i, te=%i, cs=%i, ce=%i\n", mix->time_pre, mix->time_post, current_start, current_end);
        if (current_start < fade_end && current_end > fade_start) {
            //;VGM_LOG("MIX: fade active, cs=%i < fe=%i and ce=%i > fs=%i\n", current_start, fade_end, current_end, fade_start);
            return 1;
        }
    }

    return 0;
}

static inline int32_t get_current_pos(VGMSTREAM* vgmstream, int32_t sample_count) {
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

static inline float get_fade_gain_curve(char shape, float index) {
    float gain;

    /* don't bother doing calcs near 0.0/1.0 */
    if (index <= 0.0001f || index >= 0.9999f) {
        return index;
    }

    //todo optimizations: interleave calcs, maybe use cosf, powf, etc? (with extra defines)

    /* (curve math mostly from SoX/FFmpeg) */
    switch(shape) {
        /* 2.5f in L/E 'pow' is the attenuation factor, where 5.0 (100db) is common but a bit fast
         * (alt calculations with 'exp' from FFmpeg use (factor)*ln(0.1) = -NN.N...  */

        case 'E': /* exponential (for fade-outs, closer to natural decay of sound) */
            //gain = pow(0.1f, (1.0f - index) * 2.5f);
            gain = exp(-5.75646273248511f * (1.0f - index));
            break;
        case 'L': /* logarithmic (inverse of the above, maybe for crossfades) */
            //gain = 1 - pow(0.1f, (index) * 2.5f);
            gain = 1 - exp(-5.75646273248511f * (index));
            break;

        case 'H': /* raised sine wave or cosine wave (for more musical crossfades) */
            gain = (1.0f - cos(index * MIXING_PI)) / 2.0f;
            break;

        case 'Q': /* quarter of sine wave (for musical fades) */
            gain = sin(index * MIXING_PI / 2.0f);
            break;

        case 'p': /* parabola (maybe for crossfades) */
            gain =  1.0f - sqrt(1.0f - index);
            break;
        case 'P': /* inverted parabola (maybe for fades) */
            gain = (1.0f - (1.0f - index) * (1.0f - index));
            break;

        case 'T': /* triangular/linear (simpler/sharper fades) */
        default:
            gain = index;
            break;
    }

    return gain;
}

static int get_fade_gain(mix_command_data *mix, float *out_cur_vol, int32_t current_subpos) {
    float cur_vol = 0.0f;

    if ((current_subpos >= mix->time_pre || mix->time_pre < 0) && current_subpos < mix->time_start) {
        cur_vol = mix->vol_start; /* before */
    }
    else if (current_subpos >= mix->time_end && (current_subpos < mix->time_post || mix->time_post < 0)) {
        cur_vol = mix->vol_end; /* after */
    }
    else if (current_subpos >= mix->time_start && current_subpos < mix->time_end) {
        /* in between */
        float range_vol, range_dur, range_idx, index, gain;

        if (mix->vol_start < mix->vol_end) { /* fade in */
            range_vol = mix->vol_end - mix->vol_start;
            range_dur = mix->time_end - mix->time_start;
            range_idx = current_subpos - mix->time_start;
            index = range_idx / range_dur;
        } else { /* fade out */
            range_vol = mix->vol_end - mix->vol_start;
            range_dur = mix->time_end - mix->time_start;
            range_idx = mix->time_end - current_subpos;
            index = range_idx / range_dur;
        }

        /* Fading is done like this:
         * - find current position within fade duration
         * - get linear % (or rather, index from 0.0 .. 1.0) of duration
         * - apply shape to % (from linear fade to curved fade)
         * - get final volume for that point
         *
         * Roughly speaking some curve shapes are better for fades (decay rate is more natural
         * sounding in that highest to mid/low happens faster but low to lowest takes more time,
         * kinda like a gunshot or bell), and others for crossfades (decay of fade-in + fade-out
         * is adjusted so that added volume level stays constant-ish).
         *
         * As curves can fade in two ways ('normal' and curving 'the other way'), they are adjusted
         * to get 'normal' shape on both fades (by reversing index and making 1 - gain), thus some
         * curves are complementary (exponential fade-in ~= logarithmic fade-out); the following
         * are described taking fade-in = normal.
         */
        gain = get_fade_gain_curve(mix->shape, index);

        if (mix->vol_start < mix->vol_end) {  /* fade in */
            cur_vol = mix->vol_start + range_vol * gain;
        } else { /* fade out */
            cur_vol = mix->vol_end - range_vol * gain; //mix->vol_start - range_vol * (1 - gain);
        }
    }
    else {
        /* fade is outside reach */
        goto fail;
    }

    *out_cur_vol = cur_vol;
    return 1;
fail:
    return 0;
}

#endif
