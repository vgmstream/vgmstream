#include "../vgmstream.h"
//#include "../layout/layout.h"
//#include "render.h"
//#include "decode.h"
//#include "mixing.h"
#include "plugins.h"



int vgmstream_get_play_forever(VGMSTREAM* vgmstream) {
    return vgmstream->config.play_forever;
}

void vgmstream_set_play_forever(VGMSTREAM* vgmstream, int enabled) {
    /* sometimes we need to enable/disable right before playback
     * (play config is left untouched, should mix ok as this flag is only used during
     * render, while config is always prepared as if play forever wasn't enabled) */
    vgmstream->config.play_forever = enabled;
    setup_vgmstream(vgmstream); /* update config */
}

int32_t vgmstream_get_samples(VGMSTREAM* vgmstream) {
    if (!vgmstream->config_enabled || !vgmstream->config.config_set)
        return vgmstream->num_samples;
    return vgmstream->pstate.play_duration;
}

/*****************************************************************************/

/* apply config like forced loops */
static void setup_state_modifiers(VGMSTREAM* vgmstream) {
    play_config_t* pc = &vgmstream->config;

    /* apply final config */
    if (pc->really_force_loop) {
        vgmstream_force_loop(vgmstream, true, 0, vgmstream->num_samples);
    }
    if (pc->force_loop && !vgmstream->loop_flag) {
        vgmstream_force_loop(vgmstream, true, 0, vgmstream->num_samples);
    }
    if (pc->ignore_loop) {
        vgmstream_force_loop(vgmstream, false, 0, 0);
    }

    if (!vgmstream->loop_flag) {
        pc->play_forever = false;
    }
    if (pc->play_forever) {
        pc->ignore_fade = false;
    }


    /* loop N times, but also play stream end instead of fading out */
    if (pc->ignore_fade) {
        vgmstream_set_loop_target(vgmstream, (int)pc->loop_count);
        pc->fade_time = 0;
        pc->fade_delay = 0;
    }
}

/* apply config like trims */
static void setup_state_processing(VGMSTREAM* vgmstream) {
    play_state_t* ps = &vgmstream->pstate;
    play_config_t* pc = &vgmstream->config;
    double sample_rate = vgmstream->sample_rate;

    /* time to samples */
    if (pc->pad_begin_s)
        pc->pad_begin = pc->pad_begin_s * sample_rate;
    if (pc->pad_end_s)
        pc->pad_end = pc->pad_end_s * sample_rate;
    if (pc->trim_begin_s)
        pc->trim_begin = pc->trim_begin_s * sample_rate;
    if (pc->trim_end_s)
        pc->trim_end = pc->trim_end_s * sample_rate;
    if (pc->body_time_s)
        pc->body_time = pc->body_time_s * sample_rate;
    //todo fade time also set to samples

    /* samples before all decode */
    ps->pad_begin_duration = pc->pad_begin;

    /* removed samples from first decode */
    ps->trim_begin_duration = pc->trim_begin;

    /* main samples part */
    ps->body_duration = 0;
    if (pc->body_time) {
        ps->body_duration += pc->body_time; /* whether it loops or not */
    }
    else if (vgmstream->loop_flag) {
        double loop_count = 1.0;
        if (pc->loop_count_set) /* may set 0.0 on purpose I guess */
            loop_count = pc->loop_count;

        ps->body_duration += vgmstream->loop_start_sample;
        if (pc->ignore_fade) {
            ps->body_duration += (vgmstream->loop_end_sample - vgmstream->loop_start_sample) * (int)loop_count;
            ps->body_duration += (vgmstream->num_samples - vgmstream->loop_end_sample);
        }
        else {
            ps->body_duration += (vgmstream->loop_end_sample - vgmstream->loop_start_sample) * loop_count;
        }
    }
    else {
        ps->body_duration += vgmstream->num_samples;
    }

    /* samples from some modify body */
    if (pc->trim_begin)
        ps->body_duration -= pc->trim_begin;
    if (pc->trim_end)
        ps->body_duration -= pc->trim_end;
    if (pc->fade_delay && vgmstream->loop_flag)
        ps->body_duration += pc->fade_delay * vgmstream->sample_rate;

    /* samples from fade part */
    if (pc->fade_time && vgmstream->loop_flag)
        ps->fade_duration = pc->fade_time * vgmstream->sample_rate;

    /* samples from last part (anything beyond this is empty, unless play forever is set) */
    ps->pad_end_duration = pc->pad_end;

    /* final count */
    ps->play_duration = ps->pad_begin_duration + ps->body_duration + ps->fade_duration + ps->pad_end_duration;
    ps->play_position = 0;

    /* values too big can overflow, just ignore */
    if (ps->pad_begin_duration < 0)
        ps->pad_begin_duration = 0;
    if (ps->body_duration < 0)
        ps->body_duration = 0;
    if (ps->fade_duration < 0)
        ps->fade_duration = 0;
    if (ps->pad_end_duration < 0)
        ps->pad_end_duration = 0;
    if (ps->play_duration < 0)
        ps->play_duration = 0;

    ps->pad_begin_left = ps->pad_begin_duration;
    ps->trim_begin_left = ps->trim_begin_duration;
    ps->fade_left = ps->fade_duration;
    ps->fade_start = ps->pad_begin_duration + ps->body_duration;
  //ps->pad_end_left = ps->pad_end_duration;
    ps->pad_end_start = ps->fade_start + ps->fade_duration;
}

/* apply play config to internal state */
void setup_vgmstream_play_state(VGMSTREAM* vgmstream) {
    if (!vgmstream->config.config_set)
        return;

    setup_state_modifiers(vgmstream);
    setup_state_processing(vgmstream);
}
