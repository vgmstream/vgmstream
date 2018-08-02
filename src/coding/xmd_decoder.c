#include "coding.h"


/* Decodes Konami XMD from Xbox games.
 * Algorithm reverse engineered from SH4/CV:CoD's xbe (byte-accurate). */
void decode_xmd(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, size_t frame_size) {
    off_t frame_offset;
    int i, frames_in, sample_count = 0, samples_done = 0;
    size_t bytes_per_frame, samples_per_frame;
    int16_t hist1, hist2;
    uint16_t scale;

    /* external interleave (variable size), mono */
    bytes_per_frame = frame_size;
    samples_per_frame = 2 + (frame_size - 0x06) * 2;
    frames_in = first_sample / samples_per_frame;
    first_sample = first_sample % samples_per_frame;

    /* parse frame header */
    frame_offset = stream->offset + bytes_per_frame*frames_in;
    hist2 = read_16bitLE(frame_offset+0x00,stream->streamfile);
    hist1 = read_16bitLE(frame_offset+0x02,stream->streamfile);
    scale = (uint16_t)read_16bitLE(frame_offset+0x04,stream->streamfile); /* scale doesn't go too high though */


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
    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        int32_t new_sample;
        uint8_t nibbles = (uint8_t)read_8bit(frame_offset+0x06 + i/2,stream->streamfile);

        new_sample = i&1 ? /* low nibble first */
                get_high_nibble_signed(nibbles):
                 get_low_nibble_signed(nibbles);
        /* Coefs are based on XA's filter 2 (using those creates hissing in some songs though)
         * ex. 1.796875 * (1 << 14) = 0x7300, -0.8125 * (1 << 14) = -0x3400 */
        new_sample = (new_sample*(scale<<14) + (hist1*0x7298) - (hist2*0x3350)) >> 14;

        //new_sample = clamp16(new_sample); /* not needed */
        if (sample_count >= first_sample && samples_done < samples_to_do) {
            outbuf[samples_done * channelspacing] = (int16_t)new_sample;
            samples_done++;
        }
        sample_count++;

        hist2 = hist1;
        hist1 = new_sample;
    }

    //stream->adpcm_history1_32 = hist1;
    //stream->adpcm_history2_32 = hist2;
}
