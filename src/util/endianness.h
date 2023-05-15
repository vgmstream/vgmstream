#ifndef _UTIL_ENDIAN_H
#define _UTIL_ENDIAN_H

#include "../streamfile.h"
#include "reader_get.h"

typedef uint32_t (*read_u32_t)(off_t, STREAMFILE*);
typedef  int32_t (*read_s32_t)(off_t, STREAMFILE*);
typedef uint16_t (*read_u16_t)(off_t, STREAMFILE*);
typedef  int16_t (*read_s16_t)(off_t, STREAMFILE*);
typedef float (*read_f32_t)(off_t, STREAMFILE*);

typedef  int16_t (*get_s16_t)(const uint8_t*);

/* guess byte endianness from a given value, return true if big endian and false if little endian */
static inline int guess_endian16(off_t offset, STREAMFILE* sf) {
    uint8_t buf[0x02];
    if (read_streamfile(buf, offset, 0x02, sf) != 0x02) return -1; /* ? */
    return get_u16le(buf) > get_u16be(buf) ? 1 : 0;
}

static inline int guess_endian32(off_t offset, STREAMFILE* sf) {
    uint8_t buf[0x04];
    if (read_streamfile(buf, offset, 0x04, sf) != 0x04) return -1; /* ? */
    return get_u32le(buf) > get_u32be(buf) ? 1 : 0;
}

#endif
