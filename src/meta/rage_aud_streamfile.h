#ifndef _RAGE_AUD_STREAMFILE_H_
#define _RAGE_AUD_STREAMFILE_H_
#include "deblock_streamfile.h"
#include "../util/endianness.h"
#include "../util/log.h"
#include "../coding/coding.h"

#define RAGE_AUD_MAX_MUSIC_CHANNELS 7 /* known max */
#define RAGE_AUD_FRAME_SIZE 0x800

/* ************************************************************************* */

typedef struct {
    int start_entry;
    int entries;
    int32_t channel_skip;
    int32_t channel_samples;
    uint32_t channel_size;      /* size of this channel's data (not including padding) */

    uint32_t frame_size;

    /* derived */
    uint32_t chunk_start;       /* relative to block offset */
    uint32_t chunk_size;        /* size of this channel's data (may include padding) */
} rage_aud_block_t;

typedef struct {
    int big_endian;
    int codec;
    int channels;
    uint32_t block_offset;
    uint32_t header_size;
    rage_aud_block_t blk[RAGE_AUD_MAX_MUSIC_CHANNELS];
} rage_aud_block_info_t;

/* Block format:
 * - block header for all channels (needed to find frame start)
 * - frames from channel 1
 * - ...
 * - frames from channel N
 * - usually there is padding between channels or blocks
 *
 * Header format:
 * - base header (for all channels)
 *   0x00: seek info offset (within block)
 *   0x08: seek table offset
 *   0x10: seek table offset
 * - channel info (per channel)
 *   0x00: start entry/frame for that channel
 *         Sometimes a channel has N frames while next channel start_entry N+1/2, meaning last frames will be blank/padding
 *   0x04: entries/frames in this channel (may be different between channels)
 *         This refers to 1 logical chunk of N sub-frames (XMA1=single XMA super-frame, MPEG=N VBR frames).
 *         May be partially/fully blank in MPEG and sizes/paddings aren't very consistent even in similar files (MA:LA's HANGOUT_CROWD_*).
 *   0x08: samples to discard in the beginning of this block? (MPEG/XMA2)
 *         When this is set, channel repeats XMA/MPEG super-frames from prev block. However discard doesn't seem to match
 *         repeated samples, and skip value may be very small too (ex. just 4 skip samples but repeats make 8416 samples).
 *         So maybe they just skip data like we do below and don't actually use this value.
 *   0x0c: samples in channel without discard? (for MPEG/XMA2 can vary between channels)
 *   (next fields only exists for MPEG)
 *   0x10: close to number of VBR frames but varies a bit?
 *   0x14: channel data size (not including padding between channels)
 * - seek table (entries for all channels, 1 per frame)
 *   0x00: start?
 *   0x04: end?
 * - padding up to data start
 */
static bool read_rage_aud_block(STREAMFILE* sf, rage_aud_block_info_t* bi) {
    read_s32_t read_s32 = bi->big_endian ? read_s32be : read_s32le;

    uint32_t channel_entry_size, seek_entry_size;
    uint32_t offset = bi->block_offset;
    int channels = bi->channels;

    /* read stupid block crap + derived info at once so hopefully it's a bit easier to understand */

    switch(bi->codec) {
        case 0x0000: /* XMA1 */
            channel_entry_size = 0x10;
            seek_entry_size = 0x08;
            break;
        case 0x0100: /* MPEG */
            channel_entry_size = 0x18;
            seek_entry_size = 0x08;
            break;
        default:
            VGM_LOG("RAGE AUD: unknown codec %x\n", bi->codec);
            return false;
    }

    /* base header */
    {
        offset += 0x18;
    }

    /* channel info table */
    for (int ch = 0; ch < bi->channels; ch++) {
        bi->blk[ch].start_entry        = read_s32(offset + 0x00, sf);
        bi->blk[ch].entries            = read_s32(offset + 0x04, sf);
        bi->blk[ch].channel_skip       = read_s32(offset + 0x08, sf);
        bi->blk[ch].channel_samples    = read_s32(offset + 0x0c, sf);
        if (bi->codec == 0x0100) { /* MPEG */
            bi->blk[ch].channel_size   = read_s32(offset + 0x14, sf);
        }

        offset += channel_entry_size;
    }

    /* seek table */
    for (int ch = 0; ch < channels; ch++) {
        offset += bi->blk[ch].entries * seek_entry_size;
    }

    /* derived info */
    for (int ch = 0; ch < channels; ch++) {
        bi->blk[ch].frame_size = RAGE_AUD_FRAME_SIZE;  /* XMA1 super-frame or MPEG chunk of N VBR frames */
        bi->blk[ch].chunk_size = bi->blk[ch].entries * bi->blk[ch].frame_size; /* full size between channels, may be padded */

        switch(bi->codec) {
            case 0x0000: /* XMA1 */
                bi->blk[ch].channel_size = bi->blk[ch].chunk_size; /* no  padding */
                break;
            default:
                break;
        }
    }

    /* detect block header size (aligned to 0x800) and adjust offset */
    {
        /* seek table size seems consistent for all blocks; last one defines less entries yet still writes
         * a table as big as prev blocks, repeating old values for unused entries, so final header size is consistent */
        if (!bi->header_size)
            bi->header_size = offset - bi->block_offset;
        offset = bi->block_offset + align_size_to_block(bi->header_size, RAGE_AUD_FRAME_SIZE);
    }

    /* set frame starts per channel */
    uint32_t header_chunk = offset - bi->block_offset;
    for (int ch = 0; ch < channels; ch++) {
        bi->blk[ch].chunk_start = header_chunk + bi->blk[ch].start_entry * bi->blk[ch].frame_size;

        /* unlike AWC there may be unknown padding between channels, so needs start_entry to calc offset */
        //bi->blk[ch].chunk_start = offset - bi->block_offset;
        //offset += bi->blk[ch].chunk_size;
    }

    /* beyond this is padding until chunk_start */

    return true;
}

/* Find data that repeats in the beginning of a new block at the end of last block.
 * When a new block starts there is some repeated data + channel_skip (for seeking + encoder delay?).
 * Detect it so decoder may ignore it. */
static uint32_t get_block_repeated_size(STREAMFILE* sf, rage_aud_block_info_t* bi, int channel) {

    if (bi->blk[channel].channel_skip == 0)
        return 0;

    switch(bi->codec) {
        case 0x0000: { /* XMA1 */
            /* when data repeats seems to clone the last super-frame */
            return bi->blk[channel].frame_size;
        }

#ifdef VGM_USE_MPEG
        case 0x0100: { /* MPEG */
            /* first super-frame will repeat N VBR old sub-frames, without crossing frame_size.
             * ex. repeated frames' size could be set to 0x774 (7 sub-frames) if adding 1 more would take >0x800.
             * After last sub-frame there may be padding up to frame_size (GTA4 only?). */
            uint8_t frame[RAGE_AUD_FRAME_SIZE];
            uint32_t offset = bi->block_offset + bi->blk[channel].chunk_start;

            read_streamfile(frame, offset, sizeof(frame), sf);

            /* read sub-frames until padding or end */
            int skip_size = 0x00;
            while (skip_size < sizeof(frame) - 0x04) {
                if (frame[skip_size] == 0x00) /* padding found */
                    return RAGE_AUD_FRAME_SIZE;

                mpeg_frame_info info = {0};
                uint32_t header = get_u32be(frame + skip_size);
                if (!mpeg_get_frame_info_h(header, &info)) /* ? */
                    return RAGE_AUD_FRAME_SIZE;

                if (skip_size + info.frame_size > sizeof(frame)) /* not a repeated frame */
                    return skip_size;
                skip_size += info.frame_size;
            }

            return skip_size; /* skip_size fills frame size */
        }
#endif
        default: 
            ;VGM_LOG("RAGE_AUD: found channel skip in codec %x\n", bi->codec); /* not seen */
            return 0;
    }
}

/* ************************************************************************* */

static void block_callback(STREAMFILE *sf, deblock_io_data* data) {
    int channel = data->cfg.track_number;
    rage_aud_block_info_t bi = {0};

    bi.big_endian = data->cfg.big_endian;
    bi.block_offset = data->physical_offset;
    bi.channels = data->cfg.track_count;
    bi.codec = data->cfg.track_type;
    bi.header_size = data->cfg.config;

    if (bi.block_offset >= get_streamfile_size(sf))
        return;

    if (!read_rage_aud_block(sf, &bi))
        return; //???
    data->cfg.config = bi.header_size; /* fixed for all blocks but calc'd on first one */

    uint32_t repeat_size = get_block_repeated_size(sf, &bi, channel);

    data->block_size = data->cfg.chunk_size;
    data->skip_size = bi.blk[channel].chunk_start + repeat_size;
    data->data_size = bi.blk[channel].channel_size - repeat_size;
}

/* deblocks RAGE_AUD blocks */
static STREAMFILE* setup_rage_aud_streamfile(STREAMFILE* sf, uint32_t stream_offset, uint32_t stream_size, uint32_t block_size, int channels, int channel, int codec, bool big_endian) {
    STREAMFILE* new_sf = NULL;
    deblock_config_t cfg = {0};

    if (channels > RAGE_AUD_MAX_MUSIC_CHANNELS || channel >= channels)
        return NULL;

    cfg.track_number = channel;
    cfg.track_count = channels;
    cfg.stream_start = stream_offset;
    cfg.stream_size = stream_size;
    cfg.chunk_size = block_size;
    cfg.track_type = codec;
    cfg.big_endian = big_endian;
    //cfg.physical_offset = stream_offset;
    //cfg.logical_size = rage_aud_io_size(sf, &cfg); /* force init */
    cfg.block_callback = block_callback;

    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_deblock_streamfile_f(new_sf, &cfg);
    //new_sf = open_buffer_streamfile_f(new_sf, 0);
    return new_sf;
}

#endif
