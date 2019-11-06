#include "coding.h"
#include "../util.h"

/* MTA2 decoder based on:
 * - MGS Developer Wiki: https://www.mgsdevwiki.com/wiki/index.php/MTA2_(Codec) [codec by daemon1]
 * - Solid4 tools: https://github.com/GHzGangster/Drebin
 * (PS3 probably uses floats, so this may not be 100% accurate)
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

/* tweaked XA/PSX coefs << 8 */
static const int16_t mta2_coefs[8][2] = {
    {   0,    0 },
    { 240,    0 },
    { 460, -208 },
    { 392, -220 },
    { 488, -240 },
    { 460, -240 },
    { 460, -220 },
    { 240, -104 }
};

static const int mta2_scales[32] = {
       256,    335,    438,    573,    749,    979,   1281,    1675, 
      2190,   2864,   3746,   4898,   6406,   8377,  10955,   14327, 
     18736,  24503,  32043,  41905,  54802,  71668,  93724,  122568,
    160290, 209620, 274133, 358500, 468831, 613119, 801811, 1048576
};

/* decodes a block for a channel */
void decode_mta2(VGMSTREAMCHANNEL *stream, sample_t *outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    uint8_t frame[0x10 + 0x90*8] = {0};
    int samples_done = 0, sample_count = 0, channel_block_samples, channel_first_sample, frame_size = 0;
    int i, group, row, col;
    int track_channels = 0, track_channel;


    /* track skip */
    do {
        int num_track = 0, channel_layout;

        /* parse track header (0x10) and skip tracks that our current channel doesn't belong to */
        read_streamfile(frame, stream->offset, 0x10, stream->streamfile); /* ignore EOF errors */
        num_track      = get_u8   (frame + 0x00); /* 0=first */
        /* 0x01(3): num_frame (0=first) */
        /* 0x04(1): 0? */
        channel_layout = get_u8   (frame + 0x05); /* bitmask, see mta2.c */
        frame_size     = get_u16be(frame + 0x06); /* not including this header */
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
        for (i = 0; i < 8; i++) { /* max 8ch */
            if ((channel_layout >> i) & 0x01)
                track_channels++;
        }

        if (track_channels == 0) { /* bad data, avoid div by 0 */
            VGM_LOG("MTA2: track_channels 0 at %x\n", (uint32_t)stream->offset);
            return;
        }

        /* assumes tracks channels are divided evenly in all tracks (ex. not 2ch + 1ch + 1ch) */
        if (channel / track_channels == num_track)
            break; /* channel belongs to this track */

        /* keep looping for our track */
        stream->offset += 0x10 + frame_size;
    }
    while (1);

    /* parse stuff */
    read_streamfile(frame + 0x10, stream->offset + 0x10, frame_size, stream->streamfile); /* ignore EOF errors */
    track_channel = channel % track_channels;
    channel_block_samples = (0x80*2);
    channel_first_sample = first_sample % (0x80*2);

    /* parse channel frame (header 0x04*4 + data 0x20*4) */
    for (group = 0; group < 4; group++) {
        short hist2, hist1, coefs, scale;
        uint32_t group_header = get_u32be(frame + 0x10 + track_channel*0x90 + group*0x4);
        hist2 = (short) ((group_header >> 16) & 0xfff0); /* upper 16b discarding 4b */
        hist1 = (short) ((group_header >>  4) & 0xfff0); /* lower 16b discarding 4b */
        coefs = (group_header >> 5) & 0x7; /* mid 3b */
        scale = group_header & 0x1f; /* lower 5b */

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

        /* decode nibbles */
        for (row = 0; row < 8; row++) {
            int pos = 0x10 + track_channel*0x90 + 0x10 + group*0x4 + row*0x10;
            for (col = 0; col < 4*2; col++) {
                uint8_t nibbles = frame[pos + col/2];
                int32_t sample;

                sample = col&1 ? /* high nibble first */
                        get_low_nibble_signed(nibbles) :
                        get_high_nibble_signed(nibbles);
                sample = sample * mta2_scales[scale];
                sample = (sample + hist1 * mta2_coefs[coefs][0] + hist2 * mta2_coefs[coefs][1] + 128) >> 8;
                sample = clamp16(sample);

                /* ignore last 2 nibbles (uses first 2 header samples) */
                if (row < 7 || col < 3*2) {
                    if (sample_count >= channel_first_sample && samples_done < samples_to_do) {
                        outbuf[samples_done * channelspacing] = sample;
                        samples_done++;
                    }
                    sample_count++;
                }

                hist2 = hist1;
                hist1 = sample;
            }
        }
    }


    /* block fully done */
    if (channel_first_sample + samples_done == channel_block_samples)  {
        stream->offset += 0x10 + frame_size;
    }
}
