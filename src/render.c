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

int render_layout(sample_t* buf, int32_t sample_count, VGMSTREAM* vgmstream) {

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
        case layout_blocked_tt_ad:
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
