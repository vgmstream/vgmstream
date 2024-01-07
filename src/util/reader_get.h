#ifndef _READER_GET_H
#define _READER_GET_H

#include "../streamtypes.h"

/* very common functions, so static (inline) in .h as compiler can optimize to avoid some call overhead */

/* host endian independent multi-byte integer reading */

static inline  int8_t  get_s8   (const uint8_t* p) { return ( int8_t)p[0]; }
static inline uint8_t  get_u8   (const uint8_t* p) { return (uint8_t)p[0]; }

static inline int16_t get_s16be(const uint8_t* p) {
    return ((uint16_t)p[0]<<8) | ((uint16_t)p[1]);
}
static inline uint16_t get_u16be(const uint8_t* p) { return (uint16_t)get_s16be(p); }

static inline int16_t get_s16le(const uint8_t* p) {
    return ((uint16_t)p[0]) | ((uint16_t)p[1]<<8);
}
static inline uint16_t get_u16le(const uint8_t* p) { return (uint16_t)get_s16le(p); }

static inline int32_t get_s32be(const uint8_t* p) {
    return ((uint32_t)p[0]<<24) | ((uint32_t)p[1]<<16) | ((uint32_t)p[2]<<8) | ((uint32_t)p[3]);
}
static inline uint32_t get_u32be(const uint8_t* p) { return (uint32_t)get_s32be(p); }

static inline int32_t get_s32le(const uint8_t* p) {
    return ((uint32_t)p[0]) | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
static inline uint32_t get_u32le(const uint8_t* p) { return (uint32_t)get_s32le(p); }

static inline int64_t get_s64be(const uint8_t* p) {
    return (uint64_t)(((uint64_t)p[0]<<56) | ((uint64_t)p[1]<<48) | ((uint64_t)p[2]<<40) | ((uint64_t)p[3]<<32) | ((uint64_t)p[4]<<24) | ((uint64_t)p[5]<<16) | ((uint64_t)p[6]<<8) | ((uint64_t)p[7]));
}
static inline uint64_t get_u64be(const uint8_t* p) { return (uint64_t)get_s64be(p); }

static inline int64_t get_s64le(const uint8_t* p) {
    return (uint64_t)(((uint64_t)p[0]) | ((uint64_t)p[1]<<8) | ((uint64_t)p[2]<<16) | ((uint64_t)p[3]<<24) | ((uint64_t)p[4]<<32) | ((uint64_t)p[5]<<40) | ((uint64_t)p[6]<<48) | ((uint64_t)p[7]<<56));
}
static inline uint64_t get_u64le(const uint8_t* p) { return (uint64_t)get_s64le(p); }


/* The recommended int-to-float type punning in C is through union, but pointer casting
 * works too (though less portable due to aliasing rules?). For C++ memcpy seems
 * recommended. Both work in GCC and VS2015+ (not sure about older, ifdef as needed). */
static inline float get_f32be(const uint8_t* p) {
    union {
        uint32_t u32;
        float f32;
    } temp;
    temp.u32 = get_u32be(p);
    return temp.f32;
}

static inline float get_f32le(const uint8_t* p) {
    union {
        uint32_t u32;
        float f32;
    } temp;
    temp.u32 = get_u32le(p);
    return temp.f32;
}

static inline double get_d64be(const uint8_t* p) {
    union {
        uint64_t u64;
        double d64;
    } temp;
    temp.u64 = get_u64be(p);
    return temp.d64;
}

static inline double get_d64le(const uint8_t* p) {
    union {
        uint64_t u64;
        double d64;
    } temp;
    temp.u64 = get_u64le(p);
    return temp.d64;
}

#if 0
static inline float    get_f32be_cast(const uint8_t* p) {
    uint32_t sample_int = get_u32be(p);
    float* sample_float = (float*)&sample_int;
    return *sample_float;
}
static inline float    get_f32be_mcpy(const uint8_t* p) {
    uint32_t sample_int = get_u32be(p);
    float sample_float;
    memcpy(&sample_float, &sample_int, sizeof(uint32_t));
    return sample_float;
}
#endif


#endif
