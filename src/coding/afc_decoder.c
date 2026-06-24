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

typedef struct {
    int32_t hist1;
    int32_t hist2;
} adpcm_t;

static void decode_afc_internal(adpcm_t* ctx, STREAMFILE* sf, off_t sf_offset, short* outbuf, int channels, int32_t first_sample, int32_t samples_to_do) {
    uint8_t frame[0x09] = {0};


    /* external interleave, mono */
    uint16_t bytes_per_frame = 0x09;
    int samples_per_frame = (bytes_per_frame - 0x01) * 2; // always 16
    int frames_in = first_sample / samples_per_frame;
    first_sample = first_sample % samples_per_frame; // for flat/blocked layout

    /* parse frame header */
    off_t frame_offset = sf_offset + bytes_per_frame * frames_in;
    read_streamfile(frame, frame_offset, bytes_per_frame, sf); // ignore EOF errors
    int scale = 1 << ((frame[0] >> 4) & 0xf);
    int index = (frame[0] & 0xf);
    int coef1 = afc_coefs[index][0];
    int coef2 = afc_coefs[index][1];

    int32_t hist1 = ctx->hist1;
    int32_t hist2 = ctx->hist2;
    int sample_count = 0;

    /* decode nibbles */
    for (int i = first_sample; i < first_sample + samples_to_do; i++) {
        uint8_t code = frame[0x01 + i/2];
        int32_t sample;

        sample = get_nibble_signed(code, (i & 1) == 0); // high nibble first
        sample = ((sample * scale) << 11);
        sample = (sample + coef1 * hist1 + coef2 * hist2) >> 11;

        sample = clamp16(sample);

        outbuf[sample_count] = sample;
        sample_count += channels;

        hist2 = hist1;
        hist1 = sample;
    }

    ctx->hist1 = hist1;
    ctx->hist2 = hist2;
}

void decode_afc(VGMSTREAMCHANNEL* stream, short* outbuf, int channels, int32_t first_sample, int32_t samples_to_do) {
    adpcm_t ctx = {
        .hist1 = stream->adpcm_history1_16,
        .hist2 = stream->adpcm_history2_16
    };

    decode_afc_internal(&ctx, stream->streamfile, stream->offset, outbuf, channels, first_sample, samples_to_do);

    stream->adpcm_history1_16 = ctx.hist1;
    stream->adpcm_history2_16 = ctx.hist2;

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

/* Combines AFC frames: A B C D into L=A+C, R=B+D (each frame being a substream with separate hist).
 * Info from: https://github.com/projectPiki/pikmin (see DecodeADPCM4X)
*/
void decode_afc_4x(VGMSTREAM* vgmstream, short* outbuf,  int32_t first_sample, int32_t samples_to_do) {
    int channels = vgmstream->channels;
    if (channels != 2) {
        VGM_LOG("AFC: unknown layout\n");
        return;
    }

    // OG code has separate state for 4 frames, so hack it a bit using multi-hist + joining.
    VGMSTREAMCHANNEL* stream_l = &vgmstream->ch[0];
    VGMSTREAMCHANNEL* stream_r = &vgmstream->ch[1];
    off_t offset = stream_l->offset;
    adpcm_t ctx[4] = {0};
    short tmp[4][16] = {0};

    
    int sub_first_sample = 0;
    int sub_frame_samples = 16;
    int sub_channels = 1;
    first_sample = first_sample % sub_frame_samples;

    // map streams to tmp context
    ctx[0].hist1 = stream_l->adpcm_history1_16;
    ctx[0].hist2 = stream_l->adpcm_history2_16;
    ctx[1].hist1 = stream_l->adpcm_history3_16;
    ctx[1].hist2 = stream_l->adpcm_history4_16;
    ctx[2].hist1 = stream_r->adpcm_history1_16;
    ctx[2].hist2 = stream_r->adpcm_history2_16;
    ctx[3].hist1 = stream_r->adpcm_history3_16;
    ctx[3].hist2 = stream_r->adpcm_history4_16;

    // decode x4 frames into tmp buffers
    for (int i = 0; i < 4; i++) {
        decode_afc_internal(&ctx[i], stream_l->streamfile, offset, &tmp[i][0], sub_channels, sub_first_sample, sub_frame_samples);
        offset += 0x09;
    }

    // mix x4 into x2 frames and copy output
    int mix_level = 0x5fff;
    int samples_done = 0;
    for (int i = 0; i < samples_to_do; i++) {
        int sample_l = (tmp[0][i] * mix_level >> 15) + (tmp[2][i] * mix_level >> 15); // A + C
        int sample_r = (tmp[1][i] * mix_level >> 15) + (tmp[3][i] * mix_level >> 15); // B + D

        if (i >= first_sample) {
            outbuf[samples_done++] = clamp16(sample_l);
            outbuf[samples_done++] = clamp16(sample_r);
        }
    }

    /* internal interleave: increment offset + save state on complete frame(s) */
    if (first_sample + samples_done / channels == sub_frame_samples) {
        stream_l->adpcm_history1_16 = ctx[0].hist1;
        stream_l->adpcm_history2_16 = ctx[0].hist2;
        stream_l->adpcm_history3_16 = ctx[1].hist1;
        stream_l->adpcm_history4_16 = ctx[1].hist2;
        stream_r->adpcm_history1_16 = ctx[2].hist1;
        stream_r->adpcm_history2_16 = ctx[2].hist2;
        stream_r->adpcm_history3_16 = ctx[3].hist1;
        stream_r->adpcm_history4_16 = ctx[3].hist2;

        stream_l->offset = offset;
        stream_r->offset = offset;
    }
}


int32_t afc_bytes_to_samples(size_t bytes, int channels) {
    if (channels <= 0) return 0;
    int frame_bytes = 0x09;
    int frame_samples = (frame_bytes - 0x01) * 2;
    return bytes / frame_bytes * frame_samples / channels;
}

int32_t afc_4x_bytes_to_samples(size_t bytes, int channels) {
    return afc_bytes_to_samples(bytes / 2, channels);
}
