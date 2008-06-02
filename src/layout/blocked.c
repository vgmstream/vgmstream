#include "layout.h"
#include "../vgmstream.h"

void render_vgmstream_blocked(sample * buffer, int32_t sample_count, VGMSTREAM * vgmstream) {
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

        if (vgmstream->current_block_offset>=0)
            decode_vgmstream(vgmstream, samples_written, samples_to_do, buffer);
        else {
            int i;
            /* we've run off the end! */
            for (i=samples_written*vgmstream->channels;
                    i<(samples_written+samples_to_do)*vgmstream->channels;i++)
                    buffer[i]=0;
        }

        samples_written += samples_to_do;
        vgmstream->current_sample += samples_to_do;
        vgmstream->samples_into_block+=samples_to_do;

        if (vgmstream->samples_into_block==samples_this_block) {
            switch (vgmstream->layout_type) {
                case layout_ast_blocked:
                    ast_block_update(vgmstream->next_block_offset,vgmstream);
                    break;
                case layout_halpst_blocked:
                    if (vgmstream->next_block_offset>=0)
                        halpst_block_update(vgmstream->next_block_offset,vgmstream);
                    else
                        vgmstream->current_block_offset=-1;
                    break;
				case layout_xa_blocked:
					xa_block_update(vgmstream->next_block_offset,vgmstream);
					break;
				case layout_xbox_blocked:
					xbox_block_update(vgmstream->next_block_offset,vgmstream);
					break;
				case layout_ea_blocked:
					ea_block_update(vgmstream->next_block_offset,vgmstream);
					break;
                default:
                    break;
            }

            samples_this_block = vgmstream->current_block_size / frame_size * samples_per_frame;
            vgmstream->samples_into_block=0;
        }

    }
}
