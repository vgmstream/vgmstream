#include "coding.h"


/* Decodes Tantalus TADC ADPCM codec, used in Saturn games.
 * Reverse engineered from the exe (@ 0x06086d2 w/ 0x06010000 base address) */
void decode_tantalus(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    uint8_t frame[0x10] = {0};
    off_t frame_offset;
    int frames_in, sample_count = 0;
    size_t bytes_per_frame, samples_per_frame;

    int32_t hist1 = stream->adpcm_history1_32;


    /* external interleave (fixed size), mono */
    bytes_per_frame = 0x10;
    samples_per_frame = (bytes_per_frame - 0x01) * 2;
    frames_in = first_sample / samples_per_frame;
    //first_sample = first_sample % samples_per_frame; /* for flat layout */

    /* parse frame header */
    frame_offset = stream->offset + bytes_per_frame*frames_in;
    read_streamfile(frame, frame_offset, bytes_per_frame, stream->streamfile); /* ignore EOF errors */

    // there is no XA filter select code but upper 4 bits seem to be always 0
    int shift = (frame[0x00] >> 0);


    /* decode nibbles */
    for (int i = first_sample; i < first_sample + samples_to_do; i++) {
        uint8_t nibbles = frame[0x01 + i/2];
        int32_t sample;

        int8_t code = i&1 ? /* low nibble first */
                get_high_nibble_signed(nibbles) :
                get_low_nibble_signed(nibbles);

        // calc is done via [code][shift] = delta LUT in OG code (perhaps because SH2 can only do 1/2/4 shifts)
        // basically equivalent to an XA codec with coef1 = 1.0, coef2 = 0.0
        sample = hist1 + (code << shift);

        outbuf[sample_count] = clamp16(sample);
        sample_count += channelspacing;

        hist1 = sample;
    }

    stream->adpcm_history1_32 = hist1;
}

int32_t tantalus_bytes_to_samples(size_t bytes, int channels) {
    if (channels <= 0) return 0;
    return bytes / channels / 0x10 * 30;
}
