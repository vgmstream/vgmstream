#include <string.h>
#include "util.h"
#include "streamtypes.h"

int check_sample_rate(int32_t sr) {
    return !(sr<300 || sr>96000);
}

const char * filename_extension(const char * filename) {
    const char * ext;

    /* You know what would be nice? strrchrnul().
     * Instead I have to do it myself. */
    ext = strrchr(filename,'.');
    if (ext==NULL) ext=filename+strlen(filename); /* point to null, i.e. an empty string for the extension */
    else ext=ext+1; /* skip the dot */

    return ext;
}

void interleave_channel(sample * outbuffer, sample * inbuffer, int32_t sample_count, int channel_count, int channel_number) {
    int32_t insample,outsample;

    if (channel_count==1) {
        memcpy(outbuffer,inbuffer,sizeof(sample)*sample_count);
        return;
    }

    for (insample=0,outsample=channel_number;insample<sample_count;insample++,outsample+=channel_count) {
        outbuffer[outsample]=inbuffer[insample];
    }
}

/* failed attempt at interleave in place */
/*
void interleave_stereo(sample * buffer, int32_t sample_count) {
    int32_t tomove, belongs;
    sample moving,temp;

    tomove = sample_count;
    moving = buffer[tomove];

    do {
        if (tomove<sample_count)
            belongs = tomove*2;
        else
            belongs = (tomove-sample_count)*2+1;

        printf("move %d to %d\n",tomove,belongs);

        temp = buffer[belongs];
        buffer[belongs] = moving;
        moving = temp;

        tomove = belongs;
    } while (tomove != sample_count);
}
*/

void put_8bit(uint8_t * buf, int8_t i) {
    buf[0] = i;
}

void put_16bitLE(uint8_t * buf, int16_t i) {
    buf[0] = (i & 0xFF);
    buf[1] = i >> 8;
}

void put_32bitLE(uint8_t * buf, int32_t i) {
    buf[0] = (uint8_t)(i & 0xFF);
    buf[1] = (uint8_t)((i >> 8) & 0xFF);
    buf[2] = (uint8_t)((i >> 16) & 0xFF);
    buf[3] = (uint8_t)((i >> 24) & 0xFF);
}

void put_16bitBE(uint8_t * buf, int16_t i) {
    buf[0] = i >> 8;
    buf[1] = (i & 0xFF);
}

void put_32bitBE(uint8_t * buf, int32_t i) {
    buf[0] = (uint8_t)((i >> 24) & 0xFF);
    buf[1] = (uint8_t)((i >> 16) & 0xFF);
    buf[2] = (uint8_t)((i >> 8) & 0xFF);
    buf[3] = (uint8_t)(i & 0xFF);
}

void swap_samples_le(sample *buf, int count) {
    int i;
    for (i=0;i<count;i++) {
        uint8_t b0 = buf[i]&0xff;
        uint8_t b1 = buf[i]>>8;
        uint8_t *p = (uint8_t*)&(buf[i]);
        p[0] = b0;
        p[1] = b1;
    }
}

/* length is maximum length of dst. dst will always be null-terminated if
 * length > 0 */
void concatn(int length, char * dst, const char * src) {
    int i,j;
    if (length <= 0) return;
    for (i=0;i<length-1 && dst[i];i++);   /* find end of dst */
    for (j=0;i<length-1 && src[j];i++,j++)
        dst[i]=src[j];
    dst[i]='\0';
}
