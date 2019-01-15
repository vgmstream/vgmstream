#include "coding.h"
#include "../util.h"

static const int EA_XA_TABLE[20] = {
    0,  240,  460,  392,
    0,    0, -208, -220,
    0,    1,    3,    4,
    7,    8,   10,   11,
    0,   -1,   -3,   -4
};

/* EA-XAS v1, evolution of EA-XA/XAS and cousin of MTA2. From FFmpeg (general info) + MTA2 (layout) + EA-XA (decoding)
 *
 * Layout: blocks of 0x4c per channel (128 samples), divided into 4 headers + 4 vertical groups of 15 bytes (for parallelism?).
 * To simplify, always decodes the block and discards unneeded samples, so doesn't use external hist. */
void decode_ea_xas_v1(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    int group, row, i;
    int samples_done = 0, sample_count = 0;


    /* internal interleave */
    int block_samples = 128;
    first_sample = first_sample % block_samples;


    /* process groups */
    for (group = 0; group < 4; group++) {
        int coef1, coef2;
        int16_t hist1, hist2;
        uint8_t shift;
        uint32_t group_header = (uint32_t)read_32bitLE(stream->offset + channel*0x4c + group*0x4, stream->streamfile); /* always LE */

        coef1 = EA_XA_TABLE[(uint8_t)(group_header & 0x0F) + 0];
        coef2 = EA_XA_TABLE[(uint8_t)(group_header & 0x0F) + 4];
        hist2 = (int16_t)(group_header & 0xFFF0);
        hist1 = (int16_t)((group_header >> 16) & 0xFFF0);
        shift = 20 - ((group_header >> 16) & 0x0F);

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
                uint8_t sample_byte = (uint8_t)read_8bit(stream->offset + channel*0x4c + 4*4 + row*0x04 + group + i/2, stream->streamfile);
                int sample;

                sample = get_nibble_signed(sample_byte, !(i&1)); /* upper first */
                sample = sample << shift;
                sample = (sample + hist1 * coef1 + hist2 * coef2 + 128) >> 8;
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
    if (first_sample + samples_done == block_samples)  {
        stream->offset += 0x4c * channelspacing;
    }
}


/* EA-XAS v0, without complex layouts and closer to EA-XA. Somewhat based on daemon1's decoder */
void decode_ea_xas_v0(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    off_t frame_offset;
    int i;
    int block_samples, frames_in, samples_done = 0, sample_count = 0;

    /* external interleave (fixed size), mono */
    block_samples = 32;
    frames_in = first_sample / block_samples;
    first_sample = first_sample % block_samples;

    frame_offset = stream->offset + (0x0f+0x02+0x02)*frames_in;

    /* process frames */
    {
        int coef1, coef2;
        int16_t hist1, hist2;
        uint8_t shift;
        uint32_t frame_header = (uint32_t)read_32bitLE(frame_offset, stream->streamfile); /* always LE */

        coef1 = EA_XA_TABLE[(uint8_t)(frame_header & 0x0F) + 0];
        coef2 = EA_XA_TABLE[(uint8_t)(frame_header & 0x0F) + 4];
        hist2 = (int16_t)(frame_header & 0xFFF0);
        hist1 = (int16_t)((frame_header >> 16) & 0xFFF0);
        shift = 20 - ((frame_header >> 16) & 0x0F);

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
            uint8_t sample_byte = (uint8_t)read_8bit(frame_offset + 0x02 + 0x02 + i/2, stream->streamfile);
            int sample;

            sample = get_nibble_signed(sample_byte, !(i&1)); /* upper first */
            sample = sample << shift;
            sample = (sample + hist1 * coef1 + hist2 * coef2 + 128) >> 8;
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
