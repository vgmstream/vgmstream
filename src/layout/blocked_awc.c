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
    size_t header_size, entries, block_size, block_samples, frame_size;
    size_t channel_header_size;
    int i;

    /* assumes only AWC_IMA/DSP enters here, MPEG/XMA2 need special parsing as blocked layout is too limited.
     * Block header (see awc.c for a complete description):
     * - per channel: header table (size 0x18 or 0x10)
     * - per channel: seek table  (32b * entries = global samples per frame in each block) (not in DSP/Vorbis)
     * - per channel: extra table (DSP only)
     * - padding (not in ATRAC9/DSP)
     */

    entries = read_32bit(block_offset + 0x04, sf); /* se first channel, assume all are the same (not true in MPEG/XMA) */
    block_samples = read_32bit(block_offset + 0x0c, sf);
    block_size = vgmstream->full_block_size;

    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset = block_offset + block_size;
    vgmstream->current_block_samples = block_samples;

    switch(vgmstream->coding_type) {
        case coding_NGC_DSP:
            channel_header_size = 0x10;
            frame_size = 0x08;

            /* coefs on every block but it's always the same */
            dsp_read_coefs_le(vgmstream, sf, block_offset + channel_header_size * vgmstream->channels + 0x10 + 0x1c + 0x00, 0x10 + 0x60);
            dsp_read_hist_le (vgmstream, sf, block_offset + channel_header_size * vgmstream->channels + 0x10 + 0x1c + 0x20, 0x10 + 0x60);

            header_size = 0;
            header_size += channel_header_size * vgmstream->channels; /* header table */
            /* no seek table */
            header_size += 0x70 * vgmstream->channels; /* extra table */
            /* no padding */

            break;

        default:
            channel_header_size = get_channel_header_size(sf, block_offset, vgmstream->codec_endian);
            header_size = get_block_header_size(sf, block_offset, channel_header_size, vgmstream->channels, vgmstream->codec_endian);
            frame_size = 0x800;
            break;
    }

    for (i = 0; i < vgmstream->channels; i++) {
        vgmstream->ch[i].offset = block_offset + header_size + frame_size * entries * i;
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
