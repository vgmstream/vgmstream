#include "coding.h"


/* Decodes Argonaut's ASF ADPCM codec, used in some of their PC games.
 * Algorithm should be accurate (reverse engineered from asfcodec.adl DLL). */
void decode_asf(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    off_t frame_offset;
    int i, frames_in, sample_count = 0;
    size_t bytes_per_frame, samples_per_frame;
    uint8_t shift, mode;
    int32_t hist1 = stream->adpcm_history1_32;
    int32_t hist2 = stream->adpcm_history2_32;

    /* external interleave (fixed size), mono */
    bytes_per_frame = 0x11;
    samples_per_frame = (bytes_per_frame - 0x01) * 2;
    frames_in = first_sample / samples_per_frame;
    first_sample = first_sample % samples_per_frame;

    /* parse frame header */
    frame_offset = stream->offset + bytes_per_frame*frames_in;
    shift = ((uint8_t)read_8bit(frame_offset+0x00,stream->streamfile) >> 4) & 0xf;
    mode  = ((uint8_t)read_8bit(frame_offset+0x00,stream->streamfile) >> 0) & 0xf;

    /* decode nibbles */
    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        int32_t new_sample;
        uint8_t nibbles = (uint8_t)read_8bit(frame_offset+0x01 + i/2,stream->streamfile);

        new_sample = i&1 ? /* high nibble first */
                get_low_nibble_signed(nibbles):
                get_high_nibble_signed(nibbles);
        /* move sample to upper nibble, then shift + 2 (IOW: shift + 6) */
        new_sample = (new_sample << 4) << (shift + 2);

        /* mode is checked as a flag, so there are 2 modes only, but lower nibble
         * may have other values at last frame (ex 0x02/09), could be control flags (loop related?) */
        if (mode & 0x4) { /* ~filters: 2, -1  */
            new_sample = (new_sample + (hist1 << 7) - (hist2 << 6)) >> 6;
        }
        else { /* ~filters: 1, 0  */
            new_sample = (new_sample + (hist1 << 6)) >> 6;
        }

        //new_sample = clamp16(new_sample); /* must not */
        outbuf[sample_count] = (int16_t)new_sample;
        sample_count += channelspacing;

        hist2 = hist1;
        hist1 = new_sample;
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_history2_32 = hist2;
}
