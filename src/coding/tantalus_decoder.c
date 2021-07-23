#include "coding.h"


/* Decodes Tantalus TADC ADPCM codec, used in Saturn games.
 * Guessed based on other XA-style codecs values. */
void decode_tantalus(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    uint8_t frame[0x10] = {0};
    off_t frame_offset;
    int i, frames_in, sample_count = 0;
    size_t bytes_per_frame, samples_per_frame;
    int shift, filter, coef1, coef2;
    int32_t hist1 = stream->adpcm_history1_32;
    int32_t hist2 = stream->adpcm_history2_32;


    /* external interleave (fixed size), mono */
    bytes_per_frame = 0x10;
    samples_per_frame = (bytes_per_frame - 0x01) * 2;
    frames_in = first_sample / samples_per_frame;
    //first_sample = first_sample % samples_per_frame; /* for flat layout */

    /* parse frame header */
    frame_offset = stream->offset + bytes_per_frame*frames_in;
    read_streamfile(frame, frame_offset, bytes_per_frame, stream->streamfile); /* ignore EOF errors */
    filter = (frame[0x00] >> 4) & 0xf; /* 0 in tested files */
    shift = (frame[0x00] >> 0) & 0xf;
    if (filter != 0) {
        VGM_LOG_ONCE("TANTALUS: unknown filter\n");
        coef1 = 64;
        coef2 = 64; /* will sound horrid and hopefully reported */
    }
    else {
        coef1 = 64;
        coef2 = 0;
    }


    /* decode nibbles */
    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        uint8_t nibbles = frame[0x01 + i/2];
        int32_t sample;

        sample = i&1 ? /* low nibble first */
                get_high_nibble_signed(nibbles) :
                get_low_nibble_signed(nibbles);
        sample = sample << (shift + 6);
        sample = (sample + (hist1 * coef1) + (hist2 * coef2)) >> 6;

        outbuf[sample_count] = clamp16(sample);
        sample_count += channelspacing;

        hist2 = hist1;
        hist1 = sample;
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_history2_32 = hist2;
}

int32_t tantalus_bytes_to_samples(size_t bytes, int channels) {
    if (channels <= 0) return 0;
    return bytes / channels / 0x10 * 30;
}
