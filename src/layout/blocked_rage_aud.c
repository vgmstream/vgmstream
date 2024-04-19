#include "layout.h"
#include "../vgmstream.h"
#include "../util/endianness.h"

/* RAGE AUD (MC:LA, GTA IV) blocks */
void block_update_rage_aud(off_t block_offset, VGMSTREAM* vgmstream) {
    STREAMFILE* sf = vgmstream->ch[0].streamfile;
    size_t header_size, block_samples, seek_info_size, seek_info_entry_size;
    off_t seek_info_offset;
    int i;

    //big_endian = read_u32le(block_offset, sf) == 0; /* 64-bit number */
    read_u64_t read_u64 = vgmstream->codec_endian ? read_u64be : read_u64le;
    read_u32_t read_u32 = vgmstream->codec_endian ? read_u32be : read_u32le;


    /* base header */
    seek_info_offset = read_u64(block_offset + 0x00, sf); /* 0x18 */
    seek_info_size = read_u64(block_offset + 0x08, sf);
    /* 0x10(8): seek table offset; should be identical to seek_info_size */

    /* entries are 0x10 long on PC & X360, and 0x18 on PS3 */
    seek_info_entry_size = (seek_info_size - seek_info_offset) / vgmstream->channels;
    /* seek info (per channel) */
    /* 0x00: start entry */
    /* 0x04: number of entries */
    /* 0x08: unknown */
    /* 0x0c: data size */

    /* seek table (per all entries) */
    /* 0x00: start? */
    /* 0x04: end? */


    /* find header size */
    /* can't see a better way to calc, as there may be dummy entries after usable ones
     * (table is max 0x7b8 + seek table offset + 0x800-padded) */

    /* TODO: This isn't really reliable, there are several very short 4-7ch streams in
     * both MCLA and GTA4 whose seek tables are small enough to fit in 0x800 alignment
     *
     * The best option might be to search for the highest start_entry offset across all the
     * seek info entries (offsets 0x00 and 0x04, can be along with the block_samples loop),
     * and do seek_info_size + furthest_offset * 0x08 + num_entries * 0x08, since sometimes
     * the number of seek entries are off by one, so just adding them all up won't match.
     *
     * However this should always be done from the 1st stream block, or at the very least
     * not the final block, since it can have less data left over due to it being the end
     * of the stream, where the calculation would result in it being smaller than it is.
     */
    if (vgmstream->channels > 3)
        header_size = 0x1000;
    else
        header_size = 0x800;

    /* get max data_size as channels may vary slightly (data is padded, hopefully won't create pops) */
    block_samples = 0;
    seek_info_offset += block_offset;
    for (i = 0; i < vgmstream->channels; i++) {
        size_t channel_samples = read_u32(seek_info_offset + 0x0c + seek_info_entry_size * i, sf);
        if (block_samples < channel_samples)
            block_samples = channel_samples;
    }

    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset = block_offset + vgmstream->full_block_size;
    vgmstream->current_block_samples = block_samples;
    vgmstream->current_block_size = 0;

    for (i = 0; i < vgmstream->channels; i++) {
        /* use seek table's start entry to find channel offset */
        size_t interleave_size = read_u32(seek_info_offset + 0x00 + seek_info_entry_size * i, sf) * 0x800;
        vgmstream->ch[i].offset = block_offset + header_size + interleave_size;
    }
}
