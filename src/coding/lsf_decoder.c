#include "coding.h"
#include "../util.h"


/* tweaked XA/PSX coefs << 6 */
static const short lsf_coefs[16][2] = {
    { 115, -52 },
    {   0,   0 },
    {  98, -55 },
    {  60,   0 },
    { 122, -60 },
    /* rest assumed to be 0s */
};

void decode_lsf(VGMSTREAMCHANNEL *stream, sample_t *outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    uint8_t frame[0x1c] = {0};
    off_t frame_offset;
    int i, frames_in, sample_count = 0;
    int index, shift, coef1, coef2;
    size_t bytes_per_frame, samples_per_frame;
    int32_t hist1 = stream->adpcm_history1_16;
    int32_t hist2 = stream->adpcm_history2_16;
    uint8_t header;


    /* external interleave (fixed size), mono */
    bytes_per_frame = 0x1c;
    samples_per_frame = (bytes_per_frame - 1) * 2;
    frames_in = first_sample / samples_per_frame;
    //first_sample = first_sample % samples_per_frame; /* for flat layout */

    /* external interleave (fixed size), mono */
    frame_offset = stream->offset + bytes_per_frame * frames_in;
    read_streamfile(frame, frame_offset, bytes_per_frame, stream->streamfile); /* ignore EOF errors */
    header = 0xFF - frame[0x00];
    shift = (header >> 4) & 0xf;
    index = (header >> 0) & 0xf;
    coef1 = lsf_coefs[index][0];
    coef2 = lsf_coefs[index][1];

    /* decode nibbles */
    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        uint8_t nibbles = frame[0x01 + i/2];
        int32_t sample;

        sample = i&1 ? /* low nibble first */
                get_high_nibble_signed(nibbles) :
                get_low_nibble_signed(nibbles);
        sample = sample * (1 << (12 - shift));
        sample = sample + (hist1 * coef1 + hist2 * coef2) / 64; /* >> 6 */
        sample = clamp16(sample);

        outbuf[sample_count] = sample;
        sample_count += channelspacing;

        hist2 = hist1;
        hist1 = sample;
    }

    stream->adpcm_history1_16 = hist1;
    stream->adpcm_history2_16 = hist2;
}
