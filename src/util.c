#include <string.h>
#include "util.h"
#include "streamtypes.h"

const char* filename_extension(const char* pathname) {
    const char* extension;

    /* favor strrchr (optimized/aligned) rather than homemade loops */
    extension = strrchr(pathname,'.');

    if (extension != NULL) {
        /* probably has extension */
        extension++; /* skip dot */

        /* find possible separators to avoid misdetecting folders with dots + extensionless files
         * (after the above to reduce search space, allows both slashes in case of non-normalized names) */
        if (strchr(extension, '/') == NULL && strchr(extension, '\\') == NULL)
            return extension; /* no slashes = really has extension */
    }

    /* extensionless: point to null after current name 
     * (could return NULL but prev code expects with to return an actual c-string) */
    return pathname + strlen(pathname);
}

void swap_extension(char* pathname, int pathname_len, const char* swap) {
    char* extension = (char*)filename_extension(pathname);
    //todo safeops
    if (extension[0] == '\0') {
        strcat(pathname, ".");
        strcat(pathname, swap);
    }
    else {
        strcpy(extension, swap);
    }
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

int round10(int val) {
    int round_val = val % 10;
    if (round_val < 5) /* half-down rounding */
        return val - round_val;
    else
        return val + (10 - round_val);
}

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
