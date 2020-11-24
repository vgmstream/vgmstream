#include "vgmstream.h"
#include "layout/layout.h"
#include "render.h"
#include "decode.h"
#include "mixing.h"
#include "plugins.h"


/* VGMSTREAM RENDERING
 * Caller asks for N samples in buf. vgmstream then calls layouts, that call decoders, and some optional pre/post-processing.
 * Processing must be enabled externally: padding/trimming (config), mixing (output changes), resampling, etc.
 *
 * - MIXING
 * After decoding sometimes we need to change number of channels, volume, etc. This is applied in order as
 * a mixing chain, and modifies the final buffer (see mixing.c).
 *
 * - CONFIG
 * A VGMSTREAM can work in 2 modes, defaults to simple mode:
 * - simple mode (lib-like): decodes/loops forever and results are controlled externally (fades/max time/etc).
 * - config mode (player-like): everything is internally controlled (pads/trims/time/fades/etc may be applied).
 *
 * It's done this way mainly for compatibility and to enable complex TXTP for layers/segments in selected cases
 * (Wwise emulation). Could apply always some config like begin trim/padding + modify get_vgmstream_samples, but
 * external caller may read loops/samples manually or apply its own config/fade, and changed output would mess it up.
 *
 * To enable config mode it needs 2 steps:
 * - add some internal config settings (via TXTP, or passed by plugin).
 * - enable flag with function (to signal "really delegate all decoding to vgmstream").
 * Once done, plugin should simply decode until max samples (calculated by vgmstream).
 *
 * For complex layouts, behavior of "internal" (single segment/layer) and "external" (main) VGMSTREAMs is
 * a bit complex. Internals' enable flag if play config exists (via TXTP), and this allows each part to be
 * padded/trimmed/set time/loop/etc individually.
 *
 * Config mode in the external VGMSTREAM is mostly straighforward with segments:
 * - each internal is always decoded separatedly (in simple or config mode) and results in N samples
 * - segments may even loop "internally" before moving to next segment (by default they don't)
 * - external's samples is the sum of all segments' N samples
 * - looping, fades, etc then can be applied in the external part normally.
 *
 * With layers it's a bit more complex:
 * - external's samples is the max of all layers
 * - in simple mode external uses internal's looping to loop (for performance)
 * - if layers' config mode is set, external can't rely on internal looping, so it uses it's own
 *
 * Layouts can contain layouts in cascade, so behavior can be a bit hard to understand at times.
 * This mainly applies to TXTP, segments/layers in metas usually don't need to trigger config mode.
 */


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

/* calculate samples based on player's config */
int32_t get_vgmstream_play_samples(double looptimes, double fadeseconds, double fadedelayseconds, VGMSTREAM* vgmstream) {
    if (vgmstream->loop_flag) {
        if (vgmstream->loop_target == (int)looptimes) { /* set externally, as this function is info-only */
            /* Continue playing the file normally after looping, instead of fading.
             * Most files cut abruply after the loop, but some do have proper endings.
             * With looptimes = 1 this option should give the same output vs loop disabled */
            int loop_count = (int)looptimes; /* no half loops allowed */
            return vgmstream->loop_start_sample
                + (vgmstream->loop_end_sample - vgmstream->loop_start_sample) * loop_count
                + (vgmstream->num_samples - vgmstream->loop_end_sample);
        }
        else {
            return vgmstream->loop_start_sample
                + (vgmstream->loop_end_sample - vgmstream->loop_start_sample) * looptimes
                + (fadedelayseconds + fadeseconds) * vgmstream->sample_rate;
        }
    }
    else {
        return vgmstream->num_samples;
    }
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

/*****************************************************************************/

void free_layout(VGMSTREAM* vgmstream) {

    if (vgmstream->layout_type == layout_segmented) {
        free_layout_segmented(vgmstream->layout_data);
    }

    if (vgmstream->layout_type == layout_layered) {
        free_layout_layered(vgmstream->layout_data);
    }
}

void reset_layout(VGMSTREAM* vgmstream) {

    if (vgmstream->layout_type == layout_segmented) {
        reset_layout_segmented(vgmstream->layout_data);
    }

    if (vgmstream->layout_type == layout_layered) {
        reset_layout_layered(vgmstream->layout_data);
    }
}

static int render_layout(sample_t* buf, int32_t sample_count, VGMSTREAM* vgmstream) {

    /* current_sample goes between loop points (if looped) or up to max samples,
     * must detect beyond that decoders would encounter garbage data */

    /* not ">=" to allow layouts to loop in some cases when == happens */
    if (vgmstream->current_sample > vgmstream->num_samples) {
        int channels = vgmstream->channels;

        memset(buf, 0, sample_count * sizeof(sample_t) * channels);
        return sample_count;
    }

    switch (vgmstream->layout_type) {
        case layout_interleave:
            render_vgmstream_interleave(buf, sample_count, vgmstream);
            break;
        case layout_none:
            render_vgmstream_flat(buf, sample_count, vgmstream);
            break;
        case layout_blocked_mxch:
        case layout_blocked_ast:
        case layout_blocked_halpst:
        case layout_blocked_xa:
        case layout_blocked_ea_schl:
        case layout_blocked_ea_1snh:
        case layout_blocked_caf:
        case layout_blocked_wsi:
        case layout_blocked_str_snds:
        case layout_blocked_ws_aud:
        case layout_blocked_matx:
        case layout_blocked_dec:
        case layout_blocked_vs:
        case layout_blocked_mul:
        case layout_blocked_gsb:
        case layout_blocked_xvas:
        case layout_blocked_thp:
        case layout_blocked_filp:
        case layout_blocked_ivaud:
        case layout_blocked_ea_swvr:
        case layout_blocked_adm:
        case layout_blocked_bdsp:
        case layout_blocked_tra:
        case layout_blocked_ps2_iab:
        case layout_blocked_vs_str:
        case layout_blocked_rws:
        case layout_blocked_hwas:
        case layout_blocked_ea_sns:
        case layout_blocked_awc:
        case layout_blocked_vgs:
        case layout_blocked_xwav:
        case layout_blocked_xvag_subsong:
        case layout_blocked_ea_wve_au00:
        case layout_blocked_ea_wve_ad10:
        case layout_blocked_sthd:
        case layout_blocked_h4m:
        case layout_blocked_xa_aiff:
        case layout_blocked_vs_square:
        case layout_blocked_vid1:
        case layout_blocked_ubi_sce:
            render_vgmstream_blocked(buf, sample_count, vgmstream);
            break;
        case layout_segmented:
            render_vgmstream_segmented(buf, sample_count,vgmstream);
            break;
        case layout_layered:
            render_vgmstream_layered(buf, sample_count, vgmstream);
            break;
        default:
            break;
    }

    if (vgmstream->current_sample > vgmstream->num_samples) {
        int channels = vgmstream->channels;
        int32_t excess, decoded;

        excess = (vgmstream->current_sample - vgmstream->num_samples);
        if (excess > sample_count)
            excess = sample_count;
        decoded = sample_count - excess;

        memset(buf + decoded * channels, 0, excess * sizeof(sample_t) * channels);
        return sample_count;
    }

    return sample_count;
}


static void render_trim(VGMSTREAM* vgmstream) {
    sample_t* tmpbuf = vgmstream->tmpbuf;
    size_t tmpbuf_size = vgmstream->tmpbuf_size;
    int32_t buf_samples = tmpbuf_size / vgmstream->channels; /* base channels, no need to apply mixing */

    while (vgmstream->pstate.trim_begin_left) {
        int to_do = vgmstream->pstate.trim_begin_left;
        if (to_do > buf_samples)
            to_do = buf_samples;

        render_layout(tmpbuf, to_do, vgmstream);
        /* no mixing */
        vgmstream->pstate.trim_begin_left -= to_do;
    }
}

static int render_pad_begin(VGMSTREAM* vgmstream, sample_t* buf, int samples_to_do) {
    int channels = vgmstream->pstate.output_channels;
    int to_do = vgmstream->pstate.pad_begin_left;
    if (to_do > samples_to_do)
        to_do = samples_to_do;

    memset(buf, 0, to_do * sizeof(sample_t) * channels);
    vgmstream->pstate.pad_begin_left -= to_do;

    return to_do;
}

static int render_fade(VGMSTREAM* vgmstream, sample_t* buf, int samples_left) {
    play_state_t* ps = &vgmstream->pstate;
    //play_config_t* pc = &vgmstream->config;

    //if (!ps->fade_left || pc->play_forever)
    //    return;
    //if (ps->play_position + samples_done < ps->fade_start)
    //    return;

    {
        int s, ch,  start, fade_pos;
        int channels = ps->output_channels;
        int32_t to_do = ps->fade_left;

        if (ps->play_position < ps->fade_start) {
            start = samples_left - (ps->play_position + samples_left - ps->fade_start);
            fade_pos = 0;
        }
        else {
            start = 0;
            fade_pos = ps->play_position - ps->fade_start;
        }

        if (to_do > samples_left - start)
            to_do = samples_left - start;

        //TODO: use delta fadedness to improve performance?
        for (s = start; s < start + to_do; s++, fade_pos++) {
            double fadedness = (double)(ps->fade_duration - fade_pos) / ps->fade_duration;
            for (ch = 0; ch < channels; ch++) {
                buf[s*channels + ch] = (sample_t)buf[s*channels + ch] * fadedness;
            }
        }

        ps->fade_left -= to_do;

        /* next samples after fade end would be pad end/silence, so we can just memset */
        memset(buf + (start + to_do) * channels, 0, (samples_left - to_do - start) * sizeof(sample_t) * channels);
        return start + to_do;
    }
}

static int render_pad_end(VGMSTREAM* vgmstream, sample_t* buf, int samples_left) {
    play_state_t* ps = &vgmstream->pstate;
    int channels = vgmstream->pstate.output_channels;
    int skip = 0;
    int32_t to_do;

    /* pad end works like fades, where part of buf samples and part padding (silent),
     * calc exact totals (beyond pad end normally is silence, except with segmented layout) */
    if (ps->play_position < ps->pad_end_start) {
        skip = ps->pad_end_start - ps->play_position;
        to_do = ps->pad_end_duration;
    }
    else {
        skip = 0;
        to_do = (ps->pad_end_start + ps->pad_end_duration) - ps->play_position;
    }

    if (to_do > samples_left - skip)
        to_do = samples_left - skip;

    memset(buf + (skip * channels), 0, to_do * sizeof(sample_t) * channels);
    return skip + to_do;
}


/* Decode data into sample buffer. Controls the "external" part of the decoding,
 * while layout/decode control the "internal" part. */
int render_vgmstream(sample_t* buf, int32_t sample_count, VGMSTREAM* vgmstream) {
    play_state_t* ps = &vgmstream->pstate;
    int samples_to_do = sample_count;
    int samples_done = 0;
    int done;
    sample_t* tmpbuf = buf;


    /* simple mode with no settings (just skip everything below) */
    if (!vgmstream->config_enabled) {
        render_layout(buf, samples_to_do, vgmstream);
        mix_vgmstream(buf, samples_to_do, vgmstream);
        return samples_to_do;
    }


    /* trim may go first since it doesn't need output nor changes totals */
    if (ps->trim_begin_left) {
        render_trim(vgmstream);
    }

    /* adds empty samples to buf */
    if (ps->pad_begin_left) {
        done = render_pad_begin(vgmstream, tmpbuf, samples_to_do);
        samples_done += done;
        samples_to_do -= done;
        tmpbuf += done * vgmstream->pstate.output_channels; /* as if mixed */
    }

    /* end padding (before to avoid decoding if possible, but must be inside pad region) */
    if (!vgmstream->config.play_forever
            && ps->play_position /*+ samples_to_do*/ >= ps->pad_end_start
            && samples_to_do) {
        done = render_pad_end(vgmstream, tmpbuf, samples_to_do);
        samples_done += done;
        samples_to_do -= done;
        tmpbuf += done * vgmstream->pstate.output_channels; /* as if mixed */
    }

    /* main decode */
    { //if (samples_to_do)  /* 0 ok, less likely */
        done = render_layout(tmpbuf, samples_to_do, vgmstream);

        mix_vgmstream(tmpbuf, done, vgmstream);

        samples_done += done;

        if (!vgmstream->config.play_forever) {
            /* simple fadeout */
            if (ps->fade_left && ps->play_position + done >= ps->fade_start) {
                render_fade(vgmstream, tmpbuf, done);
            }

            /* silence leftover buf samples (rarely used when no fade is set) */
            if (ps->play_position + done >= ps->pad_end_start) {
                render_pad_end(vgmstream, tmpbuf, done);
            }
        }

        tmpbuf += done * vgmstream->pstate.output_channels;
    }


    vgmstream->pstate.play_position += samples_done;

    /* signal end */
    if (!vgmstream->config.play_forever
            && ps->play_position > ps->play_duration) {
        int excess = ps->play_position - ps->play_duration;
        if (excess > sample_count)
            excess = sample_count;

        samples_done = (sample_count - excess);

        ps->play_position = ps->play_duration;
    }

    return samples_done;
}

/*****************************************************************************/

static void seek_force_loop(VGMSTREAM* vgmstream, int loop_count) {
    /* only called after hit loop */
    if (!vgmstream->hit_loop)
        return;

    /* pretend decoder reached loop end so state is set to loop start */
    vgmstream->loop_count = loop_count - 1; /* seeking to first loop musy become ++ > 0 */
    vgmstream->current_sample = vgmstream->loop_end_sample;
    vgmstream_do_loop(vgmstream);
}

static void seek_force_decode(VGMSTREAM* vgmstream, int samples) {
    sample_t* tmpbuf = vgmstream->tmpbuf;
    size_t tmpbuf_size = vgmstream->tmpbuf_size;
    int32_t buf_samples = tmpbuf_size / vgmstream->channels; /* base channels, no need to apply mixing */

    while (samples) {
        int to_do = samples;
        if (to_do > buf_samples)
            to_do = buf_samples;

        render_layout(tmpbuf, to_do, vgmstream);
        /* no mixing */
        samples -= to_do;
    }
}

void seek_vgmstream(VGMSTREAM* vgmstream, int32_t seek_sample) {
    play_state_t* ps = &vgmstream->pstate;
    int play_forever = vgmstream->config.play_forever;

    int32_t decode_samples = 0;
    int loop_count = -1;
    int is_looped = vgmstream->loop_flag || vgmstream->loop_target > 0; /* loop target disabled loop flag during decode */


    /* cleanup */
    if (seek_sample < 0)
        seek_sample = 0;
    /* play forever can seek past max */
    if (vgmstream->config_enabled && seek_sample > ps->play_duration && !play_forever)
        seek_sample = ps->play_duration;

#if 0 //todo move below, needs to clamp in decode part
    /* optimize as layouts can seek faster internally */
    if (vgmstream->layout_type == layout_segmented) {
        seek_layout_segmented(vgmstream, seek_sample);

        if (vgmstream->config_enabled) {
            vgmstream->pstate.play_position = seek_sample;
        }
        return;
    }
    else if (vgmstream->layout_type == layout_layered) {
        seek_layout_layered(vgmstream, seek_sample);

        if (vgmstream->config_enabled) {
            vgmstream->pstate.play_position = seek_sample;
        }
        return;
    }
#endif

    /* will decode and loop until seek sample, but slower */
    //todo apply same loop logic as below, or pretend we have play_forever + settings?
    if (!vgmstream->config_enabled) {
        //;VGM_LOG("SEEK: simple seek=%i, cur=%i\n", seek_sample, vgmstream->current_sample);
        if (seek_sample < vgmstream->current_sample) {
            decode_samples = seek_sample;
            reset_vgmstream(vgmstream);
        }
        else {
            decode_samples = seek_sample - vgmstream->current_sample;
        }

        seek_force_decode(vgmstream, decode_samples);
        return;
    }

    //todo could improve performance bit if hit_loop wasn't lost when calling reset
    //todo wrong seek with ignore fade, also for layered layers (pass count to force loop + layers)


    /* seeking to requested sample normally means decoding and discarding up to that point (from
     * the beginning, or current position), but can be optimized a bit to decode less with some tricks:
     * - seek may fall in part of the song that isn't actually decoding (due to config, like padding)
     * - if file loops there is no need to decode N full loops, seek can be set relative to loop region
     * - can decode to seek sample from current position or loop start depending on lowest
     *
     * some of the cases below could be simplified but the logic to get this going is kinda mind-bending
     *
     *  (ex. with file = 100, pad=5s, trim=3s, loop=20s..90s)
     *  |  pad-begin  |  body-begin | body-loop0 | body-loop1 | body-loop2 | fade |  pad-end + beyond)
     *  0             5s   (-3s)    25s          95s          165s         235s   245s       Ns
     */
    //;VGM_LOG("SEEK: seek sample=%i, is_looped=%i\n", seek_sample, is_looped);

    /* start/pad-begin: consume pad samples */
    if (seek_sample < ps->pad_begin_duration) {
        /* seek=3: pad=5-3=2 */
        decode_samples = 0;

        reset_vgmstream(vgmstream);
        ps->pad_begin_left = ps->pad_begin_duration - seek_sample;

        //;VGM_LOG("SEEK: pad start / dec=%i\n", decode_samples);
    }

    /* body: find position relative to decoder's current sample */
    else if (play_forever || seek_sample < ps->pad_begin_duration + ps->body_duration + ps->fade_duration) {
        /* seek=10 would be seekr=10-5+3=8 inside decoder */
        int32_t seek_relative = seek_sample - ps->pad_begin_duration + ps->trim_begin_duration;


        //;VGM_LOG("SEEK: body / seekr=%i, curr=%i\n", seek_relative, vgmstream->current_sample);

        /* seek can be in some part of the body, depending on looped/decoder's current/etc */
        if (!is_looped && seek_relative < vgmstream->current_sample) {
            /* seekr=50s, curr=95 > restart + decode=50s */
            decode_samples = seek_relative;
            reset_vgmstream(vgmstream);

            //;VGM_LOG("SEEK: non-loop reset / dec=%i\n", decode_samples);
        }
        else if (!is_looped && seek_relative < vgmstream->num_samples) {
            /* seekr=95s, curr=50 > decode=95-50=45s */
            decode_samples = seek_relative - vgmstream->current_sample;

            //;VGM_LOG("SEEK: non-loop forward / dec=%i\n", decode_samples);
        }
        else if (!is_looped) {
            /* seekr=120s (outside decode, can happen when body is set manually) */
            decode_samples = 0;
            vgmstream->current_sample = vgmstream->num_samples + 1;

            //;VGM_LOG("SEEK: non-loop silence / dec=%i\n", decode_samples);
        }
        else if (seek_relative < vgmstream->loop_start_sample) {
            /* seekr=6s > 6-5+3 > seek=4s inside decoder < 20s: decode 4s from start, or 1s if current was at 3s */

            if (seek_relative < vgmstream->current_sample) {
                /* seekr=9s, current=10s > decode=9s from start */
                decode_samples = seek_relative;
                reset_vgmstream(vgmstream);

                //;VGM_LOG("SEEK: loop start reset / dec=%i\n", decode_samples);
            }
            else {
                /* seekr=9s, current=8s > decode=1s from current */
                decode_samples = seek_relative - vgmstream->current_sample;

                //;VGM_LOG("SEEK: loop start forward / dec=%i\n", decode_samples);
            }
        }
        else {
            /* seek can be clamped between loop parts (relative to decoder's current_sample) to minimize decoding */
            int32_t loop_body, loop_seek, loop_curr;

            /* current must have reached loop start at some point */
            if (!vgmstream->hit_loop) {
                int32_t skip_samples;

                if (vgmstream->current_sample >= vgmstream->loop_start_sample) {
                    VGM_LOG("SEEK: bad current sample %i vs %i\n", vgmstream->current_sample, vgmstream->loop_start_sample);
                    reset_vgmstream(vgmstream);
                }

                skip_samples = (vgmstream->loop_start_sample - vgmstream->current_sample);
                //;VGM_LOG("SEEK: must loop / skip=%i, curr=%i\n", skip_samples, vgmstream->current_sample);

                seek_force_decode(vgmstream, skip_samples);
            }

            /* current must be in loop area (shouldn't happen?) */
            if (vgmstream->current_sample < vgmstream->loop_start_sample
                    || vgmstream->current_sample < vgmstream->loop_end_sample) {
                //;VGM_LOG("SEEK: current outside loop area / curr=%i, ls=%i, le=%i\n", vgmstream->current_sample, vgmstream->current_sample, vgmstream->loop_end_sample);
                seek_force_loop(vgmstream, 0);
            }


            loop_body = (vgmstream->loop_end_sample - vgmstream->loop_start_sample);
            loop_seek = seek_relative - vgmstream->loop_start_sample;
            loop_count = loop_seek / loop_body;
            loop_seek = loop_seek % loop_body;
            loop_curr = vgmstream->current_sample - vgmstream->loop_start_sample;

            /* when "ignore fade" is used and seek falls into non-fade part, this needs to seek right before it
               so when calling seek_force_loop detection kicks in, and non-fade then decodes normally */
            if (vgmstream->loop_target && vgmstream->loop_target == loop_count) {
                loop_seek = loop_body;
            }

            //;VGM_LOG("SEEK: in loop / seekl=%i, loops=%i, cur=%i, dec=%i\n", loop_seek, loop_count, loop_curr, decode_samples);
            if (loop_seek < loop_curr) {
                decode_samples += loop_seek;
                seek_force_loop(vgmstream, loop_count);

                //;VGM_LOG("SEEK: loop reset / dec=%i, loop=%i\n", decode_samples, loop_count);
            }
            else {
                decode_samples += (loop_seek - loop_curr);

                //;VGM_LOG("SEEK: loop forward / dec=%i, loop=%i\n", decode_samples, loop_count);
            }

            /* adjust fade if seek ends in fade region */
            if (!play_forever
                    && seek_sample >= ps->pad_begin_duration + ps->body_duration
                    && seek_sample < ps->pad_begin_duration + ps->body_duration + ps->fade_duration) {
                ps->fade_left = ps->pad_begin_duration + ps->body_duration + ps->fade_duration - seek_sample;
                //;VGM_LOG("SEEK: in fade / fade=%i, %i\n", ps->fade_left, ps->fade_duration);
            }
        }

        /* done at the end in case of reset */
        ps->pad_begin_left = 0;
        ps->trim_begin_left = 0;
    }

    /* pad end and beyond: ignored */
    else {
        decode_samples = 0;
        ps->pad_begin_left = 0;
        ps->trim_begin_left = 0;
        if (!is_looped)
            vgmstream->current_sample = vgmstream->num_samples + 1;

        //;VGM_LOG("SEEK: end silence / dec=%i\n", decode_samples);
        /* looping decoder state isn't changed (seek backwards could use current sample) */
    }


    seek_force_decode(vgmstream, decode_samples);

    vgmstream->pstate.play_position = seek_sample;
}
