#include "coding.h"
#include "../util.h"

/* MTA2 decoder based on:
 * - MGS Developer Wiki: https://www.mgsdevwiki.com/wiki/index.php/MTA2_(Codec) [codec by daemon1]
 * - Solid4 tools: https://github.com/GHzGangster/Drebin
 *
 * MTA2 layout:
 * - data is divided into N tracks of 0x10 header + 0x90 frame per track channel, forming N streams
 *   ex: 8ch: track0 4ch + track1 4ch + track0 4ch + track1 4ch ...; or 2ch = 1ch track0 + 1ch track1
 *   * up to 16 possible tracks, but max seen is 3 (ex. track0=sneaking, track1=action, track2=ambience)
 * - each ch frame is divided into 4 headers + 4 vertical groups with nibbles (0x4*4 + 0x20*4)
 *   ex. group1 is 0x04(4) + 0x14(4) + 0x24(4) + 0x34(4) ... (vertically maybe for paralelism?)
 *
 * Due to this vertical layout and multiple hist/indexes, it decodes everything in a block between calls
 * but discards unwanted data, instead of trying to skip to the target nibble. Meaning no need to save hist, and
 * expects samples_to_do to be block_samples at most (could be simplified, I guess).
 */

/* coefs table (extended XA filters) */
static const int mta2_coefs1[8] = {
    0,  240,  460,  392,  488,  460,  460,  240 
};
static const int mta2_coefs2[8] = {
    0,    0, -208, -220, -240, -240, -220, -104 
};
/* shift table */
static const int mta2_shifts[32] = {
       256,    335,    438,    573,    749,    979,   1281,    1675, 
      2190,   2864,   3746,   4898,   6406,   8377,  10955,   14327, 
     18736,  24503,  32043,  41905,  54802,  71668,  93724,  122568,
    160290, 209620, 274133, 358500, 468831, 613119, 801811, 1048576
};

/* expands nibble */
static short mta2_expand_nibble(int nibble, short hist1, short hist2, int coef_index, int shift_index) {
    int output;
    if (nibble > 7) /* sign extend */
        nibble = nibble - 16;

    output = (hist1 * mta2_coefs1[coef_index] + hist2 * mta2_coefs2[coef_index] + (nibble * mta2_shifts[shift_index]) + 128) >> 8;
    output = clamp16(output);
    return (short)output;
}

/* decodes a block for a channel */
void decode_mta2(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    int samples_done = 0, sample_count = 0, channel_block_samples, channel_first_sample, frame_size = 0;
    int i, group, row, col;
    int track_channels = 0, track_channel;


    /* track skip */
    do {
        int num_track = 0, channel_layout;

        /* parse track header (0x10) and skip tracks that our current channel doesn't belong to */
        num_track      =    read_8bit(stream->offset+0x00,stream->streamfile); /* 0=first */
        /* 0x01(3): num_frame (0=first) */
        /* 0x04(1): 0? */
        channel_layout =    read_8bit(stream->offset+0x05,stream->streamfile); /* bitmask, see mta2.c */
        frame_size     = read_16bitBE(stream->offset+0x06,stream->streamfile); /* not including this header */
        /* 0x08(8): null */


        VGM_ASSERT(frame_size == 0, "MTA2: empty frame at %x\n", (uint32_t)stream->offset);
        /* frame_size 0 means silent/empty frame (rarely found near EOF for one track but not others)
         * negative track only happens for truncated files (EOF) */
        if (frame_size == 0 || num_track < 0) {
            for (i = 0; i < samples_to_do; i++)
                outbuf[i * channelspacing] = 0;
            stream->offset += 0x10;
            return;
        }

        track_channels = 0;
        for (i = 0; i < 8; i++) {
            if ((channel_layout >> i) & 0x01)
                track_channels++;
        }

        if (track_channels == 0) { /* bad data, avoid div by 0 */
            VGM_LOG("track_channels 0 at %x\n", (uint32_t)stream->offset);
            return;
        }

        /* assumes tracks channels are divided evenly in all tracks (ex. not 2ch + 1ch + 1ch) */
        if (channel / track_channels == num_track)
            break; /* channel belongs to this track */

        /* keep looping for our track */
        stream->offset += 0x10 + frame_size;
    }
    while (1);

    track_channel = channel % track_channels;
    channel_block_samples = (0x80*2);
    channel_first_sample = first_sample % (0x80*2);


    /* parse channel frame (header 0x04*4 + data 0x20*4) */
    for (group = 0; group < 4; group++) {
        short hist2, hist1, coefs, shift, output;
        int group_header = read_32bitBE(stream->offset + 0x10 + track_channel*0x90 + group*0x4, stream->streamfile);
        hist2 = (short) ((group_header >> 16) & 0xfff0); /* upper 16b discarding 4b */
        hist1 = (short) ((group_header >>  4) & 0xfff0); /* lower 16b discarding 4b */
        coefs = (group_header >> 5) & 0x7; /* mid 3b */
        shift = group_header & 0x1f; /* lower 5b */

        /* write header samples (skips the last 2 group nibbles), like Drebin's decoder
         * last 2 nibbles and next 2 header hist should match though */
        if (sample_count >= channel_first_sample && samples_done < samples_to_do) {
            outbuf[samples_done * channelspacing] = hist2;
            samples_done++;
        }
        sample_count++;
        if (sample_count >= channel_first_sample && samples_done < samples_to_do) {
            outbuf[samples_done * channelspacing] = hist1;
            samples_done++;
        }
        sample_count++;

        for (row = 0; row < 8; row++) {
            for (col = 0; col < 4*2; col++) {
                uint8_t nibbles = read_8bit(stream->offset + 0x10 + 0x10 + track_channel*0x90 + group*0x4 + row*0x10 + col/2, stream->streamfile);
                int nibble_shift = (!(col&1) ? 4 : 0); /* upper first */
                output = mta2_expand_nibble((nibbles >> nibble_shift) & 0xf, hist1, hist2, coefs, shift);

                /* ignore last 2 nibbles (uses first 2 header samples) */
                if (row < 7 || col < 3*2) {
                    if (sample_count >= channel_first_sample && samples_done < samples_to_do) {
                        outbuf[samples_done * channelspacing] = output;
                        samples_done++;
                    }
                    sample_count++;
                }

                hist2 = hist1;
                hist1 = output;
            }
        }
    }


    /* block fully done */
    if (channel_first_sample + samples_done == channel_block_samples)  {
        stream->offset += 0x10 + frame_size;
    }
}
