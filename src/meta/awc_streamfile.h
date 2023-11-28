#ifndef _AWC_STREAMFILE_H_
#define _AWC_STREAMFILE_H_
#include "deblock_streamfile.h"
#include "../util/endianness.h"

#define AWC_MAX_MUSIC_CHANNELS 32 /* seen ~24 */

/* ************************************************************************* */

typedef struct {
    int start_entry; /* innacurate! */
    int entries;
    int32_t channel_skip;
    int32_t channel_samples;

    uint32_t frame_size;

    /* derived */
    uint32_t chunk_start; /* relative to block offset */
    uint32_t chunk_size;  /* size of this channel's data (not including padding) */
} awc_block_t;

typedef struct {
    int big_endian;
    uint8_t codec;
    int channels;
    uint32_t block_offset;
    awc_block_t blk[AWC_MAX_MUSIC_CHANNELS];
} awc_block_info_t;

/* Block format:
 * - block header for all channels (needed to find frame start)
 * - frames from channel 1
 * - ...
 * - frames from channel N
 * - usually there is padding between channels or blocks (usually 0s but seen 0x97 in AT9)
 * 
 * Header format:
 * - per channel (frame start table)
 *   0x00: start entry for that channel? (-1 in vorbis)
 *         may be off by +1/+2?
 *         ex. on block 0, ch0/1 have 0x007F frames, a start entry is: ch0=0x0000, ch1=0x007F (MP3)
 *         ex. on block 0, ch0/1 have 0x02A9 frames, a start entry is: ch0=0x0000, ch1=0x02AA (AT9) !!
 *         (sum of all values from all channels may go beyond all posible frames, no idea)
 *   0x04: frames in this channel (may be different between channels)
 *         'frames' here may be actual single decoder frames or a chunk of frames
 *   0x08: samples to discard in the beginning of this block (MPEG/XMA2/Vorbis only?)
 *   0x0c: samples in channel (for MPEG/XMA2 can vary between channels)
 *         full samples without removing samples to discard
 *   (next fields only exists for MPEG, Vorbis or some IMA versions)
 *   0x10: (MPEG only, empty otherwise) close to number of frames but varies a bit?
 *   0x14: (MPEG only, empty otherwise) channel chunk size (not counting padding)
 * - for each channel (seek table)
 *   32b * entries = global samples per frame in each block (for MPEG probably per full frame)
 *   (AT9 doesn't have a seek table as it's CBR)
 * - per channel (ATRAC9/DSP extra info):
 *   0x00: "D11A"
 *   0x04: frame size
 *   0x06: frame samples
 *   0x08: flags? (0x0103=AT9, 0x0104=DSP)
 *   0x0a: sample rate
 *   0x0c: ATRAC9 config (repeated but same for all blocks) or "D11E" (DSP)
 *   0x10-0x70: padding with 0x77 (ATRAC3) or standard DSP header for original full file (DSP)
 * - padding until channel data start, depending on codec (DSP/ATRAC9: one, others: aligned to 0x800)
 * - per channel:
 *   0xNN: channel frames
 *   0xNN: may have padding between channels depending on codec (mainly MPEG/XMA)
 * - padding until this block's end
 */
static bool read_awb_block(STREAMFILE* sf, awc_block_info_t* bi) {
    read_s32_t read_s32 = bi->big_endian ? read_s32be : read_s32le;
    read_u16_t read_u16 = bi->big_endian ? read_u16be : read_u16le;

    uint32_t channel_entry_size, seek_entry_size, extra_entry_size, header_padding;
    uint32_t offset = bi->block_offset;
    int channels = bi->channels;
    /* read stupid block crap + derived info at once so hopefully it's a bit easier to understand */

    switch(bi->codec) {
        case 0x05: /* XMA2 */
            channel_entry_size = 0x10;
            seek_entry_size = 0x04;
            extra_entry_size = 0x00;
            header_padding = 0x800;
            break;
        case 0x08: /* Vorbis */
            channel_entry_size = 0x18;
            seek_entry_size = 0x04;
            extra_entry_size = 0x00;
            header_padding = 0x800;
            break;
        case 0x0F: /* ATRAC9 */
            channel_entry_size = 0x10;
            seek_entry_size = 0x00;
            extra_entry_size = 0x70;
            header_padding = 0x00;
            break;
        default:
            goto fail;
    }

    /* channel info table */
    for (int ch = 0; ch < bi->channels; ch++) {
        bi->blk[ch].start_entry        = read_s32(offset + 0x00, sf);
        bi->blk[ch].entries            = read_s32(offset + 0x04, sf);
        bi->blk[ch].channel_skip       = read_s32(offset + 0x08, sf);
        bi->blk[ch].channel_samples    = read_s32(offset + 0x0c, sf);
        /* others: optional */

        offset += channel_entry_size;
    }

    /* seek table */
    for (int ch = 0; ch < channels; ch++) {
        offset += bi->blk[ch].entries * seek_entry_size;
    }

    /* extra table and derived info */
    for (int ch = 0; ch < channels; ch++) {
        switch(bi->codec) {
            case 0x05: /* XMA2 */
            case 0x08: /* Vorbis */
                /* each 'frame'/entry in Vorbis is actually N vorbis frames then padding up to 0x800
                 * (more or less like a big Ogg page or XMA 'frame'). Padding is considered part of
                 * the data and handled by the decoder, since sfx (non-blocked) algo have it. */
                bi->blk[ch].frame_size = 0x800;
                bi->blk[ch].chunk_size = bi->blk[ch].entries * bi->blk[ch].frame_size;
                break;

            case 0x0F: /* ATRAC9 */
                bi->blk[ch].frame_size = read_u16(offset + 0x04, sf);
                bi->blk[ch].chunk_size = bi->blk[ch].entries * bi->blk[ch].frame_size;
                break;

            default:
                goto fail;
        }
        offset += extra_entry_size;
    }

    /* header done, move into data start */
    if (header_padding) {
        /* padding on the current size rather than file offset (block meant to be read into memory, probably) */
        uint32_t header_size = offset - bi->block_offset;
        offset = bi->block_offset + align_size_to_block(header_size, header_padding);
    }

    /* set frame starts per channel */
    for (int ch = 0; ch < channels; ch++) {
        bi->blk[ch].chunk_start = offset - bi->block_offset;
        offset += bi->blk[ch].chunk_size;
    }

    /* beyond this is padding until chunk_start */

    return true;
fail:
    return false;
}

/* Find data that repeats in the beginning of a new block at the end of last block.
 * When a new block starts there is some repeated data + channel_skip (for seeking + encoder delay?).
 * Detect it so decoder may ignore it. */
static uint32_t get_block_repeated_size(STREAMFILE* sf, awc_block_info_t* bi, int channel) {

    if (bi->blk[channel].channel_skip == 0)
        return 0;

    switch(bi->codec) {
        case 0x05: { /* XMA2 */
            const uint32_t samples_per_subframe = 512;
            uint32_t samples_this_frame;
            uint8_t subframes;
            uint32_t offset = bi->block_offset + bi->blk[channel].chunk_start;
            int repeat_samples = bi->blk[channel].channel_skip;

            //TODO: fix (needs proper decoder + sample discard)
            /* Repeat samples are the number of decoded samples to discard, but in this streamfile we can't do that.
             * Also XMA is VBR, and may encode silent frames with up to 63 subframes yet we may have few repeat samples.
             * We could find out how many subframes of 512 samples to skip, then adjust the XMA frame header, though
             * output will be slightly off since subframes are related.
             *
             * For now just skip a full frame depending on the number of subframes vs repeat samples.
             * Most files work ok-ish but channels may desync slightly. */

            subframes = (read_u8(offset,sf) >> 2) & 0x3F; /* peek into frame header */
            samples_this_frame = subframes * samples_per_subframe;
            if (repeat_samples >= (int)(samples_this_frame * 0.13)) { /* skip mosts */
                return bi->blk[channel].frame_size;
            }
            else {
                return 0;
            }
        }

        case 0x08: /* Vorbis */
            /* when data repeats seems to clone exactly the last super-frame */            
            return bi->blk[channel].frame_size;

        case 0x0F: /* ATRAC9 */
        default: 
            VGM_LOG("AWC: found channel skip in codec %x\n", bi->codec); /* not seen */
            return 0;
    }
}

/* ************************************************************************* */

static void block_callback(STREAMFILE *sf, deblock_io_data* data) {
    int channel = data->cfg.track_number;
    awc_block_info_t bi = {0};

    bi.big_endian = data->cfg.big_endian;
    bi.block_offset = data->physical_offset;
    bi.channels = data->cfg.track_count;
    bi.codec = data->cfg.track_type;

    if (!read_awb_block(sf, &bi))
        return; //???

    uint32_t repeat_size = get_block_repeated_size(sf, &bi, channel);

    data->block_size = data->cfg.chunk_size;
    data->skip_size = bi.blk[channel].chunk_start + repeat_size;
    data->data_size = bi.blk[channel].chunk_size - repeat_size;
}

/* deblocks AWC blocks */
static STREAMFILE* setup_awc_streamfile(STREAMFILE* sf, uint32_t stream_offset, uint32_t stream_size, uint32_t block_size, int channels, int channel, uint8_t codec, int big_endian) {
    STREAMFILE* new_sf = NULL;
    deblock_config_t cfg = {0};

    if (channels >= AWC_MAX_MUSIC_CHANNELS)
        return NULL;

    cfg.track_number = channel;
    cfg.track_count = channels;
    cfg.stream_start = stream_offset;
    cfg.stream_size = stream_size;
    cfg.chunk_size = block_size;
    cfg.track_type = codec;
    cfg.big_endian = big_endian;
    //cfg.physical_offset = stream_offset;
    //cfg.logical_size = awc_xma_io_size(sf, &cfg); /* force init */
    cfg.block_callback = block_callback;

    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_deblock_streamfile_f(new_sf, &cfg);
    //new_sf = open_buffer_streamfile_f(new_sf, 0);
    return new_sf;
}

#endif
