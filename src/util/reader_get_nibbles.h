#ifndef _READER_GET_NIBBLES_H
#define _READER_GET_NIBBLES_H

#include "../streamtypes.h"

/* signed nibbles come up a lot in decoders */

static int nibble_to_int[16] = {0,1,2,3,4,5,6,7,-8,-7,-6,-5,-4,-3,-2,-1};

static inline int get_nibble_signed(uint8_t n, int upper) {
    /*return ((n&0x70)-(n&0x80))>>4;*/
    return nibble_to_int[(n >> (upper?4:0)) & 0x0f];
}

static inline int get_high_nibble_signed(uint8_t n) {
    /*return ((n&0x70)-(n&0x80))>>4;*/
    return nibble_to_int[n>>4];
}

static inline int get_low_nibble_signed(uint8_t n) {
    /*return (n&7)-(n&8);*/
    return nibble_to_int[n&0xf];
}

#endif
