#include "layout.h"
#include "../vgmstream.h"

/* blocks with video and audio */
void block_update_vid1(off_t block_offset, VGMSTREAM* vgmstream) {
    STREAMFILE* sf = vgmstream->ch[0].streamfile;
    int ch;
    int channels = vgmstream->channels;
    uint32_t (*read_u32)(off_t,STREAMFILE*) = vgmstream->codec_endian ? read_u32be : read_u32le;
    size_t audd_size = 0, data_size = 0;


    if (read_u32(block_offset + 0x00, sf) != 0x4652414D) { /* "FRAM" */
        /* signal EOF, as files ends with padding */
        vgmstream->current_block_offset = block_offset;
        vgmstream->next_block_offset = block_offset;
        vgmstream->current_block_size = 0;
        vgmstream->current_block_samples = -1;
        return;
    }

    block_offset += 0x20;
    if (read_u32(block_offset + 0x00, sf) == 0x56494444) { /* "VIDD"*/
        block_offset += read_u32(block_offset + 0x04, sf);
    }

    if (read_u32(block_offset + 0x00, sf) == 0x41554444) { /* "AUDD" */
        audd_size = read_u32(block_offset + 0x04, sf);
        data_size = read_u32(block_offset + 0x0c, sf);
    }

    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset = block_offset + audd_size;
    vgmstream->current_block_size = (data_size / channels);
    vgmstream->current_block_samples = 0;

    for (ch = 0; ch < vgmstream->channels; ch++) {
        off_t interleave, head_size;

        switch(vgmstream->coding_type) {
            case coding_PCM16_int:
                interleave = 0x02 * ch;
                head_size = 0x10;
                break;
            case coding_XBOX_IMA:
                interleave = 0x00;
                head_size = 0x10;
                break;
            case coding_NGC_DSP:
                interleave = (data_size / channels) * ch;
                head_size = 0x20;
                break;
            default:
                interleave = 0;
                head_size = 0x10;
                break;
        }

        vgmstream->ch[ch].offset = block_offset + head_size + interleave;
    }
}
