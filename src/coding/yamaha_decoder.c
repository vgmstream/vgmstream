#include "../util.h"
#include "coding.h"

/* fixed point (.8) amount to scale the current step size by */
/* part of the same series as used in MS ADPCM "ADPCMTable" */
static const unsigned int scale_step[16] = {
    230, 230, 230, 230, 307, 409, 512, 614,
    230, 230, 230, 230, 307, 409, 512, 614
};

/* actually implemented with if-else/switchs but that's too goofy */
static const int scale_step_aska[8] = {
    57, 57, 57, 57, 77, 102, 128, 153,
};

/* expand an unsigned four bit delta to a wider signed range */
static const int scale_delta[16] = {
      1,  3,  5,  7,  9, 11, 13, 15,
     -1, -3, -5, -7, -9,-11,-13,-15
};


/* raw Yamaha ADPCM a.k.a AICA as it's prominently used in Naomi/Dreamcast's Yamaha AICA sound chip,
 * also found in Windows RIFF and older Yamaha's arcade sound chips. */
void decode_yamaha(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel, int is_stereo) {
    int i, sample_count;

    int32_t hist1 = stream->adpcm_history1_16;
    int step_size = stream->adpcm_step_index;

    /* no header (external setup), pre-clamp for wrong values */
    if (step_size < 0x7f) step_size = 0x7f;
    if (step_size > 0x6000) step_size = 0x6000;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        int sample_nibble, sample_decoded, sample_delta;
        off_t byte_offset = is_stereo ?
                stream->offset + i :    /* stereo: one nibble per channel */
                stream->offset + i/2;   /* mono: consecutive nibbles */
        int nibble_shift = is_stereo ?
                (!(channel&1) ? 0:4) :  /* even = low/L, odd = high/R */
                (!(i&1) ? 0:4);         /* low nibble first */

        /* Yamaha/AICA expand, but same result as IMA's (((delta * 2 + 1) * step) >> 3) */
        sample_nibble = ((read_8bit(byte_offset,stream->streamfile) >> nibble_shift))&0xf;
        sample_delta = (step_size * scale_delta[sample_nibble]) / 8;
        sample_decoded = hist1 + sample_delta;

        outbuf[sample_count] = clamp16(sample_decoded);
        hist1 = outbuf[sample_count];

        step_size = (step_size * scale_step[sample_nibble]) >> 8;
        if (step_size < 0x7f) step_size = 0x7f;
        if (step_size > 0x6000) step_size = 0x6000;
    }

    stream->adpcm_history1_16 = hist1;
    stream->adpcm_step_index = step_size;
}

/* tri-Ace Aska ADPCM, same-ish with modified step table (reversed from Android SO's .so) */
void decode_aska(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    int i, sample_count, num_frame;
    int32_t hist1 = stream->adpcm_history1_32;
    int step_size = stream->adpcm_step_index;

    /* external interleave */
    int block_samples = (0x40 - 0x04*channelspacing) * 2 / channelspacing;
    num_frame = first_sample / block_samples;
    first_sample = first_sample % block_samples;

    /* header (hist+step) */
    if (first_sample == 0) {
        off_t header_offset = stream->offset + 0x40*num_frame + 0x04*channel;

        hist1     = read_16bitLE(header_offset+0x00,stream->streamfile);
        step_size = read_16bitLE(header_offset+0x02,stream->streamfile);
        if (step_size < 0x7f) step_size = 0x7f;
        if (step_size > 0x6000) step_size = 0x6000;
    }

    /* decode nibbles (layout: varies) */
    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        int sample_nibble, sample_decoded, sample_delta;
        off_t byte_offset = (channelspacing == 2) ?
                (stream->offset + 0x40*num_frame + 0x04*channelspacing) + i :    /* stereo: one nibble per channel */
                (stream->offset + 0x40*num_frame + 0x04*channelspacing) + i/2;   /* mono: consecutive nibbles */
        int nibble_shift = (channelspacing == 2) ?
                (!(channel&1) ? 0:4) :
                (!(i&1) ? 0:4);  /* even = low, odd = high */

        sample_nibble = (read_8bit(byte_offset,stream->streamfile) >> nibble_shift) & 0xf;
        sample_delta = ((((sample_nibble & 0x7) * 2) | 1) * step_size) >> 3; /* like 'mul' IMA with 'or' */
        if (sample_nibble & 8) sample_delta = -sample_delta;
        sample_decoded = hist1 + sample_delta;

        outbuf[sample_count] = sample_decoded; /* not clamped */
        hist1 = outbuf[sample_count];

        step_size = (step_size * scale_step_aska[sample_nibble & 0x07]) >> 6;
        if (step_size < 0x7f) step_size = 0x7f;
        if (step_size > 0x6000) step_size = 0x6000;
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_size;
}

/* Yamaha ADPCM with unknown expand variation (noisy), step size is double of normal Yamaha? */
void decode_nxap(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i, sample_count, num_frame;
    int32_t hist1 = stream->adpcm_history1_32;
    int step_size = stream->adpcm_step_index;

    /* external interleave */
    int block_samples = (0x40 - 0x4) * 2;
    num_frame = first_sample / block_samples;
    first_sample = first_sample % block_samples;

    /* header (hist+step) */
    if (first_sample == 0) {
        off_t header_offset = stream->offset + 0x40*num_frame;

        hist1     = read_16bitLE(header_offset+0x00,stream->streamfile);
        step_size = read_16bitLE(header_offset+0x02,stream->streamfile);
        if (step_size < 0x7f) step_size = 0x7f;
        if (step_size > 0x6000) step_size = 0x6000;
    }

    /* decode nibbles (layout: all nibbles from one channel) */
    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        int sample_nibble, sample_decoded, sample_delta;
        off_t byte_offset = (stream->offset + 0x40*num_frame + 0x04) + i/2;
        int nibble_shift = (i&1?4:0); /* low nibble first */

        /* Yamaha expand? */
        sample_nibble = (read_8bit(byte_offset,stream->streamfile) >> nibble_shift)&0xf;
        sample_delta = (step_size * scale_delta[sample_nibble] / 4) / 8; //todo not ok
        sample_decoded = hist1 + sample_delta;

        outbuf[sample_count] = clamp16(sample_decoded);
        hist1 = outbuf[sample_count];

        step_size = (step_size * scale_step[sample_nibble]) >> 8;
        if (step_size < 0x7f) step_size = 0x7f;
        if (step_size > 0x6000) step_size = 0x6000;
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_step_index = step_size;
}

size_t yamaha_bytes_to_samples(size_t bytes, int channels) {
    if (channels <= 0) return 0;
    /* 2 samples per byte (2 nibbles) in stereo or mono config */
    return bytes * 2 / channels;
}

size_t aska_bytes_to_samples(size_t bytes, int channels) {
    int block_align = 0x40;
    if (channels <= 0) return 0;
    return (bytes / block_align) * (block_align - 0x04*channels) * 2 / channels
            + ((bytes % block_align) ? ((bytes % block_align) - 0x04*channels) * 2 / channels : 0);
}
