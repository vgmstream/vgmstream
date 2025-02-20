#include "mixer_priv.h"
#include <limits.h>
#include <math.h>

//TODO: could precalculate tables + interpolate for some performance gain

#define MIXING_PI   3.14159265358979323846f

static inline float get_fade_gain_curve(char shape, float index) {
    float gain;

    /* don't bother doing calcs near 0.0/1.0 */
    if (index <= 0.0001f || index >= 0.9999f) {
        return index;
    }

    //TODO optimizations: interleave calcs

    /* (curve math mostly from SoX/FFmpeg) */
    switch(shape) {
        /* 2.5f in L/E 'pow' is the attenuation factor, where 5.0 (100db) is common but a bit fast
         * (alt calculations with 'exp' from FFmpeg use (factor)*ln(0.1) = -NN.N...  */

        case 'E': /* exponential (for fade-outs, closer to natural decay of sound) */
            //gain = powf(0.1f, (1.0f - index) * 2.5f);
            gain = expf(-5.75646273248511f * (1.0f - index));
            break;

        case 'L': /* logarithmic (inverse of the above, maybe for crossfades) */
            //gain = 1 - powf(0.1f, (index) * 2.5f);
            gain = 1 - expf(-5.75646273248511f * (index));
            break;

        case 'H': /* raised sine wave or cosine wave (for more musical crossfades) */
            gain = (1.0f - cosf(index * MIXING_PI)) / 2.0f;
            break;

        case 'Q': /* quarter of sine wave (for musical fades) */
            gain = sinf(index * MIXING_PI / 2.0f);
            break;

        case 'p': /* parabola (maybe for crossfades) */
            gain =  1.0f - sqrtf(1.0f - index);
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

static bool get_fade_gain(mix_op_t* op, float* out_cur_vol, int32_t current_subpos) {
    float cur_vol = 0.0f;

    if ((current_subpos >= op->time_pre || op->time_pre < 0) && current_subpos < op->time_start) {
        cur_vol = op->vol_start; /* before */
    }
    else if (current_subpos >= op->time_end && (current_subpos < op->time_post || op->time_post < 0)) {
        cur_vol = op->vol_end; /* after */
    }
    else if (current_subpos >= op->time_start && current_subpos < op->time_end) {
        /* in between */
        float range_vol, range_dur, range_idx, index, gain;

        if (op->vol_start < op->vol_end) { /* fade in */
            range_vol = op->vol_end - op->vol_start;
            range_dur = op->time_end - op->time_start;
            range_idx = current_subpos - op->time_start;
            index = range_idx / range_dur;
        } else { /* fade out */
            range_vol = op->vol_end - op->vol_start;
            range_dur = op->time_end - op->time_start;
            range_idx = op->time_end - current_subpos;
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
        gain = get_fade_gain_curve(op->shape, index);

        if (op->vol_start < op->vol_end) {  /* fade in */
            cur_vol = op->vol_start + range_vol * gain;
        } else { /* fade out */
            cur_vol = op->vol_end - range_vol * gain; //mix->vol_start - range_vol * (1 - gain);
        }
    }
    else {
        /* fade is outside reach */
        return false;
    }

    *out_cur_vol = cur_vol;
    return true;
}

void mixer_op_fade(mixer_t* mixer, mix_op_t* mix) {
    sbuf_t* smix = &mixer->smix;
    float* dst = smix->buf;
    float new_gain = 0.0f;

    int channels = smix->channels;
    int32_t current_subpos = mixer->current_subpos;

    //TODO optimize for case 0?
    for (int s = 0; s < smix->filled; s++) {
        bool fade_applies = get_fade_gain(mix, &new_gain, current_subpos);
        if (!fade_applies) { //TODO optimize?
            dst += channels;
            current_subpos++;
            continue;
        }

        if (mix->ch_dst < 0) {
            for (int ch = 0; ch < channels; ch++) {
                dst[ch] = dst[ch] * new_gain;
            }
        }
        else {
            dst[mix->ch_dst] = dst[mix->ch_dst] * new_gain;
        }

        dst += channels;
        current_subpos++;
    }
}

bool mixer_op_fade_is_active(mixer_t* mixer, int32_t current_start, int32_t current_end) {

    for (int i = 0; i < mixer->chain_count; i++) {
        mix_op_t* mix = &mixer->chain[i];
        int32_t fade_start, fade_end;
        float vol_start = mix->vol_start;

        if (mix->type != MIX_FADE)
            continue;

        /* check is current range falls within a fade
         * (assuming fades were already optimized on add) */
        if (mix->time_pre < 0 && vol_start == 1.0f) {
            fade_start = mix->time_start; /* ignore unused */
        }
        else {
            fade_start = mix->time_pre < 0 ? 0 : mix->time_pre;
        }
        fade_end = mix->time_post < 0 ? INT_MAX : mix->time_post;
        
        //;VGM_LOG("MIX: fade test, tp=%i, te=%i, cs=%i, ce=%i\n", mix->time_pre, mix->time_post, current_start, current_end);
        if (current_start < fade_end && current_end > fade_start) {
            //;VGM_LOG("MIX: fade active, cs=%i < fe=%i and ce=%i > fs=%i\n", current_start, fade_end, current_end, fade_start);
            return true;
        }
    }

    return false;
}
