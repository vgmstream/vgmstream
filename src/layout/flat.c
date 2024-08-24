#include "layout.h"
#include "../vgmstream.h"
#include "../base/decode.h"
#include "../base/sbuf.h"


/* Decodes samples for flat streams.
 * Data forms a single stream, and the decoder may internally skip chunks and move offsets as needed. */
void render_vgmstream_flat(sample_t* outbuf, int32_t sample_count, VGMSTREAM* vgmstream) {

    int samples_per_frame = decode_get_samples_per_frame(vgmstream);
    int samples_this_block = vgmstream->num_samples; /* do all samples if possible */

    /* write samples */
    int samples_filled = 0;
    while (samples_filled < sample_count) {

        if (vgmstream->loop_flag && decode_do_loop(vgmstream)) {
            /* handle looping */
            continue;
        }

        int samples_to_do = decode_get_samples_to_do(samples_this_block, samples_per_frame, vgmstream);
        if (samples_to_do > sample_count - samples_filled)
            samples_to_do = sample_count - samples_filled;

        if (samples_to_do <= 0) { /* when decoding more than num_samples */
            VGM_LOG_ONCE("FLAT: wrong samples_to_do\n"); 
            goto decode_fail;
        }

        decode_vgmstream(vgmstream, samples_filled, samples_to_do, outbuf);

        samples_filled += samples_to_do;
        vgmstream->current_sample += samples_to_do;
        vgmstream->samples_into_block += samples_to_do;
    }

    return;
decode_fail:
    sbuf_silence_s16(outbuf, sample_count, vgmstream->channels, samples_filled);
}
