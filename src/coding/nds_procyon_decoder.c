#include "coding.h"
#include "../util.h"

/* standard XA/PSX coefs << 6 */
static const int8_t proc_coefs[16][2] = {
    {   0,   0 },
    {  60,   0 },
    { 115, -52 },
    {  98, -55 },
    { 122, -60 },
    /* rest is 0s */
};

/* ADPCM found in NDS games using Procyon Studio Digital Sound Elements */
void decode_nds_procyon(VGMSTREAMCHANNEL *stream, sample_t *outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    uint8_t frame[0x10] = {0};
    off_t frame_offset;
    int i, frames_in, sample_count = 0;
    size_t bytes_per_frame, samples_per_frame;
    int index, scale, coef1, coef2;
    int32_t hist1 = stream->adpcm_history1_32;
    int32_t hist2 = stream->adpcm_history2_32;
    uint8_t header;


    /* external interleave (fixed size), mono */
    bytes_per_frame = 0x10;
    samples_per_frame = (bytes_per_frame - 0x01) * 2; /* 30 */
    frames_in = first_sample / samples_per_frame;

    /* parse frame header */
    frame_offset = stream->offset + bytes_per_frame * frames_in;
    read_streamfile(frame, frame_offset, bytes_per_frame, stream->streamfile); /* ignore EOF errors */
    header = frame[0x0F] ^ 0x80;
    scale = 12 - (header & 0xf);
    index = (header >> 4) & 0xf;
    coef1 = proc_coefs[index][0];
    coef2 = proc_coefs[index][1];

    /* decode nibbles */
    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        uint8_t nibbles = frame[i/2] ^ 0x80;
        int32_t sample = 0;

        sample = i&1 ? /* low nibble first */
                get_high_nibble_signed(nibbles) :
                get_low_nibble_signed(nibbles);
        sample = sample * 64 * 64; /* << 12 */
        if (scale < 0)
            sample <<= -scale;
        else
            sample >>= scale;
        sample = (hist1 * coef1 + hist2 * coef2 + 32) / 64  + (sample * 64);

        hist2 = hist1;
        hist1 = sample; /* clamp *after* this */

        outbuf[sample_count] = clamp16((sample + 32) / 64) / 64 * 64;
        sample_count += channelspacing;
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_history2_32 = hist2;
}
