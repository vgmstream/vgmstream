#include "layout.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "../vgmstream.h"


static size_t get_block_header_size(STREAMFILE *streamFile, off_t offset, int channels, int big_endian);

/* AWC music chunks  */
void block_update_awc(off_t block_offset, VGMSTREAM * vgmstream) {
    STREAMFILE* streamFile = vgmstream->ch[0].streamfile;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = vgmstream->codec_endian ? read_32bitBE : read_32bitLE;
    size_t header_size, entries, block_size, block_samples;
    int i;

    /* assumed only AWC_IMA enters here, MPEG/XMA2 need special parsing as blocked layout is too limited */

    entries = read_32bit(block_offset + 0x18*0 + 0x04, streamFile); /* assumed same for all channels */
    block_samples = entries * (0x800-4)*2;
    block_size = vgmstream->full_block_size;

    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset = block_offset + block_size;
    vgmstream->current_block_samples = block_samples;

    /* starts with a header block */
    /* for each channel
     *   0x00: start entry within channel (ie. entries * ch)
     *   0x04: entries
     *   0x08: samples to discard in the beginning of this block (MPEG only?)
     *   0x0c: samples in channel (for MPEG/XMA2 can vary between channels)
     *   0x10: MPEG only: close to number of frames but varies a bit?
     *   0x14: MPEG only: channel usable data size (not counting padding)
     * for each channel
     *   32b * entries = global samples per frame in each block (for MPEG probably per full frame)
     */

    header_size = get_block_header_size(streamFile, block_offset, vgmstream->channels, vgmstream->codec_endian);
    for (i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = block_offset + header_size + 0x800*entries*i;
    }

}

static size_t get_block_header_size(STREAMFILE *streamFile, off_t offset, int channels, int big_endian) {
    size_t header_size = 0;
    int i;
    int entries = channels;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = big_endian ? read_32bitBE : read_32bitLE;

    for (i = 0; i < entries; i++) {
        header_size += 0x18;
        header_size += read_32bit(offset + 0x18*i + 0x04, streamFile) * 0x04; /* entries in the table */
    }

    if (header_size % 0x800) /* padded */
        header_size +=  0x800 - (header_size % 0x800);

    return header_size;
}
