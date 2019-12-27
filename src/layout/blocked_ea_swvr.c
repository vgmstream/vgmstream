#include "layout.h"
#include "../coding/coding.h"

/* EA-style blocks */
void block_update_ea_swvr(off_t block_offset, VGMSTREAM * vgmstream) {
    STREAMFILE* streamFile = vgmstream->ch[0].streamfile;
    int i;
    size_t block_size, header_size = 0, channel_size = 0, interleave = 0;
    uint32_t block_id;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = vgmstream->codec_endian ? read_32bitBE : read_32bitLE;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = vgmstream->codec_endian ? read_16bitBE : read_16bitLE;

    block_id   = read_32bit(block_offset+0x00, streamFile);
    block_size = read_32bit(block_offset+0x04, streamFile);

    /* parse blocks (Freekstyle uses multiblocks) */
    switch(block_id) {
        case 0x5641474D: /* "VAGM" */
            if (read_16bit(block_offset+0x1a, streamFile) == 0x0024) {
                header_size = 0x40;
                channel_size = (block_size - header_size) / vgmstream->channels;

                /* ignore blocks of other subsongs */
                {
                    int target_subsong = vgmstream->stream_index ? vgmstream->stream_index : 1;
                    if (read_32bit(block_offset+0x0c, streamFile)+1 != target_subsong) {
                        channel_size = 0;
                    }
                }
            } else {
                header_size = 0x1c;
                channel_size = (block_size - header_size) / vgmstream->channels;
            }
            break;
        case 0x56414742: /* "VAGB" */
            if (read_16bit(block_offset+0x1a, streamFile) == 0x6400) {
                header_size = 0x40;
            } else {
                header_size = 0x18;
            }
            channel_size = (block_size - header_size) / vgmstream->channels;
            break;

        case 0x4453504D: /* "DSPM" */
            header_size = 0x60;
            channel_size = (block_size - header_size) / vgmstream->channels;

            /* ignore blocks of other subsongs */
            {
                int target_subsong = vgmstream->stream_index ? vgmstream->stream_index : 1;
                if (read_32bit(block_offset+0x0c, streamFile)+1 != target_subsong) {
                    channel_size = 0;
                }
            }
            dsp_read_coefs_be(vgmstream, streamFile, block_offset+0x1a, 0x22);
            //todo adpcm history?
            break;
        case 0x44535042: /* "DSPB" */
            header_size = 0x40;
            channel_size = (block_size - header_size) / vgmstream->channels;
            dsp_read_coefs_be(vgmstream, streamFile, block_offset+0x18, 0x00);
            //todo adpcm history?
            break;

        case 0x4D534943: /* "MSIC" */
            header_size = 0x1c;
            channel_size = (block_size - header_size) / vgmstream->channels;
            break;

        case 0x53484F43: /* "SHOC" (a generic block but hopefully has PC sounds) */
            if (read_32bit(block_offset+0x10, streamFile) == 0x53444154) { /* "SDAT" */
                header_size = 0x14;
                channel_size = (block_size - header_size) / vgmstream->channels;
            }
            else {
                header_size = 0;
                channel_size = 0;
            }
            break;

        case 0x46494C4C: /* "FILL" (FILLs do that up to a block size, but near end don't actually have size) */
            if ((block_offset + 0x04) % 0x6000 == 0)
                block_size = 0x04;
            else if ((block_offset + 0x04) % 0x10000 == 0)
                block_size = 0x04;
            else if (block_size > 0x100000) { /* other unknown block sizes */
                VGM_LOG("EA SWVR: bad block size at 0x%lx\n", block_offset);
                block_size = 0x04;
            }
            header_size = 0x08;
            break;
 
        case 0xFFFFFFFF:
            channel_size = -1; /* signal bad block */
            break;

        default: /* ignore, 0 samples */
            //;VGM_LOG("EA SWVR: ignored 0x%08x at 0x%lx\n", block_id, block_offset);
            break;
    }

    vgmstream->current_block_size = channel_size;
    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset = block_offset + block_size;

    interleave = vgmstream->coding_type == coding_PCM8_U_int ? 0x1 : channel_size;
    for (i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = block_offset + header_size + interleave*i;
    }
}
