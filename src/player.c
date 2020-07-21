#include "vgmstream.h"
#include "plugins.h"


int vgmstream_get_play_forever(VGMSTREAM* vgmstream) {
    return vgmstream->config.play_forever;
}
int32_t vgmstream_get_samples(VGMSTREAM* vgmstream) {
    if (!vgmstream->config_set)
        return vgmstream->num_samples;
    return vgmstream->pstate.play_duration;
}

/*****************************************************************************/

static void setup_state_vgmstream(VGMSTREAM* vgmstream) {
    play_state_t* ps = &vgmstream->pstate;
    play_config_t* pc = &vgmstream->config;


    ps->pad_begin_duration = pc->pad_begin;
    ps->pad_begin_start = 0;
    ps->pad_begin_end = ps->pad_begin_start + ps->pad_begin_duration;

    ps->trim_begin_duration = pc->trim_begin;
    ps->trim_begin_start = ps->pad_begin_end;
    ps->trim_begin_end = ps->trim_begin_start + ps->trim_begin_duration;

    /* main samples part */
    ps->body_duration = 0;
    if (pc->target_time) {
        ps->body_duration += pc->target_time; /* wheter it loops or not */
    }
    else if (vgmstream->loop_flag) {
        ps->body_duration += vgmstream->loop_start_sample;
        if (pc->ignore_fade) {
            ps->body_duration += (vgmstream->loop_end_sample - vgmstream->loop_start_sample) * (int)pc->loop_count;
            ps->body_duration += (vgmstream->num_samples - vgmstream->loop_end_sample);
        }
        else {
            ps->body_duration += (vgmstream->loop_end_sample - vgmstream->loop_start_sample) * pc->loop_count;
        }
    }
    else {
        ps->body_duration += vgmstream->num_samples;
    }

    /* no need to apply in real time */
    if (pc->trim_end)
        ps->body_duration -= pc->trim_end;
    if (pc->fade_delay && vgmstream->loop_flag)
        ps->body_duration += pc->fade_delay * vgmstream->sample_rate;
    if (ps->body_duration < 0) /* ? */
        ps->body_duration = 0;
    ps->body_start = ps->trim_begin_end;
    ps->body_end = ps->body_start + ps->body_duration;

    if (pc->fade_time && vgmstream->loop_flag)
        ps->fade_duration = pc->fade_time * vgmstream->sample_rate;
    ps->fade_start = ps->body_end;
    ps->fade_end = ps->fade_start + ps->fade_duration;

    ps->pad_end_duration = pc->pad_end;
    ps->pad_end_start = ps->fade_end;
    ps->pad_end_end = ps->pad_end_start + ps->pad_end_duration;

    /* final count */
    ps->play_duration = ps->pad_end_end;
    ps->play_position = 0;

    /* other info */
    vgmstream_mixing_enable(vgmstream, 0, &ps->input_channels, &ps->output_channels);

    VGM_LOG("**%i, %i, %i\n", ps->body_duration, ps->fade_duration, ps->play_duration);//todo
}

static void load_player_config(VGMSTREAM* vgmstream, play_config_t* def, vgmstream_cfg_t* vcfg) {
    def->play_forever = vcfg->play_forever;
    def->ignore_loop = vcfg->ignore_loop;
    def->force_loop = vcfg->force_loop;
    def->really_force_loop = vcfg->really_force_loop;
    def->ignore_fade = vcfg->ignore_fade;
    def->loop_count = vcfg->loop_times;  //todo loop times
    def->fade_delay = vcfg->fade_delay;
    def->fade_time = vcfg->fade_period; //todo loop period
}

static void load_internal_config(VGMSTREAM* vgmstream, play_config_t* def, play_config_t* tcfg) {
    /* loop limit: txtp #L > txtp #l > player #L > player #l */
    if (tcfg->play_forever) {
        def->play_forever = 1;
        def->ignore_loop = 0;
    }
    if (tcfg->loop_count_set) {
        def->ignore_loop = 0;
        def->loop_count = tcfg->loop_count;
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
}


void vgmstream_apply_config(VGMSTREAM* vgmstream, vgmstream_cfg_t* vcfg) {
    play_config_t defs = {0};
    play_config_t* def = &defs; /* for convenience... */
    play_config_t* tcfg = &vgmstream->config;


    load_player_config(vgmstream, def, vcfg);

    if (!vcfg->disable_config_override)
        load_internal_config(vgmstream, def, tcfg);


    /* apply final config */
    if (def->really_force_loop) {
        vgmstream_force_loop(vgmstream, 1, 0,vgmstream->num_samples);
    }
    if (def->force_loop && !vgmstream->loop_flag) {
        vgmstream_force_loop(vgmstream, 1, 0,vgmstream->num_samples);
    }
    if (def->ignore_loop) {
        vgmstream_force_loop(vgmstream, 0, 0,0);
    }

    /* remove non-compatible options */
    if (!vcfg->allow_play_forever)
        def->play_forever = 0;

    if (!vgmstream->loop_flag) {
        def->play_forever = 0;
    }
    if (def->play_forever) {
        def->ignore_fade = 0;
    }

    /* loop N times, but also play stream end instead of fading out */
    if (def->ignore_fade) {
        vgmstream_set_loop_target(vgmstream, (int)def->loop_count);
        def->fade_time = 0;
        def->fade_delay = 0;
    }


    /* copy final config back */
     *tcfg = *def;
     vgmstream->config_set = 1;

     setup_state_vgmstream(vgmstream);
     setup_vgmstream(vgmstream); /* save current config for reset */
}

/*****************************************************************************/

void fade_vgmstream(VGMSTREAM* vgmstream, sample_t* buf, int samples_done) {
    play_state_t* ps = &vgmstream->pstate;
    play_config_t* pc = &vgmstream->config;

    if (!ps->fade_duration || pc->play_forever)
        return;
    if (ps->play_position + samples_done < ps->fade_start)
        return;
    if (ps->play_position > ps->fade_end)
        return;

    {
        int s, ch;
        int channels = ps->output_channels;
        int sample_start, fade_pos;

        if (ps->play_position < ps->fade_start) {
            sample_start = samples_done - (ps->play_position + samples_done - ps->fade_start);
            fade_pos = 0;
        }
        else {
            sample_start = 0;
            fade_pos = ps->play_position - ps->fade_start;
        }

        //TODO: use delta fadedness to improve performance?
        for (s = sample_start; s < samples_done; s++, fade_pos++) {
            double fadedness = (double)(ps->fade_duration - fade_pos) / ps->fade_duration;
            for (ch = 0; ch < channels; ch++) {
                buf[s*channels + ch] = (sample_t)buf[s*channels + ch] * fadedness;
            }
        }
    }
}
