#include "vgmstream.h"
#include "layout/layout.h"
#include "render.h"
#include "decode.h"
#include "mixing.h"
#include "plugins.h"


static void seek_force_loop(VGMSTREAM* vgmstream, int loop_count) {
    /* only called after hit loop */
    if (!vgmstream->hit_loop)
        return;

    /* pretend decoder reached loop end so state is set to loop start */
    vgmstream->loop_count = loop_count - 1; /* seeking to first loop must become ++ > 0 */
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
