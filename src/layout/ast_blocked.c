#include "ast_blocked.h"
#include "../vgmstream.h"

/* set up for the block at the given offset */
void ast_block_update(off_t block_offset, VGMSTREAM * vgmstream) {
    int i;
    vgmstream->current_block_offset = block_offset;
    vgmstream->current_block_size = read_32bitBE(
            vgmstream->current_block_offset+4,
            vgmstream->ch[0].streamfile);
    vgmstream->next_block_offset = vgmstream->current_block_offset +
        vgmstream->current_block_size*vgmstream->channels + 0x20;

    for (i=0;i<vgmstream->channels;i++) {
        vgmstream->ch[i].offset = vgmstream->current_block_offset +
            0x20 + vgmstream->current_block_size*i;
    }
}

void render_vgmstream_ast_blocked(sample * buffer, int32_t sample_count, VGMSTREAM * vgmstream) {
    int samples_written=0;

    int frame_size = get_vgmstream_frame_size(vgmstream);
    int samples_per_frame = get_vgmstream_samples_per_frame(vgmstream);
    int samples_this_block;

    samples_this_block = vgmstream->current_block_size / frame_size * samples_per_frame;

    while (samples_written<sample_count) {
        int samples_to_do; 

        if (vgmstream->loop_flag && vgmstream_do_loop(vgmstream)) {
            samples_this_block = vgmstream->current_block_size / frame_size * samples_per_frame;
            continue;
        }

        samples_to_do = vgmstream_samples_to_do(samples_this_block, samples_per_frame, vgmstream);

        if (samples_written+samples_to_do > sample_count)
            samples_to_do=sample_count-samples_written;

        decode_vgmstream(vgmstream, samples_written, samples_to_do, buffer);

        samples_written += samples_to_do;
        vgmstream->current_sample += samples_to_do;
        vgmstream->samples_into_block+=samples_to_do;

        if (vgmstream->samples_into_block==samples_this_block) {
            ast_block_update(vgmstream->next_block_offset,vgmstream);

            samples_this_block = vgmstream->current_block_size / frame_size * samples_per_frame;
            vgmstream->samples_into_block=0;
        }

    }
}
