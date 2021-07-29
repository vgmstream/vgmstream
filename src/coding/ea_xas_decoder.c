#include "coding.h"
#include "../util.h"

#if 0
/* known game code/platforms use float buffer and coefs, but some approximations around use this int math:
 * ...
 * coef1 = table[index + 0]
 * coef2 = table[index + 4]
 * sample = clamp16(((signed_nibble << (20 - shift)) + hist1 * coef1 + hist2 * coef2 + 128) >> 8); */
static const int EA_XA_TABLE[20] = {
    0,  240,  460,  392,
    0,    0, -208, -220,
    0,    1,    3,    4,
    7,    8,   10,   11,
    0,   -1,   -3,   -4
};
#endif

/* standard CD-XA's K0/K1 filter pairs */
static const float xa_coefs[16][2] = {
    { 0.0,       0.0      },
    { 0.9375,    0.0      },
    { 1.796875, -0.8125   },
    { 1.53125,  -0.859375 },
    /* only 4 pairs exist, assume 0s for bad indexes */
};

/* EA-XAS (XA Seekable) Version 1, evolution of EA-XA/XAS and cousin of MTA2. Reverse engineered from various .exes/.so
 *
 * Layout: blocks of 0x4c per channel (128 samples), divided into 4 headers + 4 vertical groups of 15 bytes.
 * Original code reads all headers first then processes all nibbles (for CPU cache/parallelism/SIMD optimizations).
 * To simplify, always decodes the block and discards unneeded samples, so doesn't use external hist. */
void decode_ea_xas_v1(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    uint8_t frame[0x4c] = {0};
    off_t frame_offset;
    int group, row, i, samples_done = 0, sample_count = 0;
    size_t bytes_per_frame, samples_per_frame;


    /* internal interleave */
    bytes_per_frame = 0x4c;
    samples_per_frame = 128;
    first_sample = first_sample % samples_per_frame;

    frame_offset = stream->offset + bytes_per_frame * channel;
    read_streamfile(frame, frame_offset, bytes_per_frame, stream->streamfile); /* ignore EOF errors */

    //todo: original code uses float sample buffer:
    //- header pcm-hist to float-hist:  hist * (1/32768)
    //- nibble to signed to float: (int32_t)(pnibble << 28) * SHIFT_MUL_LUT[shift_index]
    //  look-up table just simplifies ((nibble << 12 << 12) >> 12 + shift) * (1/32768)
    //  though maybe introduces rounding errors?
    //- coefs apply normally, though hists are already floats
    //- final float sample isn't clamped


    /* parse group headers */
    for (group = 0; group < 4; group++) {
        float coef1, coef2;
        int16_t hist1, hist2;
        uint8_t shift;
        uint32_t group_header = (uint32_t)get_32bitLE(frame + group*0x4); /* always LE */

        coef1 = xa_coefs[group_header & 0x0F][0];
        coef2 = xa_coefs[group_header & 0x0F][1];
        hist2 = (int16_t)((group_header >>  0) & 0xFFF0);
        hist1 = (int16_t)((group_header >> 16) & 0xFFF0);
        shift = (group_header >> 16) & 0x0F;

        /* write header samples (needed) */
        if (sample_count >= first_sample && samples_done < samples_to_do) {
            outbuf[samples_done * channelspacing] = hist2;
            samples_done++;
        }
        sample_count++;
        if (sample_count >= first_sample && samples_done < samples_to_do) {
            outbuf[samples_done * channelspacing] = hist1;
            samples_done++;
        }
        sample_count++;

        /* process nibbles per group */
        for (row = 0; row < 15; row++) {
            for (i = 0; i < 1*2; i++) {
                uint8_t nibbles = frame[4*4 + row*0x04 + group + i/2];
                int sample;

                sample = i&1 ? /* high nibble first */
                        (nibbles >> 0) & 0x0f :
                        (nibbles >> 4) & 0x0f;
                sample = (int16_t)(sample << 12) >> shift; /* 16b sign extend + scale */
                sample = sample + hist1 * coef1 + hist2 * coef2;
                sample = clamp16(sample);

                if (sample_count >= first_sample && samples_done < samples_to_do) {
                    outbuf[samples_done * channelspacing] = sample;
                    samples_done++;
                }
                sample_count++;

                hist2 = hist1;
                hist1 = sample;
            }
        }
    }


    /* internal interleave (interleaved channels, but manually advances to co-exist with ea blocks) */
    if (first_sample + samples_done == samples_per_frame)  {
        stream->offset += bytes_per_frame * channelspacing;
    }
}


/* EA-XAS v0 (xas0), without complex layouts and closer to EA-XA. Somewhat based on daemon1's decoder. */
void decode_ea_xas_v0(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    uint8_t frame[0x13] = {0};
    off_t frame_offset;
    int i, frames_in, samples_done = 0, sample_count = 0;
    size_t bytes_per_frame, samples_per_frame;


    /* external interleave (fixed size), mono */
    bytes_per_frame = 0x02 + 0x02 + 0x0f;
    samples_per_frame = 1 + 1 + 0x0f*2;
    frames_in = first_sample / samples_per_frame;
    first_sample = first_sample % samples_per_frame;

    frame_offset = stream->offset + bytes_per_frame * frames_in;
    read_streamfile(frame, frame_offset, bytes_per_frame, stream->streamfile); /* ignore EOF errors */

    //todo see above

    /* process frame */
    {
        float coef1, coef2;
        int16_t hist1, hist2;
        uint8_t shift;
        uint32_t frame_header = (uint32_t)get_32bitLE(frame); /* always LE */

        coef1 = xa_coefs[frame_header & 0x0F][0];
        coef2 = xa_coefs[frame_header & 0x0F][1];
        hist2 = (int16_t)((frame_header >>  0) & 0xFFF0);
        hist1 = (int16_t)((frame_header >> 16) & 0xFFF0);
        shift = (frame_header >> 16) & 0x0F;

        /* write header samples (needed) */
        if (sample_count >= first_sample && samples_done < samples_to_do) {
            outbuf[samples_done * channelspacing] = hist2;
            samples_done++;
        }
        sample_count++;
        if (sample_count >= first_sample && samples_done < samples_to_do) {
            outbuf[samples_done * channelspacing] = hist1;
            samples_done++;
        }
        sample_count++;

        /* process nibbles */
        for (i = 0; i < 0x0f*2; i++) {
            uint8_t nibbles = frame[0x02 + 0x02 + i/2];
            int sample;

            sample = i&1 ? /* high nibble first */
                    (nibbles >> 0) & 0x0f :
                    (nibbles >> 4) & 0x0f;
            sample = (int16_t)(sample << 12) >> shift; /* 16b sign extend + scale */
            sample = sample + hist1 * coef1 + hist2 * coef2;
            sample = clamp16(sample);

            if (sample_count >= first_sample && samples_done < samples_to_do) {
                outbuf[samples_done * channelspacing] = sample;
                samples_done++;
            }
            sample_count++;

            hist2 = hist1;
            hist1 = sample;
        }
    }
}
