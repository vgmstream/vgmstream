#include "layout.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "../vgmstream.h"


static size_t get_channel_header_size(STREAMFILE* sf, off_t offset, int big_endian);
static size_t get_block_header_size(STREAMFILE* sf, off_t offset, size_t channel_header_size, int channels, int big_endian);

/* AWC music chunks  */
void block_update_awc(off_t block_offset, VGMSTREAM * vgmstream) {
    STREAMFILE* sf = vgmstream->ch[0].streamfile;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = vgmstream->codec_endian ? read_32bitBE : read_32bitLE;
    size_t header_size, entries, block_size, block_samples;
    size_t  channel_header_size;
    int i;

    /* assumed only AWC_IMA enters here, MPEG/XMA2 need special parsing as blocked layout is too limited */
    entries = read_32bit(block_offset + 0x04, sf); /* se first channel, assume all are the same */
    //block_samples = entries * (0x800-4)*2; //todo use 
    block_samples = read_32bit(block_offset + 0x0c, sf);
    block_size = vgmstream->full_block_size;

    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset = block_offset + block_size;
    vgmstream->current_block_samples = block_samples;

    /* starts with a header block */
    /* for each channel
     *   0x00: start entry within channel (ie. entries * ch) but may be off by +1/+2
     *   0x04: entries
     *   0x08: samples to discard in the beginning of this block (MPEG only?)
     *   0x0c: samples in channel (for MPEG/XMA2 can vary between channels)
     *   (next fields don't exist in later versions for IMA)
     *   0x10: (MPEG only, empty otherwise) close to number of frames but varies a bit?
     *   0x14: (MPEG only, empty otherwise) channel usable data size (not counting padding)
     * for each channel
     *   32b * entries = global samples per frame in each block (for MPEG probably per full frame)
     */

    channel_header_size = get_channel_header_size(sf, block_offset, vgmstream->codec_endian);
    header_size = get_block_header_size(sf, block_offset, channel_header_size, vgmstream->channels, vgmstream->codec_endian);
    for (i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = block_offset + header_size + 0x800*entries*i;
        VGM_ASSERT(entries != read_32bit(block_offset + channel_header_size*i + 0x04, sf), "AWC: variable number of entries found at %lx\n", block_offset);
    }

}

static size_t get_channel_header_size(STREAMFILE* sf, off_t offset, int big_endian) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = big_endian ? read_32bitBE : read_32bitLE;

    /* later games have an smaller channel header, try to detect using
     * an empty field not in IMA */
    if (read_32bit(offset + 0x14, sf) == 0x00)
        return 0x18;
    return 0x10;
}

static size_t get_block_header_size(STREAMFILE* sf, off_t offset, size_t channel_header_size, int channels, int big_endian) {
    size_t header_size = 0;
    int i;
    int entries = channels;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = big_endian ? read_32bitBE : read_32bitLE;

    for (i = 0; i < entries; i++) {
        header_size += channel_header_size;
        header_size += read_32bit(offset + channel_header_size*i + 0x04, sf) * 0x04; /* entries in the table */
    }

    if (header_size % 0x800) /* padded */
        header_size +=  0x800 - (header_size % 0x800);

    return header_size;
}
