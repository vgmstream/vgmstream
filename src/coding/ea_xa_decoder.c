#include "coding.h"
#include "../util.h"

/* AKA "EA ADPCM", evolved from CDXA. Inconsistently called EA XA/EA-XA/EAXA.
 * Some variations contain ADPCM hist header per block, but it's handled in ea_block.c */

/*
 * Another way to get coefs in EAXA v2, with no diffs (no idea which table is actually used in games):
 * coef1 = EA_XA_TABLE2[(((frame_info >> 4) & 0x0F) << 1) + 0];
 * coef2 = EA_XA_TABLE2[(((frame_info >> 4) & 0x0F) << 1) + 1];
 */
/*
static const int32_t EA_XA_TABLE2[28] = {
       0,    0,  240,    0,
     460, -208,  392, -220,
       0,    0,  240,    0,
     460,    0,  392,    0,
       0,    0,    0,    0,
    -208,   -1, -220,   -1,
       0,    0,    0, 0x3F70
};
*/

static const int EA_XA_TABLE[20] = {
    0,  240,  460,  392,
    0,    0, -208, -220,
    0,    1,    3,    4,
    7,    8,   10,   11,
    0,   -1,   -3,   -4
};

/* EA XA v2 (always mono); like v1 but with "PCM samples" flag and doesn't add 128 on expand or clamp (pre-adjusted by the encoder?) */
void decode_ea_xa_v2(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    uint8_t frame_info;
    int32_t coef1, coef2;
    int i, sample_count, shift;

    int pcm_frame_size = 0x01 + 2*0x02 + 28*0x02;
    int xa_frame_size = 0x0f;
    int frame_samples = 28;
    first_sample = first_sample % frame_samples;

    /* header */
    frame_info = read_8bit(stream->offset,stream->streamfile);

    if (frame_info == 0xEE) { /* PCM frame (used in later revisions), samples always BE */
        stream->adpcm_history1_32 = read_16bitBE(stream->offset + 0x01 + 0x00,stream->streamfile);
        stream->adpcm_history2_32 = read_16bitBE(stream->offset + 0x01 + 0x02,stream->streamfile);

        for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
            outbuf[sample_count] = read_16bitBE(stream->offset + 0x01 + 2*0x02 + i*0x02,stream->streamfile);
        }

        /* only increment offset on complete frame */
        if (i == frame_samples)
            stream->offset += pcm_frame_size;
    }
    else { /* ADPCM frame */
        coef1 = EA_XA_TABLE[(frame_info >> 4) + 0];
        coef2 = EA_XA_TABLE[(frame_info >> 4) + 4];
        shift = (frame_info & 0x0F) + 8;

        for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++, sample_count += channelspacing) {
            uint8_t sample_byte, sample_nibble;
            int32_t new_sample;
            off_t byte_offset = (stream->offset + 0x01 + i/2);
            int nibble_shift = (!(i&1)) ? 4 : 0; /* high nibble first */

            sample_byte = (uint8_t)read_8bit(byte_offset,stream->streamfile);
            sample_nibble = (sample_byte >> nibble_shift) & 0x0F;
            new_sample = (sample_nibble << 28) >> shift; /* sign extend to 32b and shift */
            new_sample = (new_sample + coef1 * stream->adpcm_history1_32 + coef2 * stream->adpcm_history2_32) >> 8;
            new_sample = clamp16(new_sample);

            outbuf[sample_count] = new_sample;
            stream->adpcm_history2_32 = stream->adpcm_history1_32;
            stream->adpcm_history1_32 = new_sample;
        }

        /* only increment offset on complete frame */
        if (i == frame_samples)
            stream->offset += xa_frame_size;
    }
}

#if 0
/* later PC games and EAAC use float math, though in the end sounds basically the same (decompiled from various exes) */
static const double XA_K0[16] = { 0.0, 0.9375, 1.796875,  1.53125 };
static const double XA_K1[16] = { 0.0,    0.0,  -0.8125, -0.859375 };
/* code uses look-up table but it's equivalent to:
 * (double)((nibble << 28) >> (shift + 8) >> 8) or (double)(signed_nibble << (12 - shift)) */
static const uint32_t FLOAT_TABLE_INT[256] = {
        0x00000000,0x45800000,0x46000000,0x46400000,0x46800000,0x46A00000,0x46C00000,0x46E00000,
        0xC7000000,0xC6E00000,0xC6C00000,0xC6A00000,0xC6800000,0xC6400000,0xC6000000,0xC5800000,
        0x00000000,0x45000000,0x45800000,0x45C00000,0x46000000,0x46200000,0x46400000,0x46600000,
        0xC6800000,0xC6600000,0xC6400000,0xC6200000,0xC6000000,0xC5C00000,0xC5800000,0xC5000000,
        0x00000000,0x44800000,0x45000000,0x45400000,0x45800000,0x45A00000,0x45C00000,0x45E00000,
        0xC6000000,0xC5E00000,0xC5C00000,0xC5A00000,0xC5800000,0xC5400000,0xC5000000,0xC4800000,
        0x00000000,0x44000000,0x44800000,0x44C00000,0x45000000,0x45200000,0x45400000,0x45600000,
        0xC5800000,0xC5600000,0xC5400000,0xC5200000,0xC5000000,0xC4C00000,0xC4800000,0xC4000000,
        0x00000000,0x43800000,0x44000000,0x44400000,0x44800000,0x44A00000,0x44C00000,0x44E00000,
        0xC5000000,0xC4E00000,0xC4C00000,0xC4A00000,0xC4800000,0xC4400000,0xC4000000,0xC3800000,
        0x00000000,0x43000000,0x43800000,0x43C00000,0x44000000,0x44200000,0x44400000,0x44600000,
        0xC4800000,0xC4600000,0xC4400000,0xC4200000,0xC4000000,0xC3C00000,0xC3800000,0xC3000000,
        0x00000000,0x42800000,0x43000000,0x43400000,0x43800000,0x43A00000,0x43C00000,0x43E00000,
        0xC4000000,0xC3E00000,0xC3C00000,0xC3A00000,0xC3800000,0xC3400000,0xC3000000,0xC2800000,
        0x00000000,0x42000000,0x42800000,0x42C00000,0x43000000,0x43200000,0x43400000,0x43600000,
        0xC3800000,0xC3600000,0xC3400000,0xC3200000,0xC3000000,0xC2C00000,0xC2800000,0xC2000000,
        0x00000000,0x41800000,0x42000000,0x42400000,0x42800000,0x42A00000,0x42C00000,0x42E00000,
        0xC3000000,0xC2E00000,0xC2C00000,0xC2A00000,0xC2800000,0xC2400000,0xC2000000,0xC1800000,
        0x00000000,0x41000000,0x41800000,0x41C00000,0x42000000,0x42200000,0x42400000,0x42600000,
        0xC2800000,0xC2600000,0xC2400000,0xC2200000,0xC2000000,0xC1C00000,0xC1800000,0xC1000000,
        0x00000000,0x40800000,0x41000000,0x41400000,0x41800000,0x41A00000,0x41C00000,0x41E00000,
        0xC2000000,0xC1E00000,0xC1C00000,0xC1A00000,0xC1800000,0xC1400000,0xC1000000,0xC0800000,
        0x00000000,0x40000000,0x40800000,0x40C00000,0x41000000,0x41200000,0x41400000,0x41600000,
        0xC1800000,0xC1600000,0xC1400000,0xC1200000,0xC1000000,0xC0C00000,0xC0800000,0xC0000000,
        0x00000000,0x3F800000,0x40000000,0x40400000,0x40800000,0x40A00000,0x40C00000,0x40E00000,
        0xC1000000,0xC0E00000,0xC0C00000,0xC0A00000,0xC0800000,0xC0400000,0xC0000000,0xBF800000,
        0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,
        0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,
        0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,
        0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,
        0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,
        0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,
};
static const float* FLOAT_TABLE = (const float *)FLOAT_TABLE_INT;

void decode_ea_xa_v2_f32(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    uint8_t frame_info;
    int i, sample_count, shift;

    int pcm_frame_size = 0x01 + 2*0x02 + 28*0x02;
    int xa_frame_size = 0x0f;
    int frame_samples = 28;
    first_sample = first_sample % frame_samples;

    /* header */
    frame_info = read_8bit(stream->offset,stream->streamfile);

    if (frame_info == 0xEE) { /* PCM frame (used in later revisions), samples always BE */
        stream->adpcm_history1_double = read_16bitBE(stream->offset + 0x01 + 0x00,stream->streamfile);
        stream->adpcm_history2_double = read_16bitBE(stream->offset + 0x01 + 0x02,stream->streamfile);

        for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
            outbuf[sample_count] = read_16bitBE(stream->offset + 0x01 + 2*0x02 + i*0x02,stream->streamfile);
        }

        /* only increment offset on complete frame */
        if (i == frame_samples)
            stream->offset += pcm_frame_size;
    }
    else { /* ADPCM frame */
        double coef1, coef2, hist1, hist2, new_sample;

        coef1 = XA_K0[(frame_info >> 4)];
        coef2 = XA_K1[(frame_info >> 4)];
        shift = (frame_info & 0x0F) + 8;// << 4;
        hist1 = stream->adpcm_history1_double;
        hist2 = stream->adpcm_history2_double;

        for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
            uint8_t sample_byte, sample_nibble;
            off_t byte_offset = (stream->offset + 0x01 + i/2);
            int nibble_shift = (!(i&1)) ? 4 : 0; /* high nibble first */

            sample_byte = (uint8_t)read_8bit(byte_offset,stream->streamfile);
            sample_nibble = (sample_byte >> nibble_shift) & 0x0F;
            new_sample = (double)FLOAT_TABLE[sample_nibble + shift];
            new_sample = new_sample + coef1 * hist1 + coef2 * hist2;

            outbuf[sample_count] = clamp16((int)new_sample);
            hist2 = hist1;
            hist1 = new_sample;
        }

        stream->adpcm_history1_double = hist1;
        stream->adpcm_history2_double = hist2;

        /* only increment offset on complete frame */
        if (i == frame_samples)
            stream->offset += xa_frame_size;
    }
}
#endif

/* EA XA v1 (mono/stereo) */
void decode_ea_xa(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel, int is_stereo) {
    uint8_t frame_info;
    int32_t coef1, coef2;
    int i, sample_count, shift;
    int hn = (channel==0); /* high nibble marker for stereo subinterleave, ch0/L=high nibble, ch1/R=low nibble */

    int frame_size = is_stereo ? 0x0f*2 : 0x0f;
    int frame_samples = 28;
    first_sample = first_sample % frame_samples;

    /* header */
    if (is_stereo) {
        /* coefs ch0+ch1 + shift ch0+ch1 */
        frame_info = read_8bit(stream->offset + 0x00, stream->streamfile);
        coef1 = EA_XA_TABLE[(hn ? frame_info >> 4 : frame_info & 0x0F) + 0];
        coef2 = EA_XA_TABLE[(hn ? frame_info >> 4 : frame_info & 0x0F) + 4];

        frame_info = read_8bit(stream->offset + 0x01, stream->streamfile);
        shift = (hn ? frame_info >> 4 : frame_info & 0x0F) + 8;
    } else {
        /* coefs + shift ch0 */
        frame_info = read_8bit(stream->offset + 0x00, stream->streamfile);
        coef1 = EA_XA_TABLE[(frame_info >> 4) + 0];
        coef2 = EA_XA_TABLE[(frame_info >> 4) + 4];
        shift = (frame_info & 0x0F) + 8;
    }

    /* samples */
    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        uint8_t sample_byte, sample_nibble;
        int32_t new_sample;
        off_t byte_offset = is_stereo ? (stream->offset + 0x02 + i) : (stream->offset + 0x01 + i/2);
        int nibble_shift = is_stereo ? (hn ? 4 : 0) : ((!(i & 1)) ? 4 : 0); /* high nibble first */

        sample_byte = (uint8_t)read_8bit(byte_offset,stream->streamfile);
        sample_nibble = (sample_byte >> nibble_shift) & 0x0F;
        new_sample = (sample_nibble << 28) >> shift; /* sign extend to 32b and shift */
        new_sample = (new_sample + coef1 * stream->adpcm_history1_32 + coef2 * stream->adpcm_history2_32 + 128) >> 8;
        new_sample = clamp16(new_sample);

        outbuf[sample_count] = new_sample;
        stream->adpcm_history2_32 = stream->adpcm_history1_32;
        stream->adpcm_history1_32 = new_sample;
    }

    /* only increment offset on complete frame */
    if (i == frame_samples)
        stream->offset += frame_size;
}

/* Maxis EA-XA v1 (mono/stereo) with byte-interleave layout in stereo mode */
void decode_maxis_xa(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    uint8_t frame_info;
    int32_t coef1, coef2;
    int i, sample_count, shift;

    int frame_size = 0x0f * channelspacing; /* varies in mono/stereo */
    int frame_samples = 28;
    first_sample = first_sample % frame_samples;

    /* header (coefs+shift ch0 + coefs+shift ch1) */
    frame_info = read_8bit(stream->offset + channel,stream->streamfile);
    coef1 = EA_XA_TABLE[(frame_info >> 4) + 0];
    coef2 = EA_XA_TABLE[(frame_info >> 4) + 4];
    shift = (frame_info & 0x0F) + 8;

    /* samples */
    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        uint8_t sample_byte, sample_nibble;
        int32_t new_sample;
        off_t byte_offset = (stream->offset + 0x01*channelspacing + (channelspacing == 2 ? i/2 + channel + (i/2)*0x01 : i/2));
        int nibble_shift = (!(i&1)) ? 4 : 0; /* high nibble first */

        sample_byte = (uint8_t)read_8bit(byte_offset,stream->streamfile);
        sample_nibble = (sample_byte >> nibble_shift) & 0x0F;
        new_sample = (sample_nibble << 28) >> shift; /* sign extend to 32b and shift */
        new_sample = (new_sample + coef1 * stream->adpcm_history1_32 + coef2 * stream->adpcm_history2_32 + 128) >> 8;
        new_sample = clamp16(new_sample);

        outbuf[sample_count] = new_sample;
        stream->adpcm_history2_32 = stream->adpcm_history1_32;
        stream->adpcm_history1_32 = new_sample;
    }

    /* only increment offset on complete frame */
    if (i == frame_samples)
        stream->offset += frame_size;
}

int32_t ea_xa_bytes_to_samples(size_t bytes, int channels) {
    if (channels <= 0) return 0;
    return bytes / channels / 0x0f * 28;
}
