#include "coding.h"
#include "../util.h"
#include <math.h>


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
        uint8_t ulawbyte = read_u8(stream->offset+i,stream->streamfile);
        outbuf[sample_count] = expand_ulaw(ulawbyte);
    }
}


void decode_ulaw_int(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i, sample_count;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        uint8_t ulawbyte = read_u8(stream->offset+i*channelspacing,stream->streamfile);
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
