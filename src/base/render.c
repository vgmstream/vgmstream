#include "../vgmstream.h"
#include "../layout/layout.h"
#include "render.h"
#include "decode.h"
#include "mixing.h"
#include "sbuf.h"


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

int render_layout(sample_t* buf, int32_t sample_count, VGMSTREAM* vgmstream) {
    if (sample_count == 0)
        return 0;

    /* current_sample goes between loop points (if looped) or up to max samples,
     * must detect beyond that decoders would encounter garbage data */

    // nothing to decode: return blank buf (not ">=" to allow layouts to loop in some cases when == happens)
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
        case layout_blocked_dec:
        case layout_blocked_vs_mh:
        case layout_blocked_mul:
        case layout_blocked_gsb:
        case layout_blocked_xvas:
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

    // decode past stream samples: blank rest of buf
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

/*****************************************************************************/

typedef struct {
    //sbuf_t sbuf;
    int16_t* tmpbuf;
    int samples_to_do;
    int samples_done;
} render_helper_t;


// consumes samples from decoder
static void play_op_trim(VGMSTREAM* vgmstream, render_helper_t* renderer) {
    play_state_t* ps = &vgmstream->pstate;
    if (!ps->trim_begin_left)
        return;
    if (!renderer->samples_to_do)
        return;

    // simpler using external buf?
    //sample_t* tmpbuf = vgmstream->tmpbuf;
    //size_t tmpbuf_size = vgmstream->tmpbuf_size;
    //int32_t buf_samples = tmpbuf_size / vgmstream->channels; /* base channels, no need to apply mixing */
    sample_t* tmpbuf = renderer->tmpbuf;
    int buf_samples = renderer->samples_to_do;

    while (ps->trim_begin_left) {
        int to_do = ps->trim_begin_left;
        if (to_do > buf_samples)
            to_do = buf_samples;

        render_layout(tmpbuf, to_do, vgmstream);
        /* no mixing */
        ps->trim_begin_left -= to_do;
    }
}

// adds empty samples to buf
static void play_op_pad_begin(VGMSTREAM* vgmstream, render_helper_t* renderer) {
    play_state_t* ps = &vgmstream->pstate;
    if (!ps->pad_begin_left)
        return;
    //if (ps->play_position > ps->play_begin_duration) //implicit
    //    return;

    int channels = ps->output_channels;
    int buf_samples = renderer->samples_to_do;

    int to_do = ps->pad_begin_left;
    if (to_do > buf_samples)
        to_do = buf_samples;

    memset(renderer->tmpbuf, 0, to_do * sizeof(sample_t) * channels);
    ps->pad_begin_left -= to_do;

    renderer->samples_done += to_do;
    renderer->samples_to_do -= to_do;
    renderer->tmpbuf += to_do * channels; /* as if mixed */
}

// fades (modifies volumes) part of buf
static void play_op_fade(VGMSTREAM* vgmstream, sample_t* buf, int samples_done) {
    play_config_t* pc = &vgmstream->config;
    play_state_t* ps = &vgmstream->pstate;

    if (pc->play_forever || !ps->fade_left)
        return;
    if (ps->play_position + samples_done < ps->fade_start)
        return;

    int start, fade_pos;
    int channels = ps->output_channels;
    int32_t to_do = ps->fade_left;

    if (ps->play_position < ps->fade_start) {
        start = samples_done - (ps->play_position + samples_done - ps->fade_start);
        fade_pos = 0;
    }
    else {
        start = 0;
        fade_pos = ps->play_position - ps->fade_start;
    }

    if (to_do > samples_done - start)
        to_do = samples_done - start;

    //TODO: use delta fadedness to improve performance?
    for (int s = start; s < start + to_do; s++, fade_pos++) {
        double fadedness = (double)(ps->fade_duration - fade_pos) / ps->fade_duration;
        for (int ch = 0; ch < channels; ch++) {
            buf[s * channels + ch] = (sample_t)(buf[s*channels + ch] * fadedness);
        }
    }

    ps->fade_left -= to_do;

    /* next samples after fade end would be pad end/silence, so we can just memset */
    memset(buf + (start + to_do) * channels, 0, (samples_done - to_do - start) * sizeof(sample_t) * channels);
}

// adds null samples after decode
// pad-end works like fades, where part of buf is samples and part is padding (blank)
// (beyond pad end normally is silence, except with segmented layout)
static int play_op_pad_end(VGMSTREAM* vgmstream, sample_t* buf, int samples_done) {
    play_config_t* pc = &vgmstream->config;
    play_state_t* ps = &vgmstream->pstate;

    if (pc->play_forever)
        return 0;
    if (samples_done == 0)
        return 0;
    if (ps->play_position + samples_done < ps->pad_end_start)
        return 0;

    int channels = vgmstream->pstate.output_channels;
    int skip = 0;
    int32_t to_do;

    if (ps->play_position < ps->pad_end_start) {
        skip = ps->pad_end_start - ps->play_position;
        to_do = ps->pad_end_duration;
    }
    else {
        skip = 0;
        to_do = (ps->pad_end_start + ps->pad_end_duration) - ps->play_position;
    }

    if (to_do > samples_done - skip)
        to_do = samples_done - skip;

    memset(buf + (skip * channels), 0, to_do * sizeof(sample_t) * channels);
    return skip + to_do;
}

// clamp final play_position + done samples. Probably doesn't matter, but just in case.
static void play_adjust_totals(VGMSTREAM* vgmstream, render_helper_t* renderer, int sample_count) {
    play_state_t* ps = &vgmstream->pstate;

    ps->play_position += renderer->samples_done;

    /* usually only happens when mixing layers of different lengths (where decoder keeps calling render) */
    if (!vgmstream->config.play_forever && ps->play_position > ps->play_duration) {
        int excess = ps->play_position - ps->play_duration;
        if (excess > sample_count)
            excess = sample_count;

        renderer->samples_done = (sample_count - excess);

        ps->play_position = ps->play_duration;
    }
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
 * buf may fall anywhere in the middle of all that. Since some ops add "fake" (non-decoded)
 * samples to buf, we need to 
 */

int render_vgmstream(sample_t* buf, int32_t sample_count, VGMSTREAM* vgmstream) {
    render_helper_t renderer = {0};
    renderer.tmpbuf = buf;
    renderer.samples_done = 0;
    renderer.samples_to_do = sample_count;

    //sbuf_init16(&renderer.sbuf, buf, sample_count, vgmstream->channels);


    /* simple mode with no settings (just skip everything below) */
    if (!vgmstream->config_enabled) {
        render_layout(buf, renderer.samples_to_do, vgmstream);
        mix_vgmstream(buf, renderer.samples_to_do, vgmstream);
        return renderer.samples_to_do;
    }


    /* adds empty samples to buf and moves it */
    play_op_pad_begin(vgmstream, &renderer);

    /* trim decoder output (may go anywhere before main render since it doesn't use render output) */
    play_op_trim(vgmstream, &renderer);


    /* main decode */
    int done = render_layout(renderer.tmpbuf, renderer.samples_to_do, vgmstream);
    mix_vgmstream(renderer.tmpbuf, done, vgmstream);


    /* simple fadeout over decoded data (after mixing since usually results in less samples) */
    play_op_fade(vgmstream, renderer.tmpbuf, done);

    /* silence leftover buf samples (after fade as rarely may mix decoded buf + trim samples when no fade is set) 
     * (could be done before render to "consume" buf but doesn't matter much) */
    play_op_pad_end(vgmstream, renderer.tmpbuf, done);

    renderer.samples_done += done;
    //renderer.samples_to_do -= done; //not useful at this point
    //renderer.tmpbuf += done * vgmstream->pstate.output_channels;


    play_adjust_totals(vgmstream, &renderer, sample_count);
    return renderer.samples_done;
}
