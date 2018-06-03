#include "layout.h"
#include "../coding/coding.h"


/* H4M video blocks with audio frames, based on h4m_audio_decode */
void block_update_h4m(off_t block_offset, VGMSTREAM * vgmstream) {
    STREAMFILE* streamFile = vgmstream->ch[0].streamfile;
    int i;
    size_t block_size, block_samples;


    /* use full_block_size as counter (a bit hacky but whatevs) */
    if (vgmstream->full_block_size <= 0) {
        /* new full block */
        /* 0x00: last_full_block_size */
        uint32_t full_block_size      = read_32bitBE(block_offset+0x04, streamFile);
        /* 0x08: vid_frame_count */
        /* 0x0c: aud_frame_count */
        /* 0x10: block_header_unk (0x01000000, except 0 in a couple of Bomberman Jetters files) */

        vgmstream->full_block_size = full_block_size; /* not including 0x14 block header */
        block_size = 0x14; /* skip header and point to first frame in full block */
        block_samples = 0; /* signal new block_update_h4m */
    }
    else {
        /* new audio or video frames in the current full block */
        uint16_t frame_type = read_16bitBE(block_offset+0x00, streamFile);
        uint16_t frame_format = read_16bitBE(block_offset+0x02, streamFile);
        uint32_t frame_size = read_32bitBE(block_offset+0x04, streamFile); /* not including 0x08 frame header */


        if (frame_type == 0x00) {
            /* HVQM4_AUDIO (there are more checks with frame_format but not too relevant for vgmstream) */
            uint32_t frame_samples = read_32bitBE(block_offset+0x08, streamFile);
            size_t block_skip;

            if (vgmstream->codec_version & 0x80) {
                frame_samples /= 2; /* ??? */
            }

            block_skip = 0x08 + 0x04;
            block_size = 0x08 + frame_size;
            block_samples = frame_samples;


            /* skip data from other audio tracks */
            if (vgmstream->num_streams) {
                uint32_t audio_bytes = frame_size - 0x04;
                block_skip += (audio_bytes / vgmstream->num_streams) * vgmstream->stream_index;
            }

            //VGM_ASSERT(frame_format < 1 && frame_format > 3, "H4M: unknown frame_format %x at %lx\n", frame_format, block_offset);
            VGM_ASSERT(frame_format == 1, "H4M: unknown frame_format %x at %lx\n", frame_format, block_offset);

            //todo handle in the decoder?
            //todo right channel first?
            /* get ADPCM hist (usually every new block) */
            for (i = 0; i < vgmstream->channels; i++) {
                if (frame_format == 1) { /* combined hist+index */
                    vgmstream->ch[i].adpcm_history1_32 = read_16bitBE(block_offset + block_skip + 0x02*i + 0x00,streamFile) & 0xFFFFFF80;
                    vgmstream->ch[i].adpcm_step_index = read_8bit(block_offset + block_skip + 0x02*i + 0x01,streamFile) & 0x7f;
                    vgmstream->ch[i].offset = block_offset + block_skip + 0x02*vgmstream->channels;
                }
                else if (frame_format == 3) { /* separate hist+index */
                    vgmstream->ch[i].adpcm_history1_32 = read_16bitBE(block_offset + block_skip + 0x03*i + 0x00,streamFile);
                    vgmstream->ch[i].adpcm_step_index = read_8bit(block_offset + block_skip + 0x03*i + 0x02,streamFile);
                    vgmstream->ch[i].offset = block_offset + block_skip + 0x03*vgmstream->channels;
                }
                else if (frame_format == 2) { /* no hist/index */
                    vgmstream->ch[i].offset = block_offset + block_skip;
                }
            }

            //todo temp hack, at it must write header sample and ignore the last nibble to get fully correct output
            if (frame_format == 1 || frame_format == 3) {
                block_samples--;
            }
        }
        else {
            block_size = 0x08 + frame_size;
            block_samples = 0; /* signal new block_update_h4m */
        }

        vgmstream->full_block_size -= block_size;
    }

    /* EOF check, there is some footer/garbage at the end */
    if (block_offset == get_streamfile_size(streamFile)
            || block_offset + block_size > get_streamfile_size(streamFile)) {
        //block_samples = -1; /* signal end block */
        vgmstream->full_block_size = 0;
        vgmstream->current_block_samples = 0;
        vgmstream->current_block_offset = get_streamfile_size(streamFile);
        vgmstream->next_block_offset = get_streamfile_size(streamFile);
        return;
    }

    vgmstream->current_block_samples = block_samples;
    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset = block_offset + block_size;
}

