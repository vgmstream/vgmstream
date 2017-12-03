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

/* EA XA v2 (always mono); like ea_xa_int but with "PCM samples" flag and doesn't add 128 on expand or clamp (pre-adjusted by the encoder?) */
void decode_ea_xa_v2(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do,int channel) {
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

        for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
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

/* EA XA v1 stereo */
void decode_ea_xa(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do,int channel) {
    uint8_t frame_info;
    int32_t coef1, coef2;
    int i, sample_count, shift;
    int hn = (channel==0); /* high nibble marker for stereo subinterleave, ch0/L=high nibble, ch1/R=low nibble */

    int frame_size = 0x1e;
    int frame_samples = 28;
    first_sample = first_sample % frame_samples;

    /* header (coefs ch0+ch1 + shift ch0+ch1) */
    frame_info = read_8bit(stream->offset+0x00,stream->streamfile);
    coef1 = EA_XA_TABLE[(hn ? frame_info >> 4 : frame_info & 0x0F) + 0];
    coef2 = EA_XA_TABLE[(hn ? frame_info >> 4 : frame_info & 0x0F) + 4];
    shift = (frame_info & 0x0F) + 8;

    frame_info = read_8bit(stream->offset+0x01,stream->streamfile);
    shift = (hn ? frame_info >> 4 : frame_info & 0x0F) + 8;

    /* samples */
    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        uint8_t sample_byte, sample_nibble;
        int32_t new_sample;
        off_t byte_offset = (stream->offset + 0x02 + i);
        int nibble_shift = (hn ? 4 : 0); /* high nibble first */

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

/* EA-XA v1 mono/interleave */
void decode_ea_xa_int(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do,int channel) {
    uint8_t frame_info;
    int32_t coef1, coef2;
    int i, sample_count, shift;

    int frame_size = 0x0f;
    int frame_samples = 28;
    first_sample = first_sample % frame_samples;

    /* header (coefs+shift ch0) */
    frame_info = read_8bit(stream->offset,stream->streamfile);
    coef1 = EA_XA_TABLE[(frame_info >> 4) + 0];
    coef2 = EA_XA_TABLE[(frame_info >> 4) + 4];
    shift = (frame_info & 0x0F) + 8;

    /* samples */
    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        uint8_t sample_byte, sample_nibble;
        int32_t new_sample;
        off_t byte_offset = (stream->offset + 0x01 + i/2);
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

/* Maxis EA-XA v1 (mono+stereo) with byte-interleave layout in stereo mode */
void decode_maxis_xa(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
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
