#ifndef _READER_SF_H
#define _READER_SF_H
#include "../streamfile.h"
#include "reader_get.h"


/* Sometimes you just need an int, and we're doing the buffering.
* Note, however, that if these fail to read they'll return -1,
* so that should not be a valid value or there should be some backup. */
static inline int16_t read_16bitLE(off_t offset, STREAMFILE* sf) {
    uint8_t buf[2];

    if (read_streamfile(buf,offset,2,sf)!=2) return -1;
    return get_s16le(buf);
}
static inline int16_t read_16bitBE(off_t offset, STREAMFILE* sf) {
    uint8_t buf[2];

    if (read_streamfile(buf,offset,2,sf)!=2) return -1;
    return get_s16be(buf);
}
static inline int32_t read_32bitLE(off_t offset, STREAMFILE* sf) {
    uint8_t buf[4];

    if (read_streamfile(buf,offset,4,sf)!=4) return -1;
    return get_s32le(buf);
}
static inline int32_t read_32bitBE(off_t offset, STREAMFILE* sf) {
    uint8_t buf[4];

    if (read_streamfile(buf,offset,4,sf)!=4) return -1;
    return get_s32be(buf);
}
static inline int64_t read_s64le(off_t offset, STREAMFILE* sf) {
    uint8_t buf[8];

    if (read_streamfile(buf,offset,8,sf)!=8) return -1;
    return get_s64le(buf);
}
static inline uint64_t read_u64le(off_t offset, STREAMFILE* sf) { return (uint64_t)read_s64le(offset, sf); }

static inline int64_t read_s64be(off_t offset, STREAMFILE* sf) {
    uint8_t buf[8];

    if (read_streamfile(buf,offset,8,sf)!=8) return -1;
    return get_s64be(buf);
}
static inline uint64_t read_u64be(off_t offset, STREAMFILE* sf) { return (uint64_t)read_s64be(offset, sf); }

static inline int8_t read_8bit(off_t offset, STREAMFILE* sf) {
    uint8_t buf[1];

    if (read_streamfile(buf,offset,1,sf)!=1) return -1;
    return buf[0];
}

/* alias of the above */
static inline int8_t   read_s8   (off_t offset, STREAMFILE* sf) { return           read_8bit(offset, sf); }
static inline uint8_t  read_u8   (off_t offset, STREAMFILE* sf) { return (uint8_t) read_8bit(offset, sf); }
static inline int16_t  read_s16le(off_t offset, STREAMFILE* sf) { return           read_16bitLE(offset, sf); }
static inline uint16_t read_u16le(off_t offset, STREAMFILE* sf) { return (uint16_t)read_16bitLE(offset, sf); }
static inline int16_t  read_s16be(off_t offset, STREAMFILE* sf) { return           read_16bitBE(offset, sf); }
static inline uint16_t read_u16be(off_t offset, STREAMFILE* sf) { return (uint16_t)read_16bitBE(offset, sf); }
static inline int32_t  read_s32le(off_t offset, STREAMFILE* sf) { return           read_32bitLE(offset, sf); }
static inline uint32_t read_u32le(off_t offset, STREAMFILE* sf) { return (uint32_t)read_32bitLE(offset, sf); }
static inline int32_t  read_s32be(off_t offset, STREAMFILE* sf) { return           read_32bitBE(offset, sf); }
static inline uint32_t read_u32be(off_t offset, STREAMFILE* sf) { return (uint32_t)read_32bitBE(offset, sf); }

static inline float read_f32be(off_t offset, STREAMFILE* sf) {
    uint8_t buf[4];

    if (read_streamfile(buf, offset, sizeof(buf), sf) != sizeof(buf))
        return -1;
    return get_f32be(buf);
}
static inline float read_f32le(off_t offset, STREAMFILE* sf) {
    uint8_t buf[4];

    if (read_streamfile(buf, offset, sizeof(buf), sf) != sizeof(buf))
        return -1;
    return get_f32le(buf);
}

static inline double read_d64be(off_t offset, STREAMFILE* sf) {
    uint8_t buf[8];

    if (read_streamfile(buf, offset, sizeof(buf), sf) != sizeof(buf))
        return -1;
    return get_d64be(buf);
}
#if 0
static inline double read_d64le(off_t offset, STREAMFILE* sf) {
    uint8_t buf[8];

    if (read_streamfile(buf, offset, sizeof(buf), sf) != sizeof(buf))
        return -1;
    return get_d64le(buf);
}
#endif

#if 0
// on GCC, this reader will be correctly optimized out (as long as it's static/inline), would be same as declaring:
// uintXX_t (*read_uXX)(off_t,uint8_t*) = be ? get_uXXbe : get_uXXle;
// only for the functions actually used in code, and inlined if possible (like big_endian param being a constant).
// on MSVC seems all read_X in sf_reader are compiled and included in the translation unit, plus ignores constants
// so may result on bloatness?
// (from godbolt tests, test more real cases)

/* collection of callbacks for quick access */
typedef struct sf_reader {
    int32_t (*read_s32)(off_t,STREAMFILE*); //maybe r.s32
    float (*read_f32)(off_t,STREAMFILE*);
    /* ... */
} sf_reader;

static inline void sf_reader_init(sf_reader* r, int big_endian) {
    memset(r, 0, sizeof(sf_reader));
    if (big_endian) {
        r->read_s32 = read_s32be;
        r->read_f32 = read_f32be;
    }
    else {
        r->read_s32 = read_s32le;
        r->read_f32 = read_f32le;
    }
}

/* sf_reader r;
 * ...
 * sf_reader_init(&r, big_endian);
 * val = r.read_s32; //maybe r.s32?
 */
#endif
#if 0  //todo improve + test + simplify code (maybe not inline?)
static inline int read_s4h(off_t offset, STREAMFILE* sf) {
    uint8_t byte = read_u8(offset, streamfile);
    return get_nibble_signed(byte, 1);
}
static inline int read_u4h(off_t offset, STREAMFILE* sf) {
    uint8_t byte = read_u8(offset, streamfile);
    return (byte >> 4) & 0x0f;
}
static inline int read_s4l(off_t offset, STREAMFILE* sf) {
    ...
}
static inline int read_u4l(off_t offset, STREAMFILE* sf) {
    ...
}
static inline int max_s32(int32_t a, int32_t b) { return a > b ? a : b; }
static inline int min_s32(int32_t a, int32_t b) { return a < b ? a : b; }
//align32, align16, clamp16, etc
#endif

/* fastest to compare would be read_u32x == (uint32), but should be pre-optimized (see get_id32x) */
static inline /*const*/ int is_id32be(off_t offset, STREAMFILE* sf, const char* s) {
    return read_u32be(offset, sf) == get_id32be(s);
}

static inline /*const*/ int is_id32le(off_t offset, STREAMFILE* sf, const char* s) {
    return read_u32le(offset, sf) == get_id32be(s);
}

static inline /*const*/ int is_id64be(off_t offset, STREAMFILE* sf, const char* s) {
    return read_u64be(offset, sf) == get_id64be(s);
}

#endif
