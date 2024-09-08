#include "../vgmstream.h"
#include "../layout/layout.h"
#include "render.h"
#include "decode.h"
#include "mixing.h"
#include "plugins.h"
#include "sbuf.h"

/* pretend decoder reached loop end so internal state is set like jumping to loop start 
 * (no effect in some layouts but that is ok) */
static void seek_force_loop_end(VGMSTREAM* vgmstream, int loop_count) {
    /* only called after hit loop */
    if (!vgmstream->hit_loop)
        return;

    vgmstream->loop_count = loop_count - 1; /* seeking to first loop must become ++ > 0 */
    vgmstream->current_sample = vgmstream->loop_end_sample;
    decode_do_loop(vgmstream);
}

static void seek_force_decode(VGMSTREAM* vgmstream, int samples) {
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


static void seek_body(VGMSTREAM* vgmstream, int32_t seek_sample) {
    //;VGM_LOG("SEEK: body / seekr=%i, curr=%i\n", seek_sample, vgmstream->current_sample);

    int32_t decode_samples;

    play_state_t* ps = &vgmstream->pstate;
    bool is_looped = vgmstream->loop_flag || vgmstream->loop_target > 0; /* loop target disabled loop flag during decode */
    bool is_config = vgmstream->config_enabled;
    int play_forever = vgmstream->config.play_forever;

    /* seek=10 would be seekr=10-5+3=8 inside decoder */
    int32_t seek_relative = seek_sample - ps->pad_begin_duration + ps->trim_begin_duration;


    /* seek can be in some part of the body, depending on looping/decoder's current position/etc */
    if (!is_looped && seek_relative < vgmstream->current_sample) {
        /* non-looped seek before decoder's position: restart + consume (seekr=50s, curr=95 > restart + decode=50s) */
        decode_samples = seek_relative;
        reset_vgmstream(vgmstream);

        //;VGM_LOG("SEEK: non-loop reset / dec=%i\n", decode_samples);
    }
    else if (!is_looped && seek_relative < vgmstream->num_samples) {
        /* non-looped seek after decoder's position: consume (seekr=95s, curr=50 > decode=95-50=45s) */
        decode_samples = seek_relative - vgmstream->current_sample;

        //;VGM_LOG("SEEK: non-loop forward / dec=%i\n", decode_samples);
    }
    else if (!is_looped) {
        /* after num_samples, can happen when body is set manually (seekr=120s) */
        decode_samples = 0;
        vgmstream->current_sample = vgmstream->num_samples + 1;

        //;VGM_LOG("SEEK: non-loop silence / dec=%i\n", decode_samples);
    }
    else if (seek_relative < vgmstream->loop_start_sample) {
        /* looped seek before decoder's position: restart + consume or just consume */

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
        /* looped seek after loop start: can be clamped between loop parts (relative to decoder's current_sample) to minimize decoding */
        //int32_t loop_outr = (vgmstream->num_samples - vgmstream->loop_end_sample);
        int32_t loop_part = (vgmstream->loop_end_sample - vgmstream->loop_start_sample); /* samples of 1 looped part */
        int32_t loop_seek = (seek_relative - vgmstream->loop_start_sample); /* samples within loop region */
        int     loop_count = loop_seek / loop_part; /* not accurate when loop_target is set */
        loop_seek = loop_seek % loop_part; /* clamp within single loop after calcs */


        /* current must have reached loop start at some point, otherwise force it (NOTE: some layouts don't actually set hit_loop) */
        if (!vgmstream->hit_loop) {
            if (vgmstream->current_sample > vgmstream->loop_start_sample) { /* may be 0 */
                VGM_LOG("SEEK: bad current sample %i vs %i\n", vgmstream->current_sample, vgmstream->loop_start_sample);
                reset_vgmstream(vgmstream);
            }

            int32_t skip_samples = (vgmstream->loop_start_sample - vgmstream->current_sample);
            //;VGM_LOG("SEEK: force loop region / skip=%i, curr=%i\n", skip_samples, vgmstream->current_sample);

            seek_force_decode(vgmstream, skip_samples);
        }

        /* current must be in loop area (may happen at start since it's smaller than loop_end) */
        if (vgmstream->current_sample < vgmstream->loop_start_sample
                || vgmstream->current_sample < vgmstream->loop_end_sample) {
            //;VGM_LOG("SEEK: outside loop region / curr=%i, ls=%i, le=%i\n", vgmstream->current_sample, vgmstream->current_sample, vgmstream->loop_end_sample);
            seek_force_loop_end(vgmstream, 0);
        }

        //;VGM_LOG("SEEK: in loop region / seekr=%i, seekl=%i, loops=%i, dec_curr=%i\n", seek_relative, loop_seek, loop_count, loop_curr);

        /* when "ignore fade" is set and seek falls into the outro part (loop count if bigged than expected), adjust seek
         * to do a whole part + outro samples (should probably calculate correct loop_count before but...) */
        if (vgmstream->loop_target && loop_count >= vgmstream->loop_target) {
            loop_seek = loop_part + (seek_relative - vgmstream->loop_start_sample) - vgmstream->loop_target * loop_part;
            loop_count = vgmstream->loop_target - 1;  /* so seek_force_loop_end detection kicks in and adds +1 */

            //;VGM_LOG("SEEK: outro outside / seek=%i, count=%i\n", decode_samples, loop_seek, loop_count);
        }
        
        int32_t loop_curr = vgmstream->current_sample - vgmstream->loop_start_sample;
        if (loop_seek < loop_curr) {
            decode_samples = loop_seek;
            seek_force_loop_end(vgmstream, loop_count);

            //;VGM_LOG("SEEK: loop reset / dec=%i, loop=%i\n", decode_samples, loop_count);
        }
        else {
            decode_samples = (loop_seek - loop_curr);
            vgmstream->loop_count = loop_count;

            //;VGM_LOG("SEEK: loop forward / dec=%i, loop=%i\n", decode_samples, loop_count);
        }

        /* adjust fade if seek ends in fade region */
        if (is_config && !play_forever
                && seek_sample >= ps->pad_begin_duration + ps->body_duration
                && seek_sample < ps->pad_begin_duration + ps->body_duration + ps->fade_duration) {
            ps->fade_left = ps->pad_begin_duration + ps->body_duration + ps->fade_duration - seek_sample;

            //;VGM_LOG("SEEK: in fade / fade=%i, %i\n", ps->fade_left, ps->fade_duration);
        }
    }

    seek_force_decode(vgmstream, decode_samples);
    //;VGM_LOG("SEEK: decode=%i, current=%i\n", decode_samples, vgmstream->current_sample);
}



void seek_vgmstream(VGMSTREAM* vgmstream, int32_t seek_sample) {
    play_state_t* ps = &vgmstream->pstate;
    int play_forever = vgmstream->config.play_forever;

    bool is_looped = vgmstream->loop_flag || vgmstream->loop_target > 0; /* loop target disabled loop flag during decode */
    bool is_config = vgmstream->config_enabled;

    /* cleanup */
    if (seek_sample < 0)
        seek_sample = 0;
    /* play forever can seek past max */
    if (is_config && seek_sample > ps->play_duration && !play_forever)
        seek_sample = ps->play_duration;


    /* will decode and loop until seek sample, but slower */
    //todo apply same loop logic as below, or pretend we have play_forever + settings?
    if (!is_config) {
        int32_t decode_samples;
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
    if (is_config && seek_sample < ps->pad_begin_duration) {
        /* seek=3: pad=5-3=2 */

        reset_vgmstream(vgmstream);
        ps->pad_begin_left = ps->pad_begin_duration - seek_sample;

        //;VGM_LOG("SEEK: pad start / dec=%i\n", 0);
    }

    /* pad end and beyond: ignored */
    else if (is_config && !play_forever && seek_sample >= ps->pad_begin_duration + ps->body_duration + ps->fade_duration) {
        ps->pad_begin_left = 0;
        ps->trim_begin_left = 0;
        if (!is_looped)
            vgmstream->current_sample = vgmstream->num_samples + 1;

        //;VGM_LOG("SEEK: end silence / dec=%i\n", 0);
        /* looping decoder state isn't changed (seek backwards could use current sample) */
    }

    /* body: seek relative to decoder's current sample */
    else {
#if 0 
        //TODO calculate samples into loop number N, and into fade region (segmented layout can only seek to loop start)

        /* optimize as layouts can seek faster internally */
        if (vgmstream->layout_type == layout_segmented) {
            seek_layout_segmented(vgmstream, seek_sample);
        }
        else if (vgmstream->layout_type == layout_layered) {
            seek_layout_layered(vgmstream, seek_sample);
        }
        else
#endif
        seek_body(vgmstream, seek_sample);

        /* done at the end in case of reset (that restores these values) */
        if (is_config) {
            ps->pad_begin_left = 0;
            ps->trim_begin_left = 0;
        }
    }

    if (is_config)
        vgmstream->pstate.play_position = seek_sample;
}
