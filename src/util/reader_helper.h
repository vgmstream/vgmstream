#ifndef _READER_HELPER_H
#define _READER_HELPER_H

#include "endianness.h"
#include "reader_sf.h"

// not sure if all compilers can optimize out this struct (MSVC?), mainly for extra complex cases 

// TODO: test if tables with functions are optimized better than function pointers

typedef struct {
    read_s32_t s32;
    read_u32_t u32;
    read_f32_t f32;
    read_s16_t s16;
    read_u16_t u16;
    read_s64_t s64;
    read_u64_t u64;
    uint32_t offset;
    STREAMFILE* sf;
} reader_t;

static void reader_set_endian(reader_t* r, int big_endian) {
    r->s32 = big_endian ? read_s32be : read_s32le;
    r->u32 = big_endian ? read_u32be : read_u32le;
    r->f32 = big_endian ? read_f32be : read_f32le;
    r->s16 = big_endian ? read_s16be : read_s16le;
    r->u16 = big_endian ? read_u16be : read_u16le;
    r->s64 = big_endian ? read_s64be : read_s64le;
    r->u64 = big_endian ? read_u64be : read_u64le;
}

static void reader_setup(reader_t* r, STREAMFILE* sf, uint32_t offset, int big_endian) {
    reader_set_endian(r, big_endian);
    r->sf = sf;
    r->offset = offset;
}

static inline uint64_t reader_u64(reader_t* r) {
    uint64_t v = r->u64(r->offset, r->sf);
    r->offset += 0x08;
    return v;
}

static inline int64_t reader_s64(reader_t* r) {
    int64_t v = r->s64(r->offset, r->sf);
    r->offset += 0x08;
    return v;
}

static inline uint32_t reader_u32(reader_t* r) {
    uint32_t v = r->u32(r->offset, r->sf);
    r->offset += 0x04;
    return v;
}

static inline int32_t reader_s32(reader_t* r) {
    int32_t v = r->s32(r->offset, r->sf);
    r->offset += 0x04;
    return v;
}

static inline uint16_t reader_u16(reader_t* r) {
    uint16_t v = r->u16(r->offset, r->sf);
    r->offset += 0x02;
    return v;
}

static inline int16_t reader_s16(reader_t* r) {
    int16_t v = r->u16(r->offset, r->sf);
    r->offset += 0x02;
    return v;
}

static inline uint8_t reader_u8(reader_t* r) {
    uint8_t v = read_u8(r->offset, r->sf);
    r->offset += 0x01;
    return v;
}

static inline int8_t reader_s8(reader_t* r) {
    int8_t v = read_s8(r->offset, r->sf);
    r->offset += 0x01;
    return v;
}

static inline float reader_f32(reader_t* r) {
    float v = r->f32(r->offset, r->sf);
    r->offset += 0x04;
    return v;
}

static inline void reader_x32(reader_t* r) {
    r->offset += 0x04;
}

static inline void reader_skip(reader_t* r, uint32_t skip) {
    r->offset += skip;
}

#endif
