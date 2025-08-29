#include "../vgmstream.h"
#include "../layout/layout.h"
#include "render.h"
#include "decode.h"
#include "mixing.h"
#include "codec_info.h"


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

/*****************************************************************************/

void render_free(VGMSTREAM* vgmstream) {
    if (!vgmstream->layout_data)
        return;

    if (vgmstream->layout_type == layout_segmented) {
        free_layout_segmented(vgmstream->layout_data);
    }

    if (vgmstream->layout_type == layout_layered) {
        free_layout_layered(vgmstream->layout_data);
    }
}

void render_reset(VGMSTREAM* vgmstream) {

    if (vgmstream->layout_type == layout_segmented) {
        reset_layout_segmented(vgmstream->layout_data);
    }

    if (vgmstream->layout_type == layout_layered) {
        reset_layout_layered(vgmstream->layout_data);
    }
}

int render_layout(sbuf_t* sbuf, VGMSTREAM* vgmstream) {
    int sample_count = sbuf->samples;

    if (sample_count == 0)
        return 0;

    /* current_sample goes between loop points (if looped) or up to max samples,
     * must detect beyond that decoders would encounter garbage data */

    // nothing to decode: return blank buf (not ">=" to allow layouts to loop in some cases when == happens)
    if (vgmstream->current_sample > vgmstream->num_samples) {
        sbuf_silence_rest(sbuf);
        return sample_count;
    }

    switch (vgmstream->layout_type) {
        case layout_interleave:
            render_vgmstream_interleave(sbuf, vgmstream);
            break;
        case layout_none:
            render_vgmstream_flat(sbuf, vgmstream);
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
        case layout_blocked_dec:
        case layout_blocked_vs_mh:
        case layout_blocked_mul:
        case layout_blocked_gsnd:
        case layout_blocked_vas_kceo:
        case layout_blocked_thp:
        case layout_blocked_filp:
        case layout_blocked_rage_aud:
        case layout_blocked_ea_swvr:
        case layout_blocked_adm:
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
        case layout_blocked_vas:
            render_vgmstream_blocked(sbuf, vgmstream);
            break;
        case layout_segmented:
            render_vgmstream_segmented(sbuf, vgmstream);
            break;
        case layout_layered:
            render_vgmstream_layered(sbuf, vgmstream);
            break;
        default:
            break;
    }

    // decode past stream samples: blank rest of buf
    if (vgmstream->current_sample > vgmstream->num_samples) {
        int32_t excess, decoded;

        excess = (vgmstream->current_sample - vgmstream->num_samples);
        if (excess > sample_count)
            excess = sample_count;
        decoded = sample_count - excess;

        sbuf_silence_part(sbuf, decoded, sample_count - decoded);

        return sample_count;
    }

    return sample_count;
}

/*****************************************************************************/

// consumes samples from decoder
static void play_op_trim(VGMSTREAM* vgmstream, sbuf_t* sbuf) {
    play_state_t* ps = &vgmstream->pstate;
    if (!ps->trim_begin_left)
        return;
    if (sbuf->samples <= 0)
        return;

#if 0
    // TODO: issues trim with loops
    // use fast seek if possible
    const codec_info_t* codec_info = codec_get_info(vgmstream);
    if (/*!vgmstream->loop_flag &&*/ codec_info && codec_info->seekable(vgmstream)) {
        // reset needed? should only trim after reset
        decode_seek(vgmstream, ps->trim_begin_left);
        vgmstream->current_sample += ps->trim_begin_left;
        ps->trim_begin_left = 0;
        ;VGM_LOG("RENDER [trim]: codec seek %i\n", vgmstream->current_sample);
        return;
    }
#endif

    sbuf_t sbuf_tmp = *sbuf;
    int buf_samples = sbuf->samples;

    while (ps->trim_begin_left) {
        int to_do = ps->trim_begin_left;
        if (to_do > buf_samples)
            to_do = buf_samples;

        sbuf_tmp.filled = 0;
        sbuf_tmp.samples = to_do;
        int done = render_layout(&sbuf_tmp, vgmstream);
        /* no mixing */

        ps->trim_begin_left -= done;
    }
}

// adds empty samples to buf
static void play_op_pad_begin(VGMSTREAM* vgmstream, sbuf_t* sbuf) {
    play_state_t* ps = &vgmstream->pstate;
    if (!ps->pad_begin_left)
        return;
    //if (ps->play_position > ps->play_begin_duration) //implicit
    //    return;

    int to_do = ps->pad_begin_left;
    if (to_do > sbuf->samples)
        to_do = sbuf->samples;

    sbuf_silence_part(sbuf, 0, to_do);

    ps->pad_begin_left -= to_do;
    sbuf->filled += to_do;
}

// fades (modifies volumes) part of buf
static void play_op_fade(VGMSTREAM* vgmstream, sbuf_t* sbuf) {
    play_config_t* pc = &vgmstream->config;
    play_state_t* ps = &vgmstream->pstate;

    if (pc->play_forever || !ps->fade_left)
        return;
    if (ps->play_position + sbuf->filled < ps->fade_start)
        return;

    int start, fade_pos;
    int32_t to_do = ps->fade_left;

    if (ps->play_position < ps->fade_start) {
        start = sbuf->filled - (ps->play_position + sbuf->filled - ps->fade_start);
        fade_pos = 0;
    }
    else {
        start = 0;
        fade_pos = ps->play_position - ps->fade_start;
    }

    if (to_do > sbuf->filled - start)
        to_do = sbuf->filled - start;

    sbuf_fadeout(sbuf, start, to_do, fade_pos, ps->fade_duration);

    ps->fade_left -= to_do;
}

// adds null samples after decode
// pad-end works like fades, where part of buf is samples and part is padding (blank)
// (beyond pad end normally is silence, except with segmented layout)
static void play_op_pad_end(VGMSTREAM* vgmstream, sbuf_t* sbuf) {
    play_config_t* pc = &vgmstream->config;
    play_state_t* ps = &vgmstream->pstate;

    if (pc->play_forever)
        return;
    if (ps->play_position + sbuf->filled < ps->pad_end_start)
        return;

    int start;
    int to_do;
    if (ps->play_position < ps->pad_end_start) {
        start = ps->pad_end_start - ps->play_position;
        to_do = ps->pad_end_duration;
    }
    else {
        start = 0;
        to_do = (ps->pad_end_start + ps->pad_end_duration) - ps->play_position;
    }

    if (to_do > sbuf->filled - start)
        to_do = sbuf->filled - start;

    sbuf_silence_part(sbuf, start, to_do);

    //TO-DO: this never adds samples and just silences them, since decoder always returns something
    //sbuf->filled += ?
}

// clamp final play_position + done samples. Probably doesn't matter (maybe for plugins checking length), but just in case.
static void play_adjust_totals(VGMSTREAM* vgmstream, sbuf_t* sbuf) {
    play_state_t* ps = &vgmstream->pstate;

    ps->play_position += sbuf->filled;

    if (vgmstream->config.play_forever)
        return;
    if (ps->play_position <= ps->play_duration)
        return;

    /* usually only happens when mixing layers of different lengths (where decoder keeps calling render) */
    int excess = ps->play_position - ps->play_duration;
    if (excess > sbuf->samples)
        excess = sbuf->samples;
    sbuf->filled = (sbuf->samples - excess);

    /* clamp */
    ps->play_position = ps->play_duration;
}

/*****************************************************************************/

/* Decode data into sample buffer. Controls the "external" part of the decoding,
 * while layout/decode control the "internal" part.
 *
 * A stream would be "externally" rendered like this:
 *      [ pad-begin ]( trim )[    decoded data * N loops   ][ pad-end ]
 *                                              \ end-fade |
 * 
 * Which part we are in depends on play_position. Because vgmstream render's 
 * buf may fall anywhere in the middle of all that. Some ops add "fake" (non-decoded)
 * samples to buf.
 */

int render_main(sbuf_t* sbuf, VGMSTREAM* vgmstream) {

    /* simple mode with no play settings (just skip everything below) */
    if (!vgmstream->config_enabled) {
        render_layout(sbuf, vgmstream);
        sbuf->filled = sbuf->samples;

        mix_vgmstream(sbuf, vgmstream);

        return sbuf->filled;
    }


    /* trim decoder output (may go anywhere before main render since it doesn't use render output, but easier first) */
    play_op_trim(vgmstream, sbuf);

    /* adds empty samples to buf and moves it */
    play_op_pad_begin(vgmstream, sbuf);


    /* main decode (use temp buf to "consume") */
    sbuf_t sbuf_tmp = *sbuf;
    sbuf_consume(&sbuf_tmp, sbuf_tmp.filled);
    int done = render_layout(&sbuf_tmp, vgmstream);
    sbuf->filled += done;

    mix_vgmstream(sbuf, vgmstream);

    /* simple fadeout over decoded data (after mixing since usually results in less samples) */
    play_op_fade(vgmstream, sbuf);


    /* silence leftover buf samples (after fade as rarely may mix decoded buf + trim samples when no fade is set) 
     * (could be done before render to "consume" buf but doesn't matter much) */
    play_op_pad_end(vgmstream, sbuf);


    play_adjust_totals(vgmstream, sbuf);
    return sbuf->filled;
}

int render_vgmstream2(sample_t* buf, int32_t sample_count, VGMSTREAM* vgmstream) {
    sbuf_t sbuf = {0};
    sbuf_init_s16(&sbuf, buf, sample_count, vgmstream->channels);

    return render_main(&sbuf, vgmstream);
}
