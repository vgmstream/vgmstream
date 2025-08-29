#include "layout.h"
#include "../vgmstream.h"
#include "../util/endianness.h"
#include "../coding/coding.h"


void block_update_thp(off_t block_offset, VGMSTREAM* vgmstream) {
    STREAMFILE* sf = vgmstream->ch[0].streamfile;

    bool big_endian = vgmstream->codec_endian;
    read_u32_t read_u32 = big_endian ? read_u32be : read_u32le;


    uint32_t next_block_size    = read_u32(block_offset + 0x00, sf); /* block may have padding, so need to save for next time */
    // 0x04: previous frame size
    uint32_t video_size         = read_u32(block_offset + 0x08,sf);
    // 0x0c: audio size

    uint32_t audio_offset = block_offset + 0x10 + video_size;

    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset = block_offset + vgmstream->full_block_size; /* use prev saved data */
    vgmstream->full_block_size = next_block_size;

    /* block samples can be smaller than block size, normally in the last block,
     * but num_samples already takes that into account, so there is no real difference */
    vgmstream->current_block_size       = read_u32(audio_offset + 0x00, sf);
    vgmstream->current_block_samples    = read_u32(audio_offset + 0x04, sf);

    audio_offset += 0x08;

    // always reserves 2 channels even in mono (size 0x40) [WarioWare Inc. (GC)]
    dsp_read_coefs(vgmstream, sf, audio_offset, 0x20, big_endian);
    dsp_read_hist (vgmstream, sf, audio_offset + 2 * 0x20, 0x04, big_endian);

    for (int i = 0; i < vgmstream->channels; i++) {
        /* always size of 2 channels even in mono [WarioWare Inc. (GC)] */
        #if 0
        off_t coef_offset = audio_offset + i*0x20;
        off_t hist_offset = audio_offset + 2*0x20 + i*0x04;
        #endif
        off_t data_offset = audio_offset + 2*0x24 + i * vgmstream->current_block_size;

#if 0
        for (int j = 0; j < 16; j++) {
            vgmstream->ch[i].adpcm_coef[j] = read_s16be(coef_offset + (j*0x02),sf);
        }
        vgmstream->ch[i].adpcm_history1_16 = read_s16be(hist_offset + 0x00,sf);
        vgmstream->ch[i].adpcm_history2_16 = read_s16be(hist_offset + 0x02,sf);
#endif
        vgmstream->ch[i].offset = data_offset;
    }
}
