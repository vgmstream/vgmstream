#include "coding.h"
#include "../util.h"

/* MTA2 (EA XAS variant?) decoder based on:
 * - MGS Developer Wiki: https://www.mgsdevwiki.com/wiki/index.php/MTA2_(Codec) [codec by daemon1]
 * - Solid4 tools: https://github.com/GHzGangster/Drebin
 *
 * MTA2 layout:
 * - data is divided into N tracks of 0x10 header + 0x90 frame per track channel, forming N streams
 *   ex: 8ch: track0 4ch + track1 4ch + track0 4ch + track1 4ch ...; or 2ch = 1ch track0 + 1ch track1
 *   * up to 16 possible tracks, but max seen is 3 (ex. track0=sneaking, track1=action, track2=ambience)
 * - each ch frame is divided into 4 headers + 4 vertical groups with nibbles (0x4*4 + 0x20*4)
 *   ex. group1 is 0x04(4) + 0x14(4) + 0x24(4) + 0x34(4) ... (vertically maybe for paralelism?)
 * - in case of "macroblock" layout, there are also headers before N tracks (like other MGS games)
 *
 * Due to this vertical layout and multiple hist/indexes, it decodes everything in a block between calls
 * but discards unwanted data, instead of trying to skip to the target nibble. Meaning no need to save hist, and
 * expects samples_to_do to be block_samples at most (could be simplified, I guess).
 *
 * Because of how the macroblock/track and stream's offset per channel work, they are supported by
 * autodetecting and skipping when needed (ideally should keep a special layout/count, but this is simpler).
 */

static const int c1[8] = { /* mod table 1 */
    0,  240,  460,  392,  488,  460,  460,  240 
};
static const int c2[8] = { /* mod table 2 */
    0,    0, -208, -220, -240, -240, -220, -104 
};
static const int c3[32] = { /* shift table */
       256,    335,    438,    573,    749,    979,   1281,    1675, 
      2190,   2864,   3746,   4898,   6406,   8377,  10955,   14327, 
     18736,  24503,  32043,  41905,  54802,  71668,  93724,  122568,
    160290, 209620, 274133, 358500, 468831, 613119, 801811, 1048576
};

/* expands nibble */
static short calculate_output(int nibble, short smp1, short smp2, int mod, int sh) {
    int output;
    if (nibble > 7) /* sign extend */
        nibble = nibble - 16;

    output = (smp1 * c1[mod] + smp2 * c2[mod] + (nibble * c3[sh]) + 128) >> 8;
    output = clamp16(output);
    return (short)output;
}


/* autodetect and skip "macroblocks" */
static void mta2_block_update(VGMSTREAMCHANNEL * stream) {
    int block_type, block_size, block_tracks, repeat = 1;

    /* may need to skip N empty blocks */
    do {
        block_type   = read_32bitBE(stream->offset + 0x00, stream->streamfile);
        block_size   = read_32bitBE(stream->offset + 0x04, stream->streamfile); /* including this header */
        /* 0x08: always null */
        block_tracks = read_32bitBE(stream->offset + 0x0c, stream->streamfile); /* total tracks of variable size (can be 0) */

        /* 0x10001: music, 0x20001: sfx?, 0xf0: loop control (goes at the end) */
        if (block_type != 0x00010001 && block_type != 0x00020001 && block_type != 0x000000F0)
            return; /* not a block */

        /* frame=010001+00/etc can be mistaken as block_type, do extra checks */
        {
            int i, track_channels = 0;
            uint16_t channel_layout = (block_size >> 16);
            uint16_t track_size = (block_size & 0xFFFF);

            /* has chanel layout == may be a track */
            if (channel_layout > 0 && channel_layout <= 0xFF) {
                for (i = 0; i < 8; i++) {
                    if ((channel_layout >> i) & 0x01)
                        track_channels++;
                }
                if (track_channels*0x90 == track_size)
                    return;
            }
        }

        if (block_size <= 0 || block_tracks < 0) {  /* nonsense block (maybe at EOF) */
            VGM_LOG("MTA2: bad block @ %08lx\n", stream->offset);
            stream->offset += 0x10;
            repeat = 0;
        }
        else if (block_tracks == 0) {  /* empty block (common), keep repeating */
            stream->offset += block_size;
        }
        else {  /* normal block, position into next track header */
            stream->offset += 0x10;
            repeat = 0;
        }
    } while (repeat);
}

/* decodes a block for a channel, skipping macroblocks/tracks if needed */
void decode_mta2(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    int samples_done = 0, sample_count = 0, channel_block_samples, channel_first_sample, frame_size = 0;
    int i, group, row, col;
    int track_channels = 0, track_channel;


    /* block/track skip */
    do {
        int num_track = 0, channel_layout;
        /* autodetect and skip macroblock header */
        mta2_block_update(stream);

        /* parse track header (0x10) and skip tracks that our current channel doesn't belong to */
        num_track      =    read_8bit(stream->offset+0x00,stream->streamfile); /* 0=first */
        /* 0x01(3): num_frame (0=first), 0x04(1): 0? */
        channel_layout =    read_8bit(stream->offset+0x05,stream->streamfile); /* bitmask, see mta2.c */
        frame_size     = read_16bitBE(stream->offset+0x06,stream->streamfile); /* not including this header */
        /* 0x08(8): null */

        /* EOF: 0-fill buffer (or, as track_channels = 0 > divs by 0) */
        if (num_track < 0) {
            for (i = 0; i < samples_to_do; i++)
                outbuf[i * channelspacing] = 0;
            return;
        }


        track_channels = 0;
        for (i = 0; i < 8; i++) {
            if ((channel_layout >> i) & 0x01)
                track_channels++;
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
        short smp2, smp1, mod, sh, output;
        int group_header = read_32bitBE(stream->offset + 0x10 + track_channel*0x90 + group*0x4, stream->streamfile);
        smp2 = (short) ((group_header >> 16) & 0xfff0); /* upper 16b discarding 4b */
        smp1 = (short) ((group_header >> 4) & 0xfff0); /* lower 16b discarding 4b */
        mod  = (group_header >> 5) & 0x7; /* mid 3b */
        sh   = group_header & 0x1f; /* lower 5b */

        /* write header samples (skips the last 2 group nibbles), like Drebin's decoder
         * last 2 nibbles and next 2 header hist should match though */
        if (sample_count >= channel_first_sample && samples_done < samples_to_do) {
            outbuf[samples_done * channelspacing] = smp2;
            samples_done++;
        }
        sample_count++;
        if (sample_count >= channel_first_sample && samples_done < samples_to_do) {
            outbuf[samples_done * channelspacing] = smp1;
            samples_done++;
        }
        sample_count++;

        for (row = 0; row < 8; row++) {
            for (col = 0; col < 4*2; col++) {
                uint8_t nibbles = read_8bit(stream->offset + 0x10 + 0x10 + track_channel*0x90 + group*0x4 + row*0x10 + col/2, stream->streamfile);
                int nibble_shift = (!(col&1) ? 4 : 0); /* upper first */
                output = calculate_output((nibbles >> nibble_shift) & 0xf, smp1, smp2, mod, sh);

                /* ignore last 2 nibbles (uses first 2 header samples) */
                if (row < 7 || col < 3*2) {
                    if (sample_count >= channel_first_sample && samples_done < samples_to_do) {
                        outbuf[samples_done * channelspacing] = output;
                        samples_done++;
                    }
                    sample_count++;
                }

                smp2 = smp1;
                smp1 = output;
            }
        }
    }


    /* block fully done */
    if (channel_first_sample + samples_done == channel_block_samples)  {
        stream->offset += 0x10 + frame_size;
    }
}
