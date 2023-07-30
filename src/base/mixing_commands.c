#include "../vgmstream.h"
#include "../util/channel_mappings.h"
#include "mixing.h"
#include "mixing_priv.h"
#include <math.h>
#include <limits.h>


static int add_mixing(VGMSTREAM* vgmstream, mix_command_data *mix) {
    mixing_data *data = vgmstream->mixing_data;
    if (!data) return 0;


    if (data->mixing_on) {
        VGM_LOG("MIX: ignoring new mixes when mixing active\n");
        return 0; /* to avoid down/upmixing after activation */
    }

    if (data->mixing_count + 1 > data->mixing_size) {
        VGM_LOG("MIX: too many mixes\n");
        return 0;
    }

    data->mixing_chain[data->mixing_count] = *mix; /* memcpy */
    data->mixing_count++;


    if (mix->command == MIX_FADE) {
        data->has_fade = 1;
    }
    else {
        data->has_non_fade = 1;
    }

    //;VGM_LOG("MIX: total %i\n", data->mixing_count);
    return 1;
}


void mixing_push_swap(VGMSTREAM* vgmstream, int ch_dst, int ch_src) {
    mixing_data *data = vgmstream->mixing_data;
    mix_command_data mix = {0};

    if (ch_dst < 0 || ch_src < 0 || ch_dst == ch_src) return;
    if (!data || ch_dst >= data->output_channels || ch_src >= data->output_channels) return;
    mix.command = MIX_SWAP;
    mix.ch_dst = ch_dst;
    mix.ch_src = ch_src;

    add_mixing(vgmstream, &mix);
}

void mixing_push_add(VGMSTREAM* vgmstream, int ch_dst, int ch_src, double volume) {
    mixing_data *data = vgmstream->mixing_data;
    mix_command_data mix = {0};
    if (!data) return;

    //if (volume < 0.0) return; /* negative volume inverts the waveform */
    if (volume == 0.0) return; /* ch_src becomes silent and nothing is added */
    if (ch_dst < 0 || ch_src < 0) return;
    if (!data || ch_dst >= data->output_channels || ch_src >= data->output_channels) return;

    mix.command = (volume == 1.0) ?  MIX_ADD_COPY : MIX_ADD;
    mix.ch_dst = ch_dst;
    mix.ch_src = ch_src;
    mix.vol = volume;

    //;VGM_LOG("MIX: add %i+%i*%f\n", ch_dst,ch_src,volume);
    add_mixing(vgmstream, &mix);
}

void mixing_push_volume(VGMSTREAM* vgmstream, int ch_dst, double volume) {
    mixing_data *data = vgmstream->mixing_data;
    mix_command_data mix = {0};

    //if (ch_dst < 0) return; /* means all channels */
    //if (volume < 0.0) return; /* negative volume inverts the waveform */
    if (volume == 1.0) return; /* no change */
    if (!data || ch_dst >= data->output_channels) return;

    mix.command = MIX_VOLUME; //if (volume == 0.0) MIX_VOLUME0 /* could simplify */
    mix.ch_dst = ch_dst;
    mix.vol = volume;

    //;VGM_LOG("MIX: volume %i*%f\n", ch_dst,volume);
    add_mixing(vgmstream, &mix);
}

void mixing_push_limit(VGMSTREAM* vgmstream, int ch_dst, double volume) {
    mixing_data *data = vgmstream->mixing_data;
    mix_command_data mix = {0};

    //if (ch_dst < 0) return; /* means all channels */
    if (volume < 0.0) return;
    if (volume == 1.0) return; /* no actual difference */
    if (!data || ch_dst >= data->output_channels) return;
    //if (volume == 0.0) return; /* dumb but whatevs */

    mix.command = MIX_LIMIT;
    mix.ch_dst = ch_dst;
    mix.vol = volume;

    add_mixing(vgmstream, &mix);
}

void mixing_push_upmix(VGMSTREAM* vgmstream, int ch_dst) {
    mixing_data *data = vgmstream->mixing_data;
    mix_command_data mix = {0};
    int ok;

    if (ch_dst < 0) return;
    if (!data || ch_dst > data->output_channels || data->output_channels +1 > VGMSTREAM_MAX_CHANNELS) return;
    /* dst can be == output_channels here, since we are inserting */

    mix.command = MIX_UPMIX;
    mix.ch_dst = ch_dst;

    ok = add_mixing(vgmstream, &mix);
    if (ok) {
        data->output_channels += 1;
        if (data->mixing_channels < data->output_channels)
            data->mixing_channels = data->output_channels;
    }
}

void mixing_push_downmix(VGMSTREAM* vgmstream, int ch_dst) {
    mixing_data *data = vgmstream->mixing_data;
    mix_command_data mix = {0};
    int ok;

    if (ch_dst < 0) return;
    if (!data || ch_dst >= data->output_channels || data->output_channels - 1 < 1) return;

    mix.command = MIX_DOWNMIX;
    mix.ch_dst = ch_dst;

    ok = add_mixing(vgmstream, &mix);
    if (ok) {
        data->output_channels -= 1;
    }
}

void mixing_push_killmix(VGMSTREAM* vgmstream, int ch_dst) {
    mixing_data *data = vgmstream->mixing_data;
    mix_command_data mix = {0};
    int ok;

    if (ch_dst <= 0) return; /* can't kill from first channel */
    if (!data || ch_dst >= data->output_channels) return;

    mix.command = MIX_KILLMIX;
    mix.ch_dst = ch_dst;

    //;VGM_LOG("MIX: killmix %i\n", ch_dst);
    ok = add_mixing(vgmstream, &mix);
    if (ok) {
        data->output_channels = ch_dst; /* clamp channels */
    }
}


static mix_command_data* get_last_fade(mixing_data *data, int target_channel) {
    int i;
    for (i = data->mixing_count; i > 0; i--) {
        mix_command_data *mix = &data->mixing_chain[i-1];
        if (mix->command != MIX_FADE)
            continue;
        if (mix->ch_dst == target_channel)
            return mix;
    }

    return NULL;
}


void mixing_push_fade(VGMSTREAM* vgmstream, int ch_dst, double vol_start, double vol_end, char shape,
        int32_t time_pre, int32_t time_start, int32_t time_end, int32_t time_post) {
    mixing_data *data = vgmstream->mixing_data;
    mix_command_data mix = {0};
    mix_command_data *mix_prev;


    //if (ch_dst < 0) return; /* means all channels */
    if (!data || ch_dst >= data->output_channels) return;
    if (time_pre > time_start || time_start > time_end || (time_post >= 0 && time_end > time_post)) return;
    if (time_start < 0 || time_end < 0) return;
    //if (time_pre < 0 || time_post < 0) return; /* special meaning of file start/end */
    //if (vol_start == vol_end) /* weird but let in case of being used to cancel others fades... maybe? */

    if (shape == '{' || shape == '}')
        shape = 'E';
    if (shape == '(' || shape == ')')
        shape = 'H';

    mix.command = MIX_FADE;
    mix.ch_dst = ch_dst;
    mix.vol_start = vol_start;
    mix.vol_end = vol_end;
    mix.shape = shape;
    mix.time_pre = time_pre;
    mix.time_start = time_start;
    mix.time_end = time_end;
    mix.time_post = time_post;


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
    mix_prev = get_last_fade(data, mix.ch_dst);
    if (mix_prev == NULL) {
        if (vol_start == 1.0 && time_pre < 0)
            time_pre = time_start; /* fade-out helds default volume before fade start can be clamped */
        if (vol_end == 1.0 && time_post < 0)
            time_post = time_end; /* fade-in helds default volume after fade end can be clamped */
    }
    else if (mix_prev->time_post < 0 || mix.time_pre < 0) {
        int is_prev = 1;
        /* test if prev is really cancelled by this */
        if ((mix_prev->time_end > mix.time_start) ||
            (mix_prev->time_post >= 0 && mix_prev->time_post > mix.time_start) ||
            (mix.time_pre >= 0 && mix.time_pre < mix_prev->time_end))
            is_prev = 0;

        if (is_prev) {
            /* change negative values to actual points */
            if (mix_prev->time_post < 0 && mix.time_pre < 0) {
                mix_prev->time_post = mix_prev->time_end;
                mix.time_pre = mix_prev->time_post;
            }

            if (mix_prev->time_post >= 0 && mix.time_pre < 0) {
                mix.time_pre = mix_prev->time_post;
            }
            else if (mix_prev->time_post < 0 && mix.time_pre >= 0) {
                mix_prev->time_post = mix.time_pre;
            }
            /* else: both define start/ends, do nothing */
        }
        /* should only modify prev if add_mixing but meh */
    }

    //;VGM_LOG("MIX: fade %i^%f~%f=%c@%i~%i~%i~%i\n", ch_dst, vol_start, vol_end, shape, time_pre, time_start, time_end, time_post);
    add_mixing(vgmstream, &mix);
}
