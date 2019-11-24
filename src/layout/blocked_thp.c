#include "layout.h"
#include "../vgmstream.h"

/* set up for the block at the given offset */
void block_update_thp(off_t block_offset, VGMSTREAM *vgmstream) {
    int i, j;
    STREAMFILE *streamFile = vgmstream->ch[0].streamfile;
    off_t audio_offset;
    size_t next_block_size, video_size;

    next_block_size = read_32bitBE(block_offset + 0x00, streamFile);
    /* 0x04: frame size previous */
    video_size = read_32bitBE(block_offset + 0x08,streamFile);
    /* 0x0c: audio size */

    audio_offset = block_offset + 0x10 + video_size;

    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset = block_offset + vgmstream->full_block_size;
    vgmstream->full_block_size = next_block_size;

    /* block samples can be smaller than block size, normally in the last block,
     * but num_samples already takes that into account, so there is no real difference */
    vgmstream->current_block_size = read_32bitBE(audio_offset + 0x00, streamFile);
    vgmstream->current_block_samples = read_32bitBE(audio_offset + 0x04, streamFile);

    audio_offset += 0x08;

    for (i = 0; i < vgmstream->channels; i++) {
        off_t coef_offset = audio_offset + i*0x20;
        off_t hist_offset = audio_offset + vgmstream->channels*0x20 + i*0x04;
        off_t data_offset = audio_offset + vgmstream->channels*0x24 + i*vgmstream->current_block_size;

        for (j = 0; j < 16; j++) {
            vgmstream->ch[i].adpcm_coef[j] = read_16bitBE(coef_offset + (j*0x02),streamFile);
        }
        vgmstream->ch[i].adpcm_history1_16 = read_16bitBE(hist_offset + 0x00,streamFile);
        vgmstream->ch[i].adpcm_history2_16 = read_16bitBE(hist_offset + 0x02,streamFile);
        vgmstream->ch[i].offset = data_offset;
    }
}
