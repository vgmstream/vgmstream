#include "samples_ops.h"


void swap_samples_le(sample_t *buf, int count) {
    /* Windows can't be BE... I think */
#if !defined(_WIN32)
#if !defined(__BYTE_ORDER__) || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
    int i;
    for (i = 0; i < count; i++) {
        /* 16b sample in memory: aabb where aa=MSB, bb=LSB */
        uint8_t b0 = buf[i] & 0xff;
        uint8_t b1 = buf[i] >> 8;
        uint8_t *p = (uint8_t*)&(buf[i]);
        /* 16b sample in buffer: bbaa where bb=LSB, aa=MSB */
        p[0] = b0;
        p[1] = b1;
        /* when endianness is LE, buffer has bbaa already so this function can be skipped */
    }
#endif
#endif
}


/* unused */
/*
void interleave_channel(sample_t * outbuffer, sample_t * inbuffer, int32_t sample_count, int channel_count, int channel_number) {
    int32_t insample,outsample;

    if (channel_count==1) {
        memcpy(outbuffer,inbuffer,sizeof(sample)*sample_count);
        return;
    }

    for (insample=0,outsample=channel_number;insample<sample_count;insample++,outsample+=channel_count) {
        outbuffer[outsample]=inbuffer[insample];
    }
}
*/

/* failed attempt at interleave in place */
/*
void interleave_stereo(sample_t * buffer, int32_t sample_count) {
    int32_t tomove, belongs;
    sample_t moving,temp;

    tomove = sample_count;
    moving = buffer[tomove];

    do {
        if (tomove<sample_count)
            belongs = tomove*2;
        else
            belongs = (tomove-sample_count)*2+1;

        temp = buffer[belongs];
        buffer[belongs] = moving;
        moving = temp;

        tomove = belongs;
    } while (tomove != sample_count);
}
*/

