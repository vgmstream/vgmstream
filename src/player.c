#include "vgmstream.h"
#include "mixing.h"
#include "plugins.h"


int vgmstream_get_play_forever(VGMSTREAM* vgmstream) {
    return vgmstream->config.play_forever;
}

int32_t vgmstream_get_samples(VGMSTREAM* vgmstream) {
    if (!vgmstream->config_enabled || !vgmstream->config.config_set)
        return vgmstream->num_samples;
    return vgmstream->pstate.play_duration;
}

/*****************************************************************************/

static void setup_state_modifiers(VGMSTREAM* vgmstream) {
    play_config_t* pc = &vgmstream->config;

    /* apply final config */
    if (pc->really_force_loop) {
        vgmstream_force_loop(vgmstream, 1, 0,vgmstream->num_samples);
    }
    if (pc->force_loop && !vgmstream->loop_flag) {
        vgmstream_force_loop(vgmstream, 1, 0,vgmstream->num_samples);
    }
    if (pc->ignore_loop) {
        vgmstream_force_loop(vgmstream, 0, 0,0);
    }

    if (!vgmstream->loop_flag) {
        pc->play_forever = 0;
    }
    if (pc->play_forever) {
        pc->ignore_fade = 0;
    }


    /* loop N times, but also play stream end instead of fading out */
    if (pc->ignore_fade) {
        vgmstream_set_loop_target(vgmstream, (int)pc->loop_count);
        pc->fade_time = 0;
        pc->fade_delay = 0;
    }
}

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
    ps->pad_begin_left = pc->pad_begin;

    /* removed samples from first decode */
    ps->trim_begin_left = pc->trim_begin;

    /* main samples part */
    ps->body_left = 0;
    if (pc->body_time) {
        ps->body_left += pc->body_time; /* whether it loops or not */
    }
    else if (vgmstream->loop_flag) {
        double loop_count = 1.0;
        if (pc->loop_count_set) /* may set 0.0 on purpose I guess */
            loop_count = pc->loop_count;

        ps->body_left += vgmstream->loop_start_sample;
        if (pc->ignore_fade) {
            ps->body_left += (vgmstream->loop_end_sample - vgmstream->loop_start_sample) * (int)loop_count;
            ps->body_left += (vgmstream->num_samples - vgmstream->loop_end_sample);
        }
        else {
            ps->body_left += (vgmstream->loop_end_sample - vgmstream->loop_start_sample) * loop_count;
        }
    }
    else {
        ps->body_left += vgmstream->num_samples;
    }

    /* samples from some modify body */
    if (pc->trim_begin)
        ps->body_left -= pc->trim_begin;
    if (pc->trim_end)
        ps->body_left -= pc->trim_end;
    if (pc->fade_delay && vgmstream->loop_flag)
        ps->body_left += pc->fade_delay * vgmstream->sample_rate;

    /* samples from fade part */
    if (pc->fade_time && vgmstream->loop_flag)
        ps->fade_duration = pc->fade_time * vgmstream->sample_rate;
    ps->fade_start = ps->pad_begin_left + ps->body_left;
    ps->fade_left = ps->fade_duration;

    /* samples from last part */
    ps->pad_end_left = pc->pad_end;

    /* todo if play forever: ignore some? */


    /* final count */
    ps->play_duration = ps->pad_begin_left + ps->body_left + ps->fade_left + ps->pad_end_left;
    ps->play_position = 0;

    /* values too big can overflow, just ignore */
    if (ps->pad_begin_left < 0)
        ps->pad_begin_left = 0;
    if (ps->body_left < 0)
        ps->body_left = 0;
    if (ps->fade_left < 0)
        ps->fade_left = 0;
    if (ps->pad_end_left < 0)
        ps->pad_end_left = 0;
    if (ps->play_duration < 0)
        ps->play_duration = 0;


    /* other info (updated once mixing is enabled) */
    ps->input_channels = vgmstream->channels;
    ps->output_channels = vgmstream->channels;
}

void setup_state_vgmstream(VGMSTREAM* vgmstream) {
    if (!vgmstream->config.config_set)
        return;

    setup_state_modifiers(vgmstream);
    setup_state_processing(vgmstream);
    setup_vgmstream(vgmstream); /* save current config for reset */
}


static void copy_time(int* dst_flag, int32_t* dst_time, double* dst_time_s, int* src_flag, int32_t* src_time, double* src_time_s) {
    if (!*src_flag)
        return;
    *dst_flag = 1;
    *dst_time = *src_time;
    *dst_time_s = *src_time_s;
}

//todo reuse in txtp?
static void load_default_config(play_config_t* def, play_config_t* tcfg) {

    /* loop limit: txtp #L > txtp #l > player #L > player #l */
    if (tcfg->play_forever) {
        def->play_forever = 1;
        def->ignore_loop = 0;
    }
    if (tcfg->loop_count_set) {
        def->ignore_loop = 0;
        def->loop_count = tcfg->loop_count;
        def->loop_count_set = 1;
        if (!tcfg->play_forever)
            def->play_forever = 0;
    }

    /* fade priority: #F > #f, #d */
    if (tcfg->ignore_fade) {
        def->ignore_fade = 1;
    }
    if (tcfg->fade_delay_set) {
        def->fade_delay = tcfg->fade_delay;
    }
    if (tcfg->fade_time_set) {
        def->fade_time = tcfg->fade_time;
    }

    /* loop priority: #i > #e > #E */
    if (tcfg->really_force_loop) {
        def->ignore_loop = 0;
        def->force_loop = 0;
        def->really_force_loop = 1;
    }
    if (tcfg->force_loop) {
        def->ignore_loop = 0;
        def->force_loop = 1;
        def->really_force_loop = 0;
    }
    if (tcfg->ignore_loop) {
        def->ignore_loop = 1;
        def->force_loop = 0;
        def->really_force_loop = 0;
    }

    copy_time(&def->pad_begin_set,  &def->pad_begin,    &def->pad_begin_s,      &tcfg->pad_begin_set,   &tcfg->pad_begin,   &tcfg->pad_begin_s);
    copy_time(&def->pad_end_set,    &def->pad_end,      &def->pad_end_s,        &tcfg->pad_end_set,     &tcfg->pad_end,     &tcfg->pad_end_s);
    copy_time(&def->trim_begin_set, &def->trim_begin,   &def->trim_begin_s,     &tcfg->trim_begin_set,  &tcfg->trim_begin,  &tcfg->trim_begin_s);
    copy_time(&def->trim_end_set,   &def->trim_end,     &def->trim_end_s,       &tcfg->trim_end_set,    &tcfg->trim_end,    &tcfg->trim_end_s);
    copy_time(&def->body_time_set,  &def->body_time,    &def->body_time_s,      &tcfg->body_time_set,   &tcfg->body_time,   &tcfg->body_time_s);
}

static void load_player_config(play_config_t* def, vgmstream_cfg_t* vcfg) {
    def->play_forever = vcfg->play_forever;
    def->ignore_loop = vcfg->ignore_loop;
    def->force_loop = vcfg->force_loop;
    def->really_force_loop = vcfg->really_force_loop;
    def->ignore_fade = vcfg->ignore_fade;

    def->loop_count = vcfg->loop_times;  //todo loop times
    def->loop_count_set = 1;
    def->fade_delay = vcfg->fade_delay;
    def->fade_delay_set = 1;
    def->fade_time = vcfg->fade_period; //todo loop period
    def->fade_time_set = 1;
}

void vgmstream_apply_config(VGMSTREAM* vgmstream, vgmstream_cfg_t* vcfg) {
    play_config_t defs = {0};
    play_config_t* def = &defs; /* for convenience... */
    play_config_t* tcfg = &vgmstream->config;


    load_player_config(def, vcfg);
    def->config_set = 1;

    if (!vcfg->disable_config_override)
        load_default_config(def, tcfg);

    if (!vcfg->allow_play_forever)
        def->play_forever = 0;

    /* copy final config back */
     *tcfg = *def;

     vgmstream->config_enabled = def->config_set;
     setup_state_vgmstream(vgmstream);
}

/*****************************************************************************/

void render_fade(VGMSTREAM* vgmstream, sample_t* buf, int samples_done) {
    play_state_t* ps = &vgmstream->pstate;
    play_config_t* pc = &vgmstream->config;

    if (!ps->fade_left || pc->play_forever)
        return;
    if (ps->play_position + samples_done < ps->fade_start)
        return; /* not yet */

    {
        int s, ch,  start, pos;
        int channels = ps->output_channels;

        if (ps->play_position < ps->fade_start) {
            start = samples_done - (ps->play_position + samples_done - ps->fade_start);
            pos = 0;
        }
        else {
            start = 0;
            pos = ps->play_position - ps->fade_start;
        }

        //TODO: use delta fadedness to improve performance?
        for (s = start; s < samples_done; s++, pos++) {
            double fadedness = (double)(ps->fade_duration - pos) / ps->fade_duration;
            for (ch = 0; ch < channels; ch++) {
                buf[s*channels + ch] = (sample_t)buf[s*channels + ch] * fadedness;
            }
        }

        vgmstream->pstate.fade_left -= (samples_done - start);
    }
}
