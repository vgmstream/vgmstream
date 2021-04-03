#include "layout.h"
#include "../coding/coding.h"


/* H4M video blocks with audio frames, based on h4m_audio_decode */
void block_update_h4m(off_t block_offset, VGMSTREAM* vgmstream) {
    STREAMFILE* sf = vgmstream->ch[0].streamfile;
    int i;
    size_t block_size, block_samples;


    /* use full_block_size as counter (a bit hacky but whatevs) */
    if (vgmstream->full_block_size <= 0) {
        /* new full block */
        /* 0x00: last_full_block_size */
        uint32_t full_block_size      = read_32bitBE(block_offset+0x04, sf);
        /* 0x08: vid_frame_count */
        /* 0x0c: aud_frame_count */
        /* 0x10: block_header_unk (0x01000000, except 0 in a couple of Bomberman Jetters files) */

        vgmstream->full_block_size = full_block_size; /* not including 0x14 block header */
        block_size = 0x14; /* skip header and point to first frame in full block */
        block_samples = 0; /* signal new block_update_h4m */
    }
    else {
        /* new audio or video frames in the current full block */
        uint16_t frame_type = read_16bitBE(block_offset+0x00, sf);
        uint16_t frame_format = read_16bitBE(block_offset+0x02, sf);
        uint32_t frame_size = read_32bitBE(block_offset+0x04, sf); /* not including 0x08 frame header */


        if (frame_type == 0x00) {
            /* HVQM4_AUDIO (there are more checks with frame_format but not too relevant for vgmstream) */
            uint32_t frame_samples = read_32bitBE(block_offset+0x08, sf);
            size_t block_skip;

            if (vgmstream->codec_config & 0x80) {
                frame_samples /= 2; /* ??? */
            }

            block_skip = 0x08 + 0x04;
            block_size = 0x08 + frame_size;
            block_samples = frame_samples;

            /* skip data from other audio tracks */
            if (vgmstream->num_streams > 1 && vgmstream->stream_index > 1) {
                uint32_t audio_bytes = frame_size - 0x04;
                block_skip += (audio_bytes / vgmstream->num_streams) * (vgmstream->stream_index-1);
            }

            VGM_ASSERT(frame_format == 1, "H4M: unknown frame_format %x at %x\n", frame_format, (uint32_t)block_offset);

            /* pass current mode to the decoder */
            vgmstream->codec_config = (frame_format << 8) | (vgmstream->codec_config & 0xFF);

            for (i = 0; i < vgmstream->channels; i++) {
                vgmstream->ch[i].offset = block_offset + block_skip;
            }
        }
        else {
            block_size = 0x08 + frame_size;
            block_samples = 0; /* signal new block_update_h4m */
        }

        vgmstream->full_block_size -= block_size;
    }

    /* EOF check, there is some footer/garbage at the end */
    if (block_offset == get_streamfile_size(sf)
            || block_offset + block_size > get_streamfile_size(sf)) {
        //block_samples = -1; /* signal end block */
        vgmstream->full_block_size = 0;
        vgmstream->current_block_samples = 0;
        vgmstream->current_block_offset = get_streamfile_size(sf);
        vgmstream->next_block_offset = get_streamfile_size(sf);
        return;
    }

    vgmstream->current_block_samples = block_samples;
    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset = block_offset + block_size;
}

