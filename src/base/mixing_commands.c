#include "../vgmstream.h"
#include "mixing.h"
#include "mixer_priv.h"
#include <math.h>
#include <limits.h>


static bool add_mixing(VGMSTREAM* vgmstream, mix_op_t* op) {
    mixer_t* mixer = vgmstream->mixer;
    if (!mixer)
        return false;


    if (mixer->active) {
        VGM_LOG("MIX: ignoring new ops when mixer is active\n");
        return false; /* to avoid down/upmixing after activation */
    }

    if (mixer->chain_count + 1 > mixer->chain_size) {
        VGM_LOG("MIX: too many mixes\n");
        return false;
    }

    mixer->chain[mixer->chain_count] = *op; /* memcpy */
    mixer->chain_count++;


    if (op->type == MIX_FADE) {
        mixer->has_fade = true;
    }
    else {
        mixer->has_non_fade = true;
    }

    //;VGM_LOG("MIX: total %i\n", data->chain_count);
    return true;
}


void mixing_push_swap(VGMSTREAM* vgmstream, int ch_dst, int ch_src) {
    mixer_t* mixer = vgmstream->mixer;
    mix_op_t op = {0};

    if (ch_dst < 0 || ch_src < 0 || ch_dst == ch_src) return;
    if (!mixer || ch_dst >= mixer->output_channels || ch_src >= mixer->output_channels) return;
    op.type = MIX_SWAP;
    op.ch_dst = ch_dst;
    op.ch_src = ch_src;

    add_mixing(vgmstream, &op);
}

void mixing_push_add(VGMSTREAM* vgmstream, int ch_dst, int ch_src, double volume) {
    mixer_t* mixer = vgmstream->mixer;
    mix_op_t op = {0};
    if (!mixer) return;

    //if (volume < 0.0) return; /* negative volume inverts the waveform */
    if (volume == 0.0) return; /* ch_src becomes silent and nothing is added */
    if (ch_dst < 0 || ch_src < 0) return;
    if (!mixer || ch_dst >= mixer->output_channels || ch_src >= mixer->output_channels) return;

    op.type = MIX_ADD;
    op.ch_dst = ch_dst;
    op.ch_src = ch_src;
    op.vol = volume;

    //;VGM_LOG("MIX: add %i+%i*%f\n", ch_dst,ch_src,volume);
    add_mixing(vgmstream, &op);
}

void mixing_push_volume(VGMSTREAM* vgmstream, int ch_dst, double volume) {
    mixer_t* mixer = vgmstream->mixer;
    mix_op_t op = {0};

    //if (ch_dst < 0) return; /* means all channels */
    //if (volume < 0.0) return; /* negative volume inverts the waveform */
    if (volume == 1.0) return; /* no change */
    if (!mixer || ch_dst >= mixer->output_channels) return;

    op.type = MIX_VOLUME; //if (volume == 0.0) MIX_VOLUME0 /* could simplify */
    op.ch_dst = ch_dst;
    op.vol = volume;

    //;VGM_LOG("MIX: volume %i*%f\n", ch_dst,volume);
    add_mixing(vgmstream, &op);
}

void mixing_push_limit(VGMSTREAM* vgmstream, int ch_dst, double volume) {
    mixer_t* mixer = vgmstream->mixer;
    mix_op_t op = {0};

    //if (ch_dst < 0) return; /* means all channels */
    if (volume < 0.0) return;
    if (volume == 1.0) return; /* no actual difference */
    if (!mixer || ch_dst >= mixer->output_channels) return;
    //if (volume == 0.0) return; /* dumb but whatevs */

    op.type = MIX_LIMIT;
    op.ch_dst = ch_dst;
    op.vol = volume;

    add_mixing(vgmstream, &op);
}

void mixing_push_upmix(VGMSTREAM* vgmstream, int ch_dst) {
    mixer_t* mixer = vgmstream->mixer;
    mix_op_t op = {0};
    int ok;

    if (ch_dst < 0) return;
    if (!mixer || ch_dst > mixer->output_channels || mixer->output_channels +1 > VGMSTREAM_MAX_CHANNELS) return;
    /* dst can be == output_channels here, since we are inserting */

    op.type = MIX_UPMIX;
    op.ch_dst = ch_dst;

    ok = add_mixing(vgmstream, &op);
    if (ok) {
        mixer->output_channels += 1;
        if (mixer->mixing_channels < mixer->output_channels)
            mixer->mixing_channels = mixer->output_channels;
    }
}

void mixing_push_downmix(VGMSTREAM* vgmstream, int ch_dst) {
    mixer_t* mixer = vgmstream->mixer;
    mix_op_t op = {0};
    int ok;

    if (ch_dst < 0) return;
    if (!mixer || ch_dst >= mixer->output_channels || mixer->output_channels - 1 < 1) return;

    op.type = MIX_DOWNMIX;
    op.ch_dst = ch_dst;

    ok = add_mixing(vgmstream, &op);
    if (ok) {
        mixer->output_channels -= 1;
    }
}

void mixing_push_killmix(VGMSTREAM* vgmstream, int ch_dst) {
    mixer_t* mixer = vgmstream->mixer;
    mix_op_t op = {0};

    if (ch_dst <= 0) return; /* can't kill from first channel */
    if (!mixer || ch_dst >= mixer->output_channels) return;

    op.type = MIX_KILLMIX;
    op.ch_dst = ch_dst;

    //;VGM_LOG("MIX: killmix %i\n", ch_dst);
    bool ok = add_mixing(vgmstream, &op);
    if (ok) {
        mixer->output_channels = ch_dst; /* clamp channels */
    }
}


static mix_op_t* get_last_fade(mixer_t* mixer, int target_channel) {
    for (int i = mixer->chain_count; i > 0; i--) {
        mix_op_t* op = &mixer->chain[i-1];
        if (op->type != MIX_FADE)
            continue;
        if (op->ch_dst == target_channel)
            return op;
    }

    return NULL;
}


void mixing_push_fade(VGMSTREAM* vgmstream, int ch_dst, double vol_start, double vol_end, char shape,
        int32_t time_pre, int32_t time_start, int32_t time_end, int32_t time_post) {
    mixer_t* mixer = vgmstream->mixer;
    mix_op_t op = {0};
    mix_op_t* op_prev;


    //if (ch_dst < 0) return; /* means all channels */
    if (!mixer || ch_dst >= mixer->output_channels) return;
    if (time_pre > time_start || time_start > time_end || (time_post >= 0 && time_end > time_post)) return;
    if (time_start < 0 || time_end < 0) return;
    //if (time_pre < 0 || time_post < 0) return; /* special meaning of file start/end */
    //if (vol_start == vol_end) /* weird but let in case of being used to cancel others fades... maybe? */

    if (shape == '{' || shape == '}')
        shape = 'E';
    if (shape == '(' || shape == ')')
        shape = 'H';

    op.type = MIX_FADE;
    op.ch_dst = ch_dst;
    op.vol_start = vol_start;
    op.vol_end = vol_end;
    op.shape = shape;
    op.time_pre = time_pre;
    op.time_start = time_start;
    op.time_end = time_end;
    op.time_post = time_post;


    /* cancel fades and optimize a bit when using negative pre/post:
     * - fades work like this:
     *   <----------|----------|---------->
     *   pre1       start1  end1      post1
     * - when pre and post are set nothing is done (fade is exact and multiple fades may overlap)
     * - when previous fade's post or current fade's pre are negative (meaning file end/start)
     *   they should cancel each other (to allow chaining fade-in + fade-out + fade-in + etc):
     *   <----------|----------|----------| |----------|----------|---------->
     *   pre1       start1  end1      post1 pre2       start2  end2      post2
     * - other cases (previous fade is actually after/in-between current fade) are ignored
     *   as they're uncommon and hard to optimize
     * fades cancel fades of the same channel, and 'all channel' (-1) fades also cancel 'all channels'
     */
    op_prev = get_last_fade(mixer, op.ch_dst);
    if (op_prev == NULL) {
        if (vol_start == 1.0 && time_pre < 0)
            time_pre = time_start; /* fade-out helds default volume before fade start can be clamped */
        if (vol_end == 1.0 && time_post < 0)
            time_post = time_end; /* fade-in helds default volume after fade end can be clamped */
    }
    else if (op_prev->time_post < 0 || op.time_pre < 0) {
        int is_prev = 1;
        /* test if prev is really cancelled by this */
        if ((op_prev->time_end > op.time_start) ||
            (op_prev->time_post >= 0 && op_prev->time_post > op.time_start) ||
            (op.time_pre >= 0 && op.time_pre < op_prev->time_end))
            is_prev = 0;

        if (is_prev) {
            /* change negative values to actual points */
            if (op_prev->time_post < 0 && op.time_pre < 0) {
                op_prev->time_post = op_prev->time_end;
                op.time_pre = op_prev->time_post;
            }

            if (op_prev->time_post >= 0 && op.time_pre < 0) {
                op.time_pre = op_prev->time_post;
            }
            else if (op_prev->time_post < 0 && op.time_pre >= 0) {
                op_prev->time_post = op.time_pre;
            }
            /* else: both define start/ends, do nothing */
        }
        /* should only modify prev if add_mixing but meh */
    }

    //;VGM_LOG("MIX: fade %i^%f~%f=%c@%i~%i~%i~%i\n", ch_dst, vol_start, vol_end, shape, time_pre, time_start, time_end, time_post);
    add_mixing(vgmstream, &op);
}
