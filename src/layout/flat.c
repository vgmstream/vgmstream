#include "layout.h"
#include "../vgmstream.h"
#include "../decode.h"


/* Decodes samples for flat streams.
 * Data forms a single stream, and the decoder may internally skip chunks and move offsets as needed. */
void render_vgmstream_flat(sample_t* outbuf, int32_t sample_count, VGMSTREAM* vgmstream) {
    int samples_written = 0;
    int samples_per_frame, samples_this_block;

    samples_per_frame = get_vgmstream_samples_per_frame(vgmstream);
    samples_this_block = vgmstream->num_samples; /* do all samples if possible */


    while (samples_written < sample_count) {
        int samples_to_do;

        if (vgmstream->loop_flag && vgmstream_do_loop(vgmstream)) {
            /* handle looping */
            continue;
        }

        samples_to_do = get_vgmstream_samples_to_do(samples_this_block, samples_per_frame, vgmstream);
        if (samples_to_do > sample_count - samples_written)
            samples_to_do = sample_count - samples_written;

        if (samples_to_do == 0) { /* when decoding more than num_samples */
            VGM_LOG_ONCE("FLAT: samples_to_do 0\n"); 
            goto decode_fail;
        }

        decode_vgmstream(vgmstream, samples_written, samples_to_do, outbuf);

        samples_written += samples_to_do;
        vgmstream->current_sample += samples_to_do;
        vgmstream->samples_into_block += samples_to_do;
    }

    return;
decode_fail:
    memset(outbuf + samples_written * vgmstream->channels, 0, (sample_count - samples_written) * vgmstream->channels * sizeof(sample_t));
}
