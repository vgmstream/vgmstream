/*
 * util.h - utility functions
 */

#include "streamtypes.h"

#ifndef _UTIL_H
#define _UTIL_H

/* host endian independent multi-byte integer reading */

static inline int16_t get_16bitBE(uint8_t * p) {
    return (p[0]<<8) | (p[1]);
}

static inline int16_t get_16bitLE(uint8_t * p) {
    return (p[0]) | (p[1]<<8);
}

static inline int32_t get_32bitBE(uint8_t * p) {
    return (p[0]<<24) | (p[1]<<16) | (p[2]<<8) | (p[3]);
}

static inline int32_t get_32bitLE(uint8_t * p) {
    return (p[0]) | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
}

/* signed nibbles come up a lot */

static inline int get_high_nibble_signed(int n) {
    return ((n&0x70)-(n&0x80))>>4;
}

static inline int get_low_nibble_signed(int n) {
    return (n&7)-(n&8);
}

/* return true for a good sample rate */
int check_sample_rate(int32_t sr);

/* return a file's extension (a pointer to the first character of the
 * extension in the original filename or the ending null byte if no extension
 */
const char * filename_extension(const char * filename);

static inline int16_t clamp16(int32_t val) {
        if (val>32767) return 32767;
            if (val<-32768) return -32768;
                return val;
}

#endif
