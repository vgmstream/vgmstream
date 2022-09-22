#include "layout.h"
#include "../coding/coding.h"
#include "../vgmstream.h"

/* weird mix of Ubi ADPCM format with Ubi IMA, found in Splinter Cell Essentials (PSP) */
void block_update_ubi_sce(off_t block_offset, VGMSTREAM* vgmstream) {
    STREAMFILE* sf = vgmstream->ch[0].streamfile;
    int i, channels;
    size_t header_size, frame_size, subframe_size, padding_size;

    /* format mimics Ubi ADPCM's:
     * - 0x34/38 header with frame size (ex. 0x600), pre-read in meta
     * xN frames:
     * - 0x34 channel header per channel, with ADPCM config
     * - subframe (ex. 0x300) + padding byte
     * - subframe (ex. 0x300) + padding byte
     *
     * to skip the padding byte we'll detect subframes using codec_config as a counter
     * (higher bit has a special meaning)
     */

    if ((vgmstream->codec_config & 1) == 0) {
        header_size = 0x34; /* read header in first subframe */
    }
    else {
        header_size = 0x00;
    }
    vgmstream->codec_config ^= 1; /* swap counter bit */

    channels = vgmstream->channels;
    frame_size = vgmstream->full_block_size;
    subframe_size = frame_size / 2;
    padding_size = 0x01;

    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset = block_offset +  header_size * vgmstream->channels + subframe_size + padding_size;
    vgmstream->current_block_samples = ima_bytes_to_samples(subframe_size, channels);

    for (i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = block_offset + header_size * channels;

        if (header_size > 0) {
            vgmstream->ch[i].adpcm_step_index  = read_32bitLE(block_offset + header_size * i + 0x04, sf);
            vgmstream->ch[i].adpcm_history1_32 = read_32bitLE(block_offset + header_size * i + 0x08, sf);

            /* First step is always 0x500, not sure if it's a bug or a feature but the game just takes it as is and
             * ends up reading 0 from out-of-bounds memory area which causes a pop at the start. Yikes.
             * It gets clamped later so the rest of the sound plays ok.
             * We put 89 here as our special index which contains 0 to simulate this.
             */
            if (vgmstream->ch[i].adpcm_step_index == 0x500) {
                vgmstream->ch[i].adpcm_step_index = 89;
            }
        }
    }
}
