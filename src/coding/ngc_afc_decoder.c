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

void decode_afc(VGMSTREAMCHANNEL* stream, short* outbuf, int channels, int32_t first_sample, int32_t samples_to_do) {
    uint8_t frame[0x09] = {0};


    /* external interleave, mono */
    uint16_t bytes_per_frame = 0x09;
    int samples_per_frame = (bytes_per_frame - 0x01) * 2; // always 16
    int frames_in = first_sample / samples_per_frame;
    first_sample = first_sample % samples_per_frame; // for flat/blocked layout

    /* parse frame header */
    off_t frame_offset = stream->offset + bytes_per_frame * frames_in;
    read_streamfile(frame, frame_offset, bytes_per_frame, stream->streamfile); // ignore EOF errors
    int scale = 1 << ((frame[0] >> 4) & 0xf);
    int index = (frame[0] & 0xf);
    int coef1 = afc_coefs[index][0];
    int coef2 = afc_coefs[index][1];

    int32_t hist1 = stream->adpcm_history1_16;
    int32_t hist2 = stream->adpcm_history2_16;
    int sample_count = 0;

    /* decode nibbles */
    for (int i = first_sample; i < first_sample + samples_to_do; i++) {
        uint8_t code = frame[0x01 + i/2];
        int32_t sample;

        sample = i & 1 ? /* high nibble first */
                get_low_nibble_signed(code) :
                get_high_nibble_signed(code);
        sample = ((sample * scale) << 11);
        sample = (sample + coef1 * hist1 + coef2 * hist2) >> 11;

        sample = clamp16(sample);

        outbuf[sample_count] = sample;
        sample_count += channels;

        hist2 = hist1;
        hist1 = sample;
    }

    stream->adpcm_history1_16 = hist1;
    stream->adpcm_history2_16 = hist2;
}


static int nibble2_to_int[4] = {0, 1, -2, -1};
static int nibble2_shift[4] = {6, 4, 2, 0};

static inline int get_nibble2_signed(uint8_t n, int skip) {
    int shift = nibble2_shift[skip & 0x03];
    return nibble2_to_int[(n >> shift) & 0x03];
}

// some info from: https://github.com/XAYRGA/JaiSeqX
void decode_afc_2bit(VGMSTREAMCHANNEL* stream, short* outbuf, int channels, int32_t first_sample, int32_t samples_to_do) {
    uint8_t frame[0x05] = {0};


    /* external interleave, mono */
    uint16_t bytes_per_frame = 0x05;
    int samples_per_frame = (bytes_per_frame - 0x01) * 4; // always 16
    int frames_in = first_sample / samples_per_frame;
    first_sample = first_sample % samples_per_frame; // for flat/blocked layout

    /* parse frame header */
    off_t frame_offset = stream->offset + bytes_per_frame * frames_in;
    read_streamfile(frame, frame_offset, bytes_per_frame, stream->streamfile); /* ignore EOF errors */
    int scale = 8192 << ((frame[0] >> 4) & 0xf);
    int index = (frame[0] & 0xf);
    int coef1 = afc_coefs[index][0];
    int coef2 = afc_coefs[index][1];

    int32_t hist1 = stream->adpcm_history1_16;
    int32_t hist2 = stream->adpcm_history2_16;
    int sample_count = 0;

    /* decode nibbles */
    for (int i = first_sample; i < first_sample + samples_to_do; i++) {
        uint8_t code = frame[0x01 + i/4];
        int32_t sample;

        sample = get_nibble2_signed(code, i);
        sample = (sample * scale);
        sample = (sample + coef1 * hist1 + coef2 * hist2) >> 11;

        sample = clamp16(sample);

        outbuf[sample_count] = sample;
        sample_count += channels;

        hist2 = hist1;
        hist1 = sample;
    }

    stream->adpcm_history1_16 = hist1;
    stream->adpcm_history2_16 = hist2;
}
