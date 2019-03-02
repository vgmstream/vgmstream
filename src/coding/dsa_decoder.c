#include "coding.h"


static const int dsa_coefs[16] = {
        0x0,     0x1999,  0x3333,  0x4CCC,
        0x6666,  0x8000,  0x9999,  0xB333,
        0xCCCC,  0xE666,  0x10000, 0x11999,
        0x13333, 0x18000, 0x1CCCC, 0x21999
};

/* Decodes Ocean DSA ADPCM codec from Last Rites (PC).
 * Reverse engineered from daemon1's reverse engineering. */
void decode_dsa(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    off_t frame_offset;
    int i, frames_in, sample_count = 0;
    size_t bytes_per_frame, samples_per_frame;
    uint8_t header;
    int shift, filter;
    int32_t hist1 = stream->adpcm_history1_32;

    /* external interleave (fixed size), mono */
    bytes_per_frame = 0x08;
    samples_per_frame = (bytes_per_frame - 0x01) * 2;
    frames_in = first_sample / samples_per_frame;
    first_sample = first_sample % samples_per_frame;

    /* parse frame header */
    frame_offset = stream->offset + bytes_per_frame*frames_in;
    header = (uint8_t)read_8bit(frame_offset+0x00,stream->streamfile);
    shift  = 0x0c - ((header >> 4) & 0xf);
    filter = dsa_coefs[header & 0xf];

    /* decode nibbles */
    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        int32_t sample;
        uint8_t nibbles = (uint8_t)read_8bit(frame_offset+0x01 + i/2,stream->streamfile);

        sample = i&1 ? /* high nibble first */
                (nibbles >> 0) & 0xf :
                (nibbles >> 4) & 0xf;

        sample = ((int16_t)(sample << 0xC) >> shift); /* 16b sign extend + scale */
        sample = sample + ((hist1 * filter) >> 0x10);

        outbuf[sample_count] = (sample_t)(sample << 2);
        sample_count += channelspacing;

        hist1 = sample;
    }

    stream->adpcm_history1_32 = hist1;
}
