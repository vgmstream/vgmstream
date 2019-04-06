#include "layout.h"
#include "../vgmstream.h"


/* Decodes samples for flat streams.
 * Data forms a single stream, and the decoder may internally skip chunks and move offsets as needed. */
void render_vgmstream_flat(sample_t * buffer, int32_t sample_count, VGMSTREAM * vgmstream) {
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

        samples_to_do = vgmstream_samples_to_do(samples_this_block, samples_per_frame, vgmstream);
        if (samples_to_do > sample_count - samples_written)
            samples_to_do = sample_count - samples_written;

        if (samples_to_do == 0) {
            VGM_LOG("layout_flat: wrong samples_to_do 0 found\n"); /* could happen when calling render at EOF? */
            //VGM_LOG("layout_flat: tb=%i sib=%i, spf=%i\n", samples_this_block, vgmstream->samples_into_block, samples_per_frame);
            memset(buffer + samples_written*vgmstream->channels, 0, (sample_count - samples_written) * vgmstream->channels * sizeof(sample_t));
            break;
        }

        decode_vgmstream(vgmstream, samples_written, samples_to_do, buffer);

        samples_written += samples_to_do;
        vgmstream->current_sample += samples_to_do;
        vgmstream->samples_into_block += samples_to_do;
    }
}
