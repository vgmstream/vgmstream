#include "../vgmstream.h"
#include "../layout/layout.h"
#include "render.h"
#include "decode.h"
#include "mixing.h"
#include "codec_info.h"



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

rc_t render_layout(sbuf_t* sbuf, VGMSTREAM* vgmstream) {

    int sample_count = sbuf->samples - sbuf->filled;
    if (sample_count == 0) {
        // may happen with padding added/etc
        return RC_RENDER_OK;
    }

    /* current_sample goes between loop points (if looped) or up to max samples,
     * must detect beyond that decoders would encounter garbage data */

    // Nothing to decode: return blank buf (not ">=" to allow layouts to loop in some cases when == happens)
    // Only blank samples to detect near EOF.
    if (vgmstream->current_sample > vgmstream->num_samples) {
        VGM_LOG("RENDER: no more samples in vgmstream, current=%i, max=%i\n", vgmstream->current_sample, vgmstream->num_samples);
        sbuf_silence_rest(sbuf);
        return RC_RENDER_OK;
    }

    rc_t rc;
    switch (vgmstream->layout_type) {
        case layout_none:
            rc = render_layout_flat(sbuf, vgmstream);
            break;
        case layout_interleave:
            rc = render_layout_interleave(sbuf, vgmstream);
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
        case layout_blocked_hvqm4:
        case layout_blocked_xa_aiff:
        case layout_blocked_vs_square:
        case layout_blocked_vid1:
        case layout_blocked_ubi_sce:
        case layout_blocked_tt_ad:
        case layout_blocked_vas:
        case layout_blocked_cf_df:
        case layout_blocked_cf_df_v5:
            rc = render_layout_blocked(sbuf, vgmstream);
            break;
        case layout_segmented:
            rc = render_layout_segmented(sbuf, vgmstream);
            break;
        case layout_layered:
            rc = render_layout_layered(sbuf, vgmstream);
            break;
        default:
            rc = RC_LAYOUT_ERROR;
            break;
    }

    if (rc < 0) {
        VGM_LOG("RENDER: layout error\n"); 
        sbuf_silence_rest(sbuf);

         // ignore to allow looping in edge cases, and emmit silent samples near EOF
        return RC_RENDER_OK;
    }


    // decode past stream samples: blank rest of buf
    if (vgmstream->current_sample > vgmstream->num_samples) {
        int32_t excess, decoded;

        excess = (vgmstream->current_sample - vgmstream->num_samples);
        if (excess > sample_count)
            excess = sample_count;
        decoded = sample_count - excess;

        sbuf_silence_part(sbuf, decoded, sample_count - decoded);
    }

    return RC_RENDER_OK;
}

/*****************************************************************************/

// consumes samples from decoder
static void play_op_trim(VGMSTREAM* vgmstream, sbuf_t* sbuf) {
    if (!vgmstream->config_enabled)
        return;

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
        int rc = render_layout(&sbuf_tmp, vgmstream);
        if (rc < 0 || rc == RC_RENDER_EOR)
            break;

        /* no mixing */
        ps->trim_begin_left -= sbuf_tmp.filled;
    }
}

// adds empty samples to buf
static void play_op_pad_begin(VGMSTREAM* vgmstream, sbuf_t* sbuf) {
    if (!vgmstream->config_enabled)
        return;

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
    if (!vgmstream->config_enabled)
        return;

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
    if (!vgmstream->config_enabled)
        return;

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
    if (!vgmstream->config_enabled)
        return;

    play_state_t* ps = &vgmstream->pstate;

    ps->play_position += sbuf->filled;

    if (vgmstream->config.play_forever)
        return;
    if (ps->play_position <= ps->play_duration)
        return;

    // usually only happens when mixing layers of different lengths (where decoder keeps calling render)
    int excess = ps->play_position - ps->play_duration;
    if (excess > sbuf->samples)
        excess = sbuf->samples;
    sbuf->filled = (sbuf->samples - excess);

    /* clamp */
    ps->play_position = ps->play_duration;
}

static rc_t get_result_code(VGMSTREAM* vgmstream, sbuf_t* sbuf) {
    if (!vgmstream->config_enabled) {

        //TODO: maybe clamp samples

        if (vgmstream->current_sample > vgmstream->num_samples) {
            return RC_RENDER_EOR;
        }

        return RC_RENDER_OK;
    }
    else {

        play_state_t* ps = &vgmstream->pstate;

        if (vgmstream->config.play_forever)
            return RC_RENDER_OK;

        if (ps->play_position < ps->play_duration)
            return RC_RENDER_OK;

        // no more samples to play
        return RC_RENDER_EOR;
    }
}

#if 0
/* supply sbuf if caller didn't (may still have set max samples) */
static rc_t setup_buf(sbuf_t* sbuf, VGMSTREAM* vgmstream) {

    if (sbuf->fmt != SFMT_NONE)
        return RC_RENDER_OK;

    int buf_samples = vgmstream->tmpbuf_size / vgmstream->channels / sizeof(float);
    int max_samples = sbuf->samples;
    if (buf_samples > max_samples && max_samples > 0)
        buf_samples = max_samples;

    if (buf_samples == 0) {
        VGM_LOG("RENDER: no samples to render\n");
        return RC_RENDER_ERROR;
    }

    //TODO: tmpbuf is also used when seeking; setup on demand (both render and seek may be needed for layered)
    sfmt_t sfmt = mixing_get_input_sample_type(vgmstream);
    sbuf_init(sbuf, sfmt, vgmstream->tmpbuf, buf_samples, vgmstream->channels);

    return RC_RENDER_OK;
}
#endif

/*****************************************************************************/

/* Decode data into sbuf, which may be updated at various points during render.
 * Controls the "external" part of the decoding, while layout/decode control the "internal" part.
 *
 * A stream would be "externally" rendered like this:
 *      [ pad-begin ]( trim )[    decoded data * N loops   ][ pad-end ]
 *                                              \ end-fade |
 *
 * Which section depends on play_position. vgmstream render's buf may fall anywhere in the middle
 * of all that, so it's handles in steps. Some ops add "fake" (non-decoded) samples to buf.
 */

rc_t render_main(sbuf_t* sbuf, VGMSTREAM* vgmstream) {
#if 0
    rc_t buf_rc = setup_buf(sbuf, vgmstream);
    if (buf_rc < 0)
        return buf_rc;
#endif

    // trim decoder output (may go anywhere before main render since it doesn't use render output, but easier first)
    play_op_trim(vgmstream, sbuf);

    // adds empty samples to buf and moves it
    play_op_pad_begin(vgmstream, sbuf);


    /* main decode */
    /*rc_t rc =*/ render_layout(sbuf, vgmstream);

    /* apply mixing ops (may change output totals) */
    mix_vgmstream(sbuf, vgmstream);


    // simple fadeout over decoded data (after mixing since usually results in less samples)
    play_op_fade(vgmstream, sbuf);

    // silence leftover buf samples (after fade, as rarely may mix decoded buf + trim samples when no fade is set)
    // (could be done before render to "consume" buf but doesn't matter much)
    play_op_pad_end(vgmstream, sbuf);

    // change play position and max buffer
    play_adjust_totals(vgmstream, sbuf);

    rc_t rc = get_result_code(vgmstream, sbuf);

    // should be part of main mixer process, but txtp calcs and render ops assume original sample rate,
    // making it a bit hard to plug in resampling without potential buggy changes around
    resample_vgmstream(sbuf, vgmstream, rc == RC_RENDER_EOR);

    return rc;
}
