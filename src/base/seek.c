#include "../vgmstream.h"
#include "../layout/layout.h"
#include "render.h"
#include "decode.h"
#include "mixing.h"
#include "plugins.h"
#include "sbuf.h"
#include "codec_info.h"


/* Seeking in vgmstream can be divided into:
 * - 'render' part (padding, trims, etc)
 * - 'layout' part (segments, layers, blocks, etc)
 * - 'decode' part (codec + loops)
 * 
 * In vgmstream, seeking to a requested sample mostly means decoding and discarding up to that point
 * (from the beginning, or current position), since it's simple and fast enough for most codecs.
 * 
 * This can be optimized a bit with some tricks, like detecting if seeking falls into render/layout/decode parts,
 * or clamping seek sample into a loop region.
 *
 *  render:  |  pad-begin  | (trim)                                 |  fade  |  pad-end + beyond  |
 *  layout:                |  segment1  |  segment2                 |
 *  decode:                |    body    |  body-begin |  body-loop  |
 */

static void seek_force_render(VGMSTREAM* vgmstream, int samples) {
    if (!samples)
        return;
    //;VGM_LOG("SEEK: force render %i\n", samples);

#if 0
    // TODO: issues trim with loops enabled
    // TODO: detect 'render' vs 'decode'
    // use fast seek if possible
    const codec_info_t* codec_info = codec_get_info(vgmstream);
    if (/*!vgmstream->loop_flag &&*/ codec_info && codec_info->seekable(vgmstream)) {
        // reset needed?
        decode_seek(vgmstream, vgmstream->current_sample + samples);
        vgmstream->current_sample = samples;
        ;VGM_LOG("RENDER [trim]: codec seek\n");
        return;
    }
#endif

    void* tmpbuf = vgmstream->tmpbuf;
    int buf_samples = vgmstream->tmpbuf_size / vgmstream->channels / sizeof(float); /* base decoder channels, no need to apply mixing */

    sbuf_t sbuf_tmp;
    sbuf_init(&sbuf_tmp, mixing_get_input_sample_type(vgmstream), tmpbuf, buf_samples, vgmstream->channels);

    while (samples) {
        int to_do = samples;
        if (to_do > buf_samples)
            to_do = buf_samples;
        sbuf_tmp.samples = to_do;
        render_layout(&sbuf_tmp, vgmstream);

        /* no mixing */
        samples -= to_do;

        sbuf_tmp.filled = 0; // discard buf
    }
}

/* ************************************************************************* */

/* pretend decoder reached loop end so internal state is set like jumping to loop start 
 * (no effect in some layouts but that is ok)
 */
static void seek_decode_force_loop_end(VGMSTREAM* vgmstream, int loop_count) {
    /* only called after hit loop */
    if (!vgmstream->hit_loop)
        return;

    vgmstream->loop_count = loop_count - 1; /* seeking to first loop must become ++ > 0 */
    vgmstream->current_sample = vgmstream->loop_end_sample;
    decode_do_loop(vgmstream);
}

/* Seeks in the 'decode' part
 * Sample has been adjusted to be relative within the decode part.
 * May fall into loop-intro / body xN loops / loop-end (or body-end).
 * //TODO: seek into body_end
 */
static void seek_decode(VGMSTREAM* vgmstream, int32_t seek_sample) {
    //;VGM_LOG("SEEK [decode]: current=%i (target=%i)\n", vgmstream->current_sample, seek_sample);

    bool is_looped = vgmstream->loop_flag || vgmstream->loop_target > 0; // loop target may disable loop flag during decode

    // ignore seek to current (not likely but...)
    if (seek_sample == vgmstream->current_sample) {
        //;VGM_LOG("SEEK [decode]: ignore seek to current\n");
        return;
    }


    // non-looped seek before decoder's position: restart + consume
    if (!is_looped && seek_sample < vgmstream->current_sample) {
        // ex. seek=50s, curr=95s: restart + decode=50s
        reset_vgmstream(vgmstream);

        seek_force_render(vgmstream, seek_sample);
        //;VGM_LOG("SEEK [decode]: non-loop reset / dec=%i\n", seek_sample);
        return;
    }

    // non-looped seek after decoder's position: consume
    if (!is_looped && seek_sample < vgmstream->num_samples) {
        // ex. seek=95s, curr=50: decode=95-50=45s
        int32_t decode_samples = seek_sample - vgmstream->current_sample;

        seek_force_render(vgmstream, decode_samples);
        //;VGM_LOG("SEEK [decode]: non-loop forward / dec=%i\n", decode_samples);
        return;
    }

    // non-looped seeks after after num_samples, can happen when body is set manually
    if (!is_looped) {
        //TODO test if needed and unify with the above if not
        vgmstream->current_sample = vgmstream->num_samples + 1;

        //;VGM_LOG("SEEK [decode]: non-loop after decode\n");
        return;
    }


    // looped seek before decoder's position: restart + consume or just consume
    if (is_looped && seek_sample < vgmstream->loop_start_sample) {
        int32_t decode_samples = 0;

        if (seek_sample < vgmstream->current_sample) {
            // seek=9s, current=10s: decode 9s from start
            reset_vgmstream(vgmstream);
            decode_samples = seek_sample;

            //;VGM_LOG("SEEK [decode]: loop start reset / dec=%i\n", decode_samples);
        }
        else {
            // seek=9s, current=8s: decode 1s from current
            decode_samples = seek_sample - vgmstream->current_sample;

            //;VGM_LOG("SEEK [decode]: loop start forward / dec=%i\n", decode_samples);
        }

        seek_force_render(vgmstream, decode_samples);
        return;
    }

    // looped seek after loop start: can be clamped between loop body (relative to decoder's current_sample) to minimize decoding
    // ex. file intro=0..10s, body=10s...100s, outro=..110s
    //  - seek=50s: seek 10s over intro, seek 40s into body
    //  - seek=135s: seek 10s over intro, seek 35s into body (seek 90s into loop1 + 35s into loop 2)
    if (is_looped) {

        int32_t loop_body = (vgmstream->loop_end_sample - vgmstream->loop_start_sample); // samples of 1 looped region
        int32_t loop_seek = (seek_sample - vgmstream->loop_start_sample); // samples after loop start
        int     loop_count = loop_seek / loop_body; // (not accurate when loop_target is set)
        loop_seek = loop_seek % loop_body; // clamp within single loop after calcs


        // current must have reached loop start at some point, otherwise force it (NOTE: some layouts don't actually set hit_loop)
        if (!vgmstream->hit_loop) {
            if (vgmstream->current_sample > vgmstream->loop_start_sample) {
                VGM_LOG("SEEK [decode]: bad current sample %i vs %i\n", vgmstream->current_sample, vgmstream->loop_start_sample);
                reset_vgmstream(vgmstream);
            }

            int32_t skip_samples = (vgmstream->loop_start_sample - vgmstream->current_sample);
            //;VGM_LOG("SEEK [decode]: force loop region / skip=%i, curr=%i\n", skip_samples, vgmstream->current_sample);

            seek_force_render(vgmstream, skip_samples);
        }

        // current must hit loop end (may happen at start since it's smaller than loop_end)
        if (vgmstream->current_sample < vgmstream->loop_end_sample) {
            //;VGM_LOG("SEEK [decode]: outside loop region / curr=%i, ls=%i, le=%i\n", vgmstream->current_sample, vgmstream->current_sample, vgmstream->loop_end_sample);
            seek_decode_force_loop_end(vgmstream, 0);
        }

        //;VGM_LOG("SEEK: in loop region / seekr=%i, seekl=%i, loops=%i, dec_curr=%i\n", seek_sample, loop_seek, loop_count, loop_curr);

        // when "ignore fade" is set and seek falls into the outro part (loop count is bigger than expected), adjust seek
        // to do a whole part + outro samples (should probably calculate correct loop_count before but...)
        if (vgmstream->loop_target && loop_count >= vgmstream->loop_target) {
            loop_seek = loop_body + (seek_sample - vgmstream->loop_start_sample) - vgmstream->loop_target * loop_body;
            loop_count = vgmstream->loop_target - 1;  // so seek_decode_force_loop_end detection kicks in and adds +1

            //;VGM_LOG("SEEK [decode]: outro outside / seek=%i, count=%i\n", loop_seek, loop_count);
        }

        // seek is clamped to loop region, decode from loop start or from current position
        int32_t decode_samples = 0;
        int32_t loop_current = vgmstream->current_sample - vgmstream->loop_start_sample;
        if (loop_seek < loop_current) {
            decode_samples = loop_seek;
            seek_decode_force_loop_end(vgmstream, loop_count);

            //;VGM_LOG("SEEK[decode]: loop reset / dec=%i, loop=%i\n", decode_samples, loop_count);
        }
        else {
            decode_samples = (loop_seek - loop_current);
            vgmstream->loop_count = loop_count;

            //;VGM_LOG("SEEK [decode]: loop forward / dec=%i, loop=%i\n", decode_samples, loop_count);
        }

        seek_force_render(vgmstream, decode_samples);
        //;VGM_LOG("SEEK [decode]: current=%i\n", vgmstream->current_sample);
        return;
    }
}

/* ************************************************************************* */

typedef void (*seek_layout_fn_t)(VGMSTREAM* vgmstream, int32_t seek_sample);

//TODO: avoid resetting?
//TODO test unify with the above
static void seek_layout_custom(VGMSTREAM* vgmstream, seek_layout_fn_t seek_layout_fn, int32_t seek_sample) {

    bool is_looped = vgmstream->loop_flag || vgmstream->loop_target > 0; // loop target may disable loop flag during decode

    if (!is_looped && seek_sample >= vgmstream->num_samples) {
        vgmstream->current_sample = vgmstream->num_samples + 1;
        return;
    }

    if (!is_looped) {
        reset_vgmstream(vgmstream);
        seek_layout_fn(vgmstream, seek_sample);
        return;
    }

    if (is_looped && seek_sample < vgmstream->loop_start_sample) {
        reset_vgmstream(vgmstream);
        seek_layout_fn(vgmstream, seek_sample);
        return;
    }

    if (is_looped) {
        // clamp seek to playable region
        int32_t loop_body = (vgmstream->loop_end_sample - vgmstream->loop_start_sample); // samples of 1 looped region
        int32_t loop_seek = (seek_sample - vgmstream->loop_start_sample); // samples after loop start
        int     loop_count = loop_seek / loop_body; // (not accurate when loop_target is set)
        loop_seek = loop_seek % loop_body; // clamp within single loop after calcs

        reset_vgmstream(vgmstream);

        // when "ignore fade" is set and seek falls into the outro part (loop count is bigger than expected), adjust seek
        // to do a whole part + outro samples (should probably calculate correct loop_count before but...)
        if (vgmstream->loop_target && loop_count >= vgmstream->loop_target) {
            loop_seek = loop_body + (seek_sample - vgmstream->loop_start_sample) - vgmstream->loop_target * loop_body;
            loop_count = vgmstream->loop_target - 1;  // so seek_decode_force_loop_end detection kicks in and adds +1

            //;VGM_LOG("SEEK [layout]: outro outside / seek=%i, count=%i\n", loop_seek, loop_count);

            //TODO: not working correctly (incorrectly set flags stuff), for now give up and use regular (slower) seek
            if (vgmstream->layout_type == layout_segmented) {
                seek_decode(vgmstream, seek_sample);
                return;
            }
        }

        seek_layout_fn(vgmstream, vgmstream->loop_start_sample);
        decode_do_loop(vgmstream); // ugly but needed to loop after seeking due to how layout works
        seek_layout_fn(vgmstream, vgmstream->loop_start_sample + loop_seek);
        vgmstream->loop_count = loop_count;
        return;
    }
}


/* Seeks in the 'layout' part
 */
static void seek_layout(VGMSTREAM* vgmstream, int32_t seek_sample) {
    //;VGM_LOG("SEEK [layout]: (target=%i)\n", seek_sample);

    // layouts can seek faster internally
    if (vgmstream->layout_type == layout_segmented) {
        seek_layout_custom(vgmstream, seek_layout_segmented, seek_sample);
        return;
    }

    if (vgmstream->layout_type == layout_layered) {
        seek_layout_custom(vgmstream, seek_layout_layered, seek_sample);
        return;
    }
    
    {
        // common layouts without implemented seeking
        seek_decode(vgmstream, seek_sample);
        return;
    }
}

/* ************************************************************************* */

// helper
typedef struct {
    bool play_forever;
    bool is_config;
    bool is_looped;
    bool is_body_end;

    int32_t play_sample;

    int32_t seek_target;
    int32_t seek_decode;
} seek_render_t;


/* trim-begin removes N samples from output = increases seek into decode
 * ex. seek=0s, trim=5s: seek decoder to 0+5=5s
 * ex. seek=3s, trim=5s: seek decoder to 3+5=8s
 * ex. seek=5s, trim=5s: seek decoder to 5+5=10s
 * (doesn't depend on 'reset')
 */
static void seek_render_trim_begin(VGMSTREAM* vgmstream, seek_render_t* sc) {
    if (!sc->is_config)
        return;

    play_state_t* ps = &vgmstream->pstate;
    if (ps->trim_begin_duration <= 0)
        return;

    // Must trim all samples in decoder, so add to final seek and decoder will reset as needed.
    // trim_begin_left state doesn't matter, as a seek to N into decoder can be treated as absolute.
    int32_t trim_samples = ps->trim_begin_duration;
    ps->trim_begin_left = 0; // all 'consumed' during decode
    sc->seek_decode += trim_samples;
    //;VGM_LOG("SEEK [render]: trim-begin s=%i (decode=%i, target=%i)\n", trim_samples, sc->seek_decode, sc->seek_target);
}

/* Pad-begin adds N samples to output = decreases seek (but also adjusts pad_left).
 * ex. seek=10s, pad=5s: seek decoder to 10-5s=0s (plus pad_left: 0s)
 * ex. seek=3s,  pad=5s: seek decoder to 3-5=0s (plus pad_left: 5-3=2s)
 * (doesn't depend on 'reset')
 */
static void seek_render_pad_begin(VGMSTREAM* vgmstream, seek_render_t* sc) {
    if (!sc->is_config)
        return;

    play_state_t* ps = &vgmstream->pstate;
    if (ps->pad_begin_duration <= 0)
        return;

    // Adjust pad_left so next 'decode' emmits pad samples properly.
    // Must also seek into decoder (if seek is less than duration, it will seek to 0 = reset)
    int32_t pad_samples = ps->pad_begin_duration;
    if (pad_samples > sc->seek_target)
        pad_samples = sc->seek_target;
    ps->pad_begin_left = (ps->pad_begin_duration - pad_samples);
    sc->seek_decode -= pad_samples;
    //;VGM_LOG("SEEK [render]: pad-begin s=%i left=%i (decode=%i, target=%i)\n", pad_samples, ps->pad_begin_left, sc->seek_decode, sc->seek_target);
}

/* pad-end and beyond: ignored
 */
static void seek_render_pad_end(VGMSTREAM* vgmstream, seek_render_t* sc) {
    if (!sc->is_config || sc->play_forever)
        return;

    play_state_t* ps = &vgmstream->pstate;
    if (ps->pad_end_duration <= 0)
        return;

    // must fall after body's samples
    if (sc->seek_target < ps->pad_begin_duration + ps->body_duration + ps->fade_duration)
        return;

    // looping decoder state isn't changed (seek backwards could use current sample)
    if (!sc->is_looped)
        vgmstream->current_sample = vgmstream->num_samples + 1; // TODO: needed???

    //ps->pad_end_left = ... // nothing to adjust due to how pad end works
    //;VGM_LOG("SEEK [render]: pad-end (decode=%i, target=%i)\n", sc->seek_decode, sc->seek_target);
}

/* adjust fade if seek ends in fade region
 */
static void seek_render_fade(VGMSTREAM* vgmstream, seek_render_t* sc) {
    if (!sc->is_config || sc->play_forever || sc->is_body_end)
        return;

    // seek must fall within fade
    play_state_t* ps = &vgmstream->pstate;
    if (sc->seek_target < ps->pad_begin_duration + ps->body_duration)
        return;
    if (sc->seek_target >= ps->pad_begin_duration + ps->body_duration + ps->fade_duration)
        return;

    ps->fade_left = ps->pad_begin_duration + ps->body_duration + ps->fade_duration - sc->seek_target;
    //;VGM_LOG("SEEK [render]: fade s=%i (decode=%i, target=%i)\n", ps->fade_left, sc->seek_decode, sc->seek_target);
}

/* Seeks in the 'render' part
 */
static void seek_render(VGMSTREAM* vgmstream, seek_render_t* sc) {

    // ignore seek to current (not likely but...)
    if (sc->seek_target == sc->play_sample) {
        //;VGM_LOG("SEEK [render]: ignore seek to current\n");
        return;
    }

    // render doesn't need to reset as state is handled in each function

    seek_render_trim_begin(vgmstream, sc);
    seek_render_pad_begin(vgmstream, sc);

    // save 'render' values modified above and possible undone by reset during 'decode'
    play_state_t ps_copy = vgmstream->pstate; //memcpy

    seek_layout(vgmstream, sc->seek_decode);

    // restore values undone by reset
    vgmstream->pstate = ps_copy; //memcpy

    seek_render_pad_end(vgmstream, sc);
    seek_render_fade(vgmstream, sc);

    if (sc->is_config) {
        vgmstream->pstate.play_position = sc->seek_target;
    }
}

/* ************************************************************************* */

static int32_t clamp_seek(VGMSTREAM* vgmstream, int32_t seek_sample) {

    /* cleanup */
    if (seek_sample < 0)
        return 0;

    play_state_t* ps = &vgmstream->pstate;
    bool is_config = vgmstream->config_enabled;

    /* play forever can seek past max */
    if (is_config && seek_sample > ps->play_duration && !vgmstream->config.play_forever)
        return ps->play_duration;

    return seek_sample;
}

//TODO apply same loop logic as below (should be ok now with public API)
//TODO: check that decode doesn't use config
/* decode and loop until seek sample (slow) */
static void seek_simple(VGMSTREAM* vgmstream, int32_t seek_sample) {
    //;VGM_LOG("SEEK [simple]: cur=%i (target=%i)\n", vgmstream->current_sample, seek_sample);

    int32_t decode_samples;
    if (seek_sample < vgmstream->current_sample) {
        decode_samples = seek_sample;
        reset_vgmstream(vgmstream);
    }
    else {
        decode_samples = seek_sample - vgmstream->current_sample;
    }

    seek_force_render(vgmstream, decode_samples);
    //;VGM_LOG("SEEK [simple]: done, cur=%i\n", vgmstream->current_sample);
}


void seek_vgmstream(VGMSTREAM* vgmstream, int32_t seek_sample) {
    //;VGM_LOG("SEEK [main]: s=%i\n", seek_sample);

    seek_sample = clamp_seek(vgmstream, seek_sample);

    bool is_config = vgmstream->config_enabled;
    if (!is_config) {
        seek_simple(vgmstream, seek_sample);
        return;
    }

    play_state_t* ps = &vgmstream->pstate;

    seek_render_t sc = {0};
    sc.is_config = vgmstream->config_enabled;
    sc.is_looped = vgmstream->loop_flag 
        || vgmstream->loop_target > 0; // loop target may disable loop flag during decode
    sc.play_forever = vgmstream->config.play_forever;
    sc.is_body_end = vgmstream->loop_target > 0;

    sc.seek_target = seek_sample;
    sc.seek_decode = seek_sample;

    sc.play_sample = ps->play_position;
    if (!sc.is_config)
        sc.play_sample = vgmstream->current_sample;

    seek_render(vgmstream, &sc);
    return;
}
