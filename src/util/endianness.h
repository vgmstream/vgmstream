#ifndef _ENDIANNESS_H
#define _ENDIANNESS_H

#include "../streamfile.h"
#include "reader_get.h"
#include "reader_sf.h"

typedef uint64_t (*read_u64_t)(off_t, STREAMFILE*);
typedef  int64_t (*read_s64_t)(off_t, STREAMFILE*);
typedef uint32_t (*read_u32_t)(off_t, STREAMFILE*);
typedef  int32_t (*read_s32_t)(off_t, STREAMFILE*);
typedef uint16_t (*read_u16_t)(off_t, STREAMFILE*);
typedef  int16_t (*read_s16_t)(off_t, STREAMFILE*);
typedef float (*read_f32_t)(off_t, STREAMFILE*);

typedef  uint16_t (*get_u16_t)(const uint8_t*);
typedef   int16_t (*get_s16_t)(const uint8_t*);
typedef  uint32_t (*get_u32_t)(const uint8_t*);
typedef   int32_t (*get_s32_t)(const uint8_t*);


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

static inline read_u32_t guess_read_u32(off_t offset, STREAMFILE* sf) {
    return guess_endian32(offset,sf) ? read_u32be : read_u32le;
}

static inline read_u32_t get_read_u32(bool big_endian) {
    return big_endian ? read_u32be : read_u32le;
}

static inline read_s32_t get_read_s32(bool big_endian) {
    return big_endian ? read_s32be : read_s32le;
}

static inline read_u16_t get_read_u16(bool big_endian) {
    return big_endian ? read_u16be : read_u16le;
}

static inline read_s16_t get_read_s16(bool big_endian) {
    return big_endian ? read_s16be : read_s16le;
}

static inline read_f32_t get_read_f32(bool big_endian) {
    return big_endian ? read_f32be : read_f32le;
}

#endif
