#include "coding.h"


/* Decodes Argonaut's ASF ADPCM codec, used in some of their PC games.
 * Reverse engineered from asfcodec.adl DLL. */
void decode_asf(VGMSTREAMCHANNEL *stream, sample_t *outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    uint8_t frame[0x11] = {0};
    off_t frame_offset;
    int i, frames_in, sample_count = 0;
    size_t bytes_per_frame, samples_per_frame;
    int shift, mode;
    int32_t hist1 = stream->adpcm_history1_32;
    int32_t hist2 = stream->adpcm_history2_32;


    /* external interleave (fixed size), mono */
    bytes_per_frame = 0x11;
    samples_per_frame = (bytes_per_frame - 0x01) * 2;
    frames_in = first_sample / samples_per_frame;
    //first_sample = first_sample % samples_per_frame; /* for flat layout */

    /* parse frame header */
    frame_offset = stream->offset + bytes_per_frame*frames_in;
    read_streamfile(frame, frame_offset, bytes_per_frame, stream->streamfile); /* ignore EOF errors */
    shift = (frame[0x00] >> 4) & 0xf;
    mode  = (frame[0x00] >> 0) & 0xf;

    /* decode nibbles */
    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        uint8_t nibbles = frame[0x01 + i/2];
        int32_t sample;

        sample = i&1 ? /* high nibble first */
                get_low_nibble_signed(nibbles):
                get_high_nibble_signed(nibbles);
        sample = (sample << 4) << (shift + 2); /* move sample to upper nibble, then shift + 2 (IOW: shift + 6) */

        /* mode is checked as a flag, so there are 2 modes only, but lower nibble
         * may have other values at last frame (ex 0x02/09), could be control flags (loop related?) */
        if (mode & 0x4) { /* ~filters: 2, -1  */
            sample = (sample + (hist1 << 7) - (hist2 << 6)) >> 6;
        }
        else { /* ~filters: 1, 0  */
            sample = (sample + (hist1 << 6)) >> 6;
        }

        outbuf[sample_count] = (int16_t)sample; /* must not clamp */
        sample_count += channelspacing;

        hist2 = hist1;
        hist1 = sample;
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_history2_32 = hist2;
}

int32_t asf_bytes_to_samples(size_t bytes, int channels) {
    if (channels <= 0) return 0;
    return bytes / channels / 0x11 * 32;
}
