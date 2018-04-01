#include "layout.h"
#include "../coding/coding.h"
#include "../vgmstream.h"

/* set up for the block at the given offset */
void block_update_xa(off_t block_offset, VGMSTREAM * vgmstream) {
    int i;
    int8_t currentChannel=0;
    int8_t subAudio=0;

    vgmstream->xa_get_high_nibble = 1; /* reset nibble order */

    /* don't change this variable in the init process */
    if (vgmstream->samples_into_block != 0)
        vgmstream->xa_sector_length += 0x80;

    /* XA mode2/form2 sector, size 0x930
     * 0x00: sync word
     * 0x0c: header = minute, second, sector, mode (always 0x02)
     * 0x10: subheader = file, channel (marker), submode flags, xa header
     * 0x14: subheader again
     * 0x18: data
     * 0x918: unused
     * 0x92c: EDC/checksum or null
     * 0x930: end
     * (in non-blocked ISO 2048 mode1/data chunks are 0x800)
     */

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

    // We get to the end of a sector ?
    if (vgmstream->xa_sector_length == (18*0x80)) {
        vgmstream->xa_sector_length = 0;

        // 0x30 of unused bytes/sector :(
        if (!vgmstream->xa_headerless) {
            block_offset += 0x30;
begin:
            // Search for selected channel & valid audio
            currentChannel = read_8bit(block_offset-0x07,vgmstream->ch[0].streamfile);
            subAudio = read_8bit(block_offset-0x06,vgmstream->ch[0].streamfile);

            // audio is coded as 0x64
            if (!((subAudio==0x64) && (currentChannel==vgmstream->xa_channel))) {
                // go to next sector
                block_offset += 0x930;
                if (currentChannel!=-1) goto begin;
            }
        }
    }

    vgmstream->current_block_offset = block_offset;

    // Quid : how to stop the current channel ???
    // i set up 0 to current_block_size to make vgmstream not playing bad samples
    // another way to do it ???
    // (as the number of samples can be false in cd-xa due to multi-channels)
    vgmstream->current_block_size = (currentChannel==-1 ? 0 : 0x70);

    vgmstream->next_block_offset = vgmstream->current_block_offset + 0x80;
    for (i=0;i<vgmstream->channels;i++) {
        vgmstream->ch[i].offset = vgmstream->current_block_offset;
    }
}
