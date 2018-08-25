#include "layout.h"
#include "../vgmstream.h"


/* Decodes samples for interleaved streams.
 * Data has interleaved chunks per channel, and once one is decoded the layout moves offsets,
 * skipping other chunks (essentially a simplified variety of blocked layout).
 * Incompatible with decoders that move offsets. */
void render_vgmstream_interleave(sample * buffer, int32_t sample_count, VGMSTREAM * vgmstream) {
    int samples_written = 0;
    int frame_size, samples_per_frame, samples_this_block;
    int has_interleave_last = vgmstream->interleave_last_block_size && vgmstream->channels > 1;

    frame_size = get_vgmstream_frame_size(vgmstream);
    samples_per_frame = get_vgmstream_samples_per_frame(vgmstream);
    samples_this_block = vgmstream->interleave_block_size / frame_size * samples_per_frame;

    if (has_interleave_last &&
            vgmstream->current_sample - vgmstream->samples_into_block + samples_this_block > vgmstream->num_samples) {
        /* adjust values again if inside last interleave */
        frame_size = get_vgmstream_shortframe_size(vgmstream);
        samples_per_frame = get_vgmstream_samples_per_shortframe(vgmstream);
        samples_this_block = vgmstream->interleave_last_block_size / frame_size * samples_per_frame;
    }

    /* mono interleaved stream with no layout set, just behave like flat layout */
    if (samples_this_block == 0 && vgmstream->channels == 1)
        samples_this_block = vgmstream->num_samples;


    while (samples_written < sample_count) {
        int samples_to_do; 

        if (vgmstream->loop_flag && vgmstream_do_loop(vgmstream)) {
            /* handle looping, restore standard interleave sizes */
            if (has_interleave_last) { /* assumes that won't loop back into a interleave_last */
                frame_size = get_vgmstream_frame_size(vgmstream);
                samples_per_frame = get_vgmstream_samples_per_frame(vgmstream);
                samples_this_block = vgmstream->interleave_block_size / frame_size * samples_per_frame;
                if (samples_this_block == 0 && vgmstream->channels == 1)
                    samples_this_block = vgmstream->num_samples;
            }
            continue;
        }

        samples_to_do = vgmstream_samples_to_do(samples_this_block, samples_per_frame, vgmstream);
        if (samples_to_do > sample_count - samples_written)
            samples_to_do = sample_count - samples_written;

        if (samples_to_do == 0) { /* happens when interleave is not set */
            VGM_LOG("layout_interleave: wrong samples_to_do found\n");
            memset(buffer + samples_written*vgmstream->channels, 0, (sample_count - samples_written) * vgmstream->channels * sizeof(sample));
            break;
        }

        decode_vgmstream(vgmstream, samples_written, samples_to_do, buffer);

        samples_written += samples_to_do;
        vgmstream->current_sample += samples_to_do;
        vgmstream->samples_into_block += samples_to_do;


        /* move to next interleaved block when all samples are consumed */
        if (vgmstream->samples_into_block == samples_this_block) {
            int ch;

            if (has_interleave_last &&
                    vgmstream->current_sample + samples_this_block > vgmstream->num_samples) {
                /* adjust values again if inside last interleave */
                frame_size = get_vgmstream_shortframe_size(vgmstream);
                samples_per_frame = get_vgmstream_samples_per_shortframe(vgmstream);
                samples_this_block = vgmstream->interleave_last_block_size / frame_size * samples_per_frame;
                if (samples_this_block == 0 && vgmstream->channels == 1)
                    samples_this_block = vgmstream->num_samples;

                for (ch = 0; ch < vgmstream->channels; ch++) {
                    off_t skip = vgmstream->interleave_block_size*(vgmstream->channels-ch) +
                            vgmstream->interleave_last_block_size*ch;;
                    vgmstream->ch[ch].offset += skip;
                }
            }
            else {
                for (ch = 0; ch < vgmstream->channels; ch++) {
                    off_t skip = vgmstream->interleave_block_size*vgmstream->channels;
                    vgmstream->ch[ch].offset += skip;
                }
            }

            vgmstream->samples_into_block = 0;
        }

    }
}
