#include "layout.h"
#include "../coding/coding.h"
#include "../vgmstream.h"

/* parse a CD-XA raw mode2/form2 sector */
void block_update_xa(off_t block_offset, VGMSTREAM * vgmstream) {
    STREAMFILE* streamFile = vgmstream->ch[0].streamfile;
    int i;
    size_t block_samples;
    uint8_t xa_submode;


    /* XA mode2/form2 sector, size 0x930
     * 0x00: sync word
     * 0x0c: header = minute, second, sector, mode (always 0x02)
     * 0x10: subheader = file, channel (substream marker), submode flags, xa header
     * 0x14: subheader again (for error correction)
     * 0x18: data
     * 0x918: unused
     * 0x92c: EDC/checksum or null
     * 0x930: end
     */

    /* channel markers supposedly could be used to interleave streams, ex. audio languages within video
     * (extractors may split .XA using channels?) */
    VGM_ASSERT(block_offset + 0x930 < get_streamfile_size(streamFile) &&
            (uint8_t)read_8bit(block_offset + 0x000 + 0x11,streamFile) !=
            (uint8_t)read_8bit(block_offset + 0x930 + 0x11,streamFile),
            "XA block: subchannel change at %lx\n", block_offset);

    /* submode flag bits (typical audio value = 0x64)
     * - 7: end of file
     * - 6: real time mode
     * - 5: sector form (0=form1, 1=form2)
     * - 4: trigger (for application)
     * - 3: data sector
     * - 2: audio sector
     * - 1: video sector
     * - 0: end of audio
     */
    xa_submode = (uint8_t)read_8bit(block_offset + 0x12,streamFile);

    /* audio sector must set/not set certain flags, as per spec */
    if ((xa_submode & 0x20) && !(xa_submode & 0x08) && (xa_submode & 0x04) && !(xa_submode & 0x02) ) {
        block_samples = (28*8 / vgmstream->channels) * 18; /* size 0x900, 18 frames of size 0x80 with 8 subframes of 28 samples */
    }
    else {
        block_samples = 0; /* not an audio sector */
        ;VGM_LOG("XA block: non audio block found at %lx\n", block_offset);
    }

    vgmstream->current_block_offset = block_offset;
    vgmstream->current_block_samples = block_samples;
    vgmstream->next_block_offset = block_offset + 0x930;

    for (i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = block_offset + 0x18;
    }
}
