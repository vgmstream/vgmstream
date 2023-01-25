#include "coding.h"
#include "../util.h"
#include <math.h>

void decode_pcm16le(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i;
    int32_t sample_count;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        outbuf[sample_count]=read_16bitLE(stream->offset+i*2,stream->streamfile);
    }
}

void decode_pcm16be(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i;
    int32_t sample_count;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        outbuf[sample_count]=read_16bitBE(stream->offset+i*2,stream->streamfile);
    }
}

void decode_pcm16_int(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int big_endian) {
    int i, sample_count;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = big_endian ? read_16bitBE : read_16bitLE;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        outbuf[sample_count]=read_16bit(stream->offset+i*2*channelspacing,stream->streamfile);
    }
}

void decode_pcm8(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i;
    int32_t sample_count;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        outbuf[sample_count]=read_8bit(stream->offset+i,stream->streamfile)*0x100;
    }
}

void decode_pcm8_int(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i;
    int32_t sample_count;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        outbuf[sample_count]=read_8bit(stream->offset+i*channelspacing,stream->streamfile)*0x100;
    }
}

void decode_pcm8_unsigned(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i;
    int32_t sample_count;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        int16_t v = (uint8_t)read_8bit(stream->offset+i,stream->streamfile);
        outbuf[sample_count] = v*0x100 - 0x8000;
    }
}

void decode_pcm8_unsigned_int(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i;
    int32_t sample_count;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        int16_t v = (uint8_t)read_8bit(stream->offset+i*channelspacing,stream->streamfile);
        outbuf[sample_count] = v*0x100 - 0x8000;
    }
}

void decode_pcm8_sb(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i;
    int32_t sample_count;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        int16_t v = (uint8_t)read_8bit(stream->offset+i,stream->streamfile);
        if (v&0x80) v = 0-(v&0x7f);
        outbuf[sample_count] = v*0x100;
    }
}

void decode_pcm4(VGMSTREAM * vgmstream, VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    int i, nibble_shift, is_high_first, is_stereo;
    int32_t sample_count;
    int16_t v;
    off_t byte_offset;

    is_high_first = (vgmstream->codec_config & 1);
    is_stereo = (vgmstream->channels != 1);

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        byte_offset = is_stereo ?
                stream->offset + i :    /* stereo: one nibble per channel (assumed, not sure if stereo version actually exists) */
                stream->offset + i/2;   /* mono: consecutive nibbles */
        nibble_shift = is_high_first ?
                is_stereo ? (!(channel&1) ? 4:0) : (!(i&1) ? 4:0) : /* even = high, odd = low */
                is_stereo ? (!(channel&1) ? 0:4) : (!(i&1) ? 0:4);  /* even = low, odd = high */

        v = (int16_t)read_8bit(byte_offset, stream->streamfile);
        v = (v >> nibble_shift) & 0x0F;
        outbuf[sample_count] = v*0x11*0x100;
    }
}

void decode_pcm4_unsigned(VGMSTREAM * vgmstream, VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel) {
    int i, nibble_shift, is_high_first, is_stereo;
    int32_t sample_count;
    int16_t v;
    off_t byte_offset;

    is_high_first = (vgmstream->codec_config & 1);
    is_stereo = (vgmstream->channels != 1);

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        byte_offset = is_stereo ?
                stream->offset + i :    /* stereo: one nibble per channel (assumed, not sure if stereo version actually exists) */
                stream->offset + i/2;   /* mono: consecutive nibbles */
        nibble_shift = is_high_first ?
                is_stereo ? (!(channel&1) ? 4:0) : (!(i&1) ? 4:0) : /* even = high, odd = low */
                is_stereo ? (!(channel&1) ? 0:4) : (!(i&1) ? 0:4);  /* even = low, odd = high */

        v = (int16_t)read_8bit(byte_offset, stream->streamfile);
        v = (v >> nibble_shift) & 0x0F;
        outbuf[sample_count] = v*0x11*0x100 - 0x8000;
    }
}

static int expand_ulaw(uint8_t ulawbyte) {
    int sign, segment, quantization, sample;
    const int bias = 0x84;

    ulawbyte = ~ulawbyte; /* stored in complement */
    sign = (ulawbyte & 0x80);
    segment = (ulawbyte & 0x70) >> 4; /* exponent */
    quantization = ulawbyte & 0x0F; /* mantissa */

    sample = (quantization << 3) + bias; /* add bias */
    sample <<= segment;
    sample = (sign) ? (bias - sample) : (sample - bias); /* remove bias */

#if 0   // the above follows Sun's implementation, but this works too
    {
        static int exp_lut[8] = {0,132,396,924,1980,4092,8316,16764}; /* precalcs from bias */
        new_sample = exp_lut[segment] + (quantization << (segment + 3));
        if (sign != 0) new_sample = -new_sample;
    }
#endif

    return sample;
}

/* decodes u-law (ITU G.711 non-linear PCM), from g711.c */
void decode_ulaw(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i, sample_count;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        uint8_t ulawbyte = read_8bit(stream->offset+i,stream->streamfile);
        outbuf[sample_count] = expand_ulaw(ulawbyte);
    }
}


void decode_ulaw_int(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i, sample_count;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        uint8_t ulawbyte = read_8bit(stream->offset+i*channelspacing,stream->streamfile);
        outbuf[sample_count] = expand_ulaw(ulawbyte);
    }
}

static int expand_alaw(uint8_t alawbyte) {
    int sign, segment, quantization, sample;

    alawbyte ^= 0x55;
    sign = (alawbyte & 0x80);
    segment = (alawbyte & 0x70) >> 4; /* exponent */
    quantization = alawbyte & 0x0F; /* mantissa */

    sample = (quantization << 4);
    switch (segment) {
        case 0:
            sample += 8;
            break;
        case 1:
            sample += 0x108;
            break;
        default:
            sample += 0x108;
            sample <<= segment - 1;
            break;
    }
    sample = (sign) ? sample : -sample;

    return sample;
}

/* decodes a-law (ITU G.711 non-linear PCM), from g711.c */
void decode_alaw(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i, sample_count;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        uint8_t alawbyte = read_8bit(stream->offset+i,stream->streamfile);
        outbuf[sample_count] = expand_alaw(alawbyte);;
    }
}

void decode_pcmfloat(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int big_endian) {
    int i, sample_count;
    float (*read_f32)(off_t,STREAMFILE*) = big_endian ? read_f32be : read_f32le;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        float sample_float = read_f32(stream->offset+i*4,stream->streamfile);
        int sample_pcm = (int)floor(sample_float * 32767.f + .5f);

        outbuf[sample_count] = clamp16(sample_pcm);
    }
}

void decode_pcm24be(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i;
    int32_t sample_count;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count += channelspacing) {
        off_t offset = stream->offset + i * 0x03;
        int v = read_u8(offset+0x02, stream->streamfile) | (read_s16be(offset + 0x00, stream->streamfile) << 8);
        outbuf[sample_count] = (v >> 8);
    }
}

void decode_pcm24le(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i;
    int32_t sample_count;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        off_t offset = stream->offset + i * 0x03;
        int v = read_u8(offset+0x00, stream->streamfile) | (read_s16le(offset + 0x01, stream->streamfile) << 8);
        outbuf[sample_count] = (v >> 8);
    }
}

int32_t pcm_bytes_to_samples(size_t bytes, int channels, int bits_per_sample) {
    if (channels <= 0 || bits_per_sample <= 0) return 0;
    return ((int64_t)bytes * 8) / channels / bits_per_sample;
}

int32_t pcm24_bytes_to_samples(size_t bytes, int channels) {
    return pcm_bytes_to_samples(bytes, channels, 24);
}

int32_t pcm16_bytes_to_samples(size_t bytes, int channels) {
    return pcm_bytes_to_samples(bytes, channels, 16);
}

int32_t pcm8_bytes_to_samples(size_t bytes, int channels) {
    return pcm_bytes_to_samples(bytes, channels, 8);
}
