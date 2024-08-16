#include "layout.h"
#include "../vgmstream.h"
#include "../base/decode.h"


/* Decodes samples for interleaved streams.
 * Data has interleaved chunks per channel, and once one is decoded the layout moves offsets,
 * skipping other chunks (essentially a simplified variety of blocked layout).
 * Incompatible with decoders that move offsets. */
void render_vgmstream_interleave(sample_t * buffer, int32_t sample_count, VGMSTREAM * vgmstream) {
    int samples_written = 0;
    int samples_per_frame, samples_this_block; /* used */
    int samples_per_frame_d = 0, samples_this_block_d = 0; /* default */
    int samples_per_frame_f = 0, samples_this_block_f = 0; /* first */
    int samples_per_frame_l = 0, samples_this_block_l = 0; /* last */
    int has_interleave_first = vgmstream->interleave_first_block_size && vgmstream->channels > 1;
    int has_interleave_last = vgmstream->interleave_last_block_size && vgmstream->channels > 1;
    int has_interleave_internal_updates = vgmstream->codec_internal_updates;


    /* setup */
    {
        int frame_size_d = decode_get_frame_size(vgmstream);
        samples_per_frame_d = decode_get_samples_per_frame(vgmstream);
        if (frame_size_d == 0 || samples_per_frame_d == 0) goto fail;
        samples_this_block_d = vgmstream->interleave_block_size / frame_size_d * samples_per_frame_d;
    }
    if (has_interleave_first) {
        int frame_size_f = decode_get_frame_size(vgmstream);
        samples_per_frame_f = decode_get_samples_per_frame(vgmstream); //todo samples per shortframe
        if (frame_size_f == 0 || samples_per_frame_f == 0) goto fail;
        samples_this_block_f = vgmstream->interleave_first_block_size / frame_size_f * samples_per_frame_f;
    }
    if (has_interleave_last) {
        int frame_size_l = decode_get_shortframe_size(vgmstream);
        samples_per_frame_l = decode_get_samples_per_shortframe(vgmstream);
        if (frame_size_l == 0 || samples_per_frame_l == 0) goto fail;
        samples_this_block_l = vgmstream->interleave_last_block_size / frame_size_l * samples_per_frame_l;
    }

    /* set current values */
    if (has_interleave_first &&
            vgmstream->current_sample < samples_this_block_f) {
        samples_per_frame = samples_per_frame_f;
        samples_this_block = samples_this_block_f;
    }
    else if (has_interleave_last &&
                vgmstream->current_sample - vgmstream->samples_into_block + samples_this_block_d > vgmstream->num_samples) {
        samples_per_frame = samples_per_frame_l;
        samples_this_block = samples_this_block_l;
    }
    else {
        samples_per_frame = samples_per_frame_d;
        samples_this_block = samples_this_block_d;
    }

    /* mono interleaved stream with no layout set, just behave like flat layout */
    if (samples_this_block == 0 && vgmstream->channels == 1)
        samples_this_block = vgmstream->num_samples;


    /* write samples */
    while (samples_written < sample_count) {
        int samples_to_do; 

        if (vgmstream->loop_flag && decode_do_loop(vgmstream)) {
            /* handle looping, restore standard interleave sizes */

            if (has_interleave_first &&
                    vgmstream->current_sample < samples_this_block_f) {
                /* use first interleave*/
                samples_per_frame = samples_per_frame_f;
                samples_this_block = samples_this_block_f;
                if (samples_this_block == 0 && vgmstream->channels == 1)
                    samples_this_block = vgmstream->num_samples;
            }
            else if (has_interleave_last) { /* assumes that won't loop back into a interleave_last */
                samples_per_frame = samples_per_frame_d;
                samples_this_block = samples_this_block_d;
                if (samples_this_block == 0 && vgmstream->channels == 1)
                    samples_this_block = vgmstream->num_samples;
            }

            continue;
        }

        samples_to_do = decode_get_samples_to_do(samples_this_block, samples_per_frame, vgmstream);
        if (samples_to_do > sample_count - samples_written)
            samples_to_do = sample_count - samples_written;

        if (samples_to_do == 0) { /* happens when interleave is not set */
            goto fail;
        }

        decode_vgmstream(vgmstream, samples_written, samples_to_do, buffer);

        samples_written += samples_to_do;
        vgmstream->current_sample += samples_to_do;
        vgmstream->samples_into_block += samples_to_do;


        /* move to next interleaved block when all samples are consumed */
        if (vgmstream->samples_into_block == samples_this_block) {
            int ch;

            if (has_interleave_first &&
                    vgmstream->current_sample == samples_this_block_f) {
                /* restore standard frame size after going past first interleave */
                samples_per_frame = samples_per_frame_d;
                samples_this_block = samples_this_block_d;
                if (samples_this_block == 0 && vgmstream->channels == 1)
                    samples_this_block = vgmstream->num_samples;

                for (ch = 0; ch < vgmstream->channels; ch++) {
                    off_t skip =
                            vgmstream->interleave_first_skip*(vgmstream->channels-1-ch) +
                            vgmstream->interleave_first_block_size*(vgmstream->channels-ch) +
                            vgmstream->interleave_block_size*ch;
                    vgmstream->ch[ch].offset += skip;
                }
            }
            else if (has_interleave_last &&
                    vgmstream->current_sample + samples_this_block > vgmstream->num_samples) {
                /* adjust values again if inside last interleave */
                samples_per_frame = samples_per_frame_l;
                samples_this_block = samples_this_block_l;
                if (samples_this_block == 0 && vgmstream->channels == 1)
                    samples_this_block = vgmstream->num_samples;

                for (ch = 0; ch < vgmstream->channels; ch++) {
                    off_t skip =
                            vgmstream->interleave_block_size*(vgmstream->channels-ch) +
                            vgmstream->interleave_last_block_size*ch;
                    vgmstream->ch[ch].offset += skip;
                }
            }
            else if (has_interleave_internal_updates) {
                for (ch = 0; ch < vgmstream->channels; ch++) {
                    off_t skip = vgmstream->interleave_block_size * (vgmstream->channels - 1);
                    vgmstream->ch[ch].offset += skip;
                }
            }
            else {
                for (ch = 0; ch < vgmstream->channels; ch++) {
                    off_t skip = vgmstream->interleave_block_size * vgmstream->channels;
                    vgmstream->ch[ch].offset += skip;
                }
            }

            if (vgmstream->broken_interleave_sample_count != 0 && vgmstream->current_sample == vgmstream->broken_interleave_sample_pos) {
                samples_per_frame = samples_per_frame_d;
                for (ch = 0; ch < vgmstream->channels; ch++) {
                    vgmstream->ch[ch].offset += vgmstream->broken_interleave_sample_count / samples_per_frame;
                }
            }

            vgmstream->samples_into_block = 0;
        }
    }
    return;
fail:
    VGM_LOG_ONCE("layout_interleave: wrong values found\n");
    memset(buffer + samples_written*vgmstream->channels, 0, (sample_count - samples_written) * vgmstream->channels * sizeof(sample_t));
}
