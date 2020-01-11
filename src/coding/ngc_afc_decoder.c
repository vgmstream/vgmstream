#include "coding.h"
#include "../util.h"

static const int16_t afc_coefs[16][2] = {
        {    0,    0 },
        { 2048,    0 },
        {    0, 2048 },
        { 1024, 1024 },
        { 4096,-2048 },
        { 3584,-1536 },
        { 3072,-1024 },
        { 4608,-2560 },
        { 4200,-2248 },
        { 4800,-2300 },
        { 5120,-3072 },
        { 2048,-2048 },
        { 1024,-1024 },
        {-1024, 1024 },
        {-1024,    0 },
        {-2048,    0 }
};

void decode_ngc_afc(VGMSTREAMCHANNEL *stream, sample_t *outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    uint8_t frame[0x09] = {0};
    off_t frame_offset;
    int i, frames_in, sample_count = 0;
    size_t bytes_per_frame, samples_per_frame;
    int index, scale, coef1, coef2;
    int32_t hist1 = stream->adpcm_history1_16;
    int32_t hist2 = stream->adpcm_history2_16;


    /* external interleave, mono */
    bytes_per_frame = 0x09;
    samples_per_frame = (bytes_per_frame - 0x01) * 2; /* always 16 */
    frames_in = first_sample / samples_per_frame;
    first_sample = first_sample % samples_per_frame; /* for flat/blocked layout */

    /* parse frame header */
    frame_offset = stream->offset + bytes_per_frame * frames_in;
    read_streamfile(frame, frame_offset, bytes_per_frame, stream->streamfile); /* ignore EOF errors */
    scale = 1 << ((frame[0] >> 4) & 0xf);
    index = (frame[0] & 0xf);
    coef1 = afc_coefs[index][0];
    coef2 = afc_coefs[index][1];

    /* decode nibbles */
    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        uint8_t nibbles = frame[0x01 + i/2];
        int32_t sample;

        sample = i&1 ? /* high nibble first */
                get_low_nibble_signed(nibbles) :
                get_high_nibble_signed(nibbles);
        sample = ((sample * scale) << 11);
        sample = (sample + coef1*hist1 + coef2*hist2) >> 11;

        sample = clamp16(sample);

        outbuf[sample_count] = sample;
        sample_count += channelspacing;

        hist2 = hist1;
        hist1 = sample;
    }

    stream->adpcm_history1_16 = hist1;
    stream->adpcm_history2_16 = hist2;
}
