#include "layout.h"
#include "../vgmstream.h"
#include "../base/decode.h"


/* Decodes samples for flat streams.
 * Data forms a single stream, and the decoder may internally skip chunks and move offsets as needed. */
rc_t render_layout_flat(sbuf_t* sdst, VGMSTREAM* vgmstream) {
    int samples_per_frame = decode_get_samples_per_frame(vgmstream);
    int samples_this_block = vgmstream->num_samples; /* do all samples if possible */

    /* write samples */
    while (sdst->filled < sdst->samples) {

        if (vgmstream->loop_flag && decode_do_loop(vgmstream)) {
            /* handle looping */
            continue;
        }

        int samples_to_do = decode_get_samples_to_do(samples_this_block, samples_per_frame, vgmstream);
        if (samples_to_do > sdst->samples - sdst->filled)
            samples_to_do = sdst->samples - sdst->filled;

        // TODO: may try to handle more in some cases (ex. #b or seeking)
        // no more samples left to fill
        //if (samples_to_do == 0)
        //    break;

        if (samples_to_do <= 0) {
            VGM_LOG("FLAT: wrong samples_to_do\n"); 
            return RC_LAYOUT_ERROR;
        }

        int curr_filled = sdst->filled;
        decode_vgmstream(sdst, vgmstream, samples_to_do);
        int samples_done = sdst->filled - curr_filled;

        vgmstream->current_sample += samples_done;
        vgmstream->samples_into_block += samples_done;
    }

    return RC_RENDER_OK;
}
