#include "coding.h"


/* Decodes Konami XMD from Xbox games.
 * Algorithm reverse engineered from SH4/CV:CoD's xbe (byte-accurate). */
void decode_xmd(VGMSTREAMCHANNEL *stream, sample_t *outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, size_t frame_size) {
    uint8_t frame[0x15] = {0};
    off_t frame_offset;
    int frames_in, sample_count = 0, samples_done = 0;
    size_t bytes_per_frame, samples_per_frame;


    /* external interleave (variable size), mono */
    bytes_per_frame = frame_size;
    samples_per_frame = 2 + (frame_size - 0x06) * 2;
    frames_in = first_sample / samples_per_frame;
    //first_sample = first_sample % samples_per_frame; /* for flat layout */

    /* parse frame header */
    frame_offset = stream->offset + bytes_per_frame * frames_in;
    read_streamfile(frame, frame_offset, bytes_per_frame, stream->streamfile); /* ignore EOF errors */

    int16_t hist2  = get_s16le(frame + 0x00);
    int16_t hist1  = get_s16le(frame + 0x02);
    uint16_t scale = get_u16le(frame + 0x04); // scale doesn't go too high though

    // Coefs are based on XA's filter 2 (using those creates hissing in some songs though)
    // ex. 1.796875 * (1 << 14) = 0x7300, -0.8125 * (1 << 14) = -0x3400
    int coef1 = 0x7298;
    int coef2 = 0x3350;

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

   
    /* decode nibbles */
    for (int i = first_sample; i < first_sample + samples_to_do; i++) {
        uint8_t nibbles = frame[0x06 + i/2];
        int32_t sample;

        sample = get_nibble_signed(nibbles, i&1); // low nibble first

        sample = (sample * (scale << 14) + (hist1 * coef1) - (hist2 * coef2)) >> 14;
        //sample = clamp16(sample); // not needed

        if (sample_count >= first_sample && samples_done < samples_to_do) {
            outbuf[samples_done * channelspacing] = (int16_t)sample;
            samples_done++;
        }
        sample_count++;

        hist2 = hist1;
        hist1 = sample;
    }

    //stream->adpcm_history1_32 = hist1;
    //stream->adpcm_history2_32 = hist2;
}
