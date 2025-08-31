#ifndef _BITSTREAM_MSB_H
#define _BITSTREAM_MSB_H

#include <stdint.h>

/* Simple bitreader for MPEG/standard bit style, in 'most significant byte' (MSB) format.
 * Example: with 0x1234 = 00010010 00110100, reading 5b + 6b = 00010 010001
 *  (first upper 5b, then next lower 3b and next upper 3b = 6b)
 * Kept in .h since it's slightly faster (compiler can optimize statics better using default compile flags).
 * Assumes bufs aren't that big (probable max ~0x20000000)
 */

typedef struct {
    uint8_t* buf;           // buffer to read/write
    uint32_t bufsize;       // max size
    uint32_t b_max;         // max size in bits
    uint32_t b_off;         // current offset in bits inside buffer
} bitstream_t;

/* convenience util */
static inline void bm_setup(bitstream_t* bs, uint8_t* buf, uint32_t bufsize) {
    bs->buf = buf;
    bs->bufsize = bufsize;
    bs->b_max = bufsize * 8;
    bs->b_off = 0;
}

static inline int bm_set(bitstream_t* bs, uint32_t b_off) {
    if (bs->b_off > bs->b_max)
        return 0;

    bs->b_off = b_off;

    return 1;
}

static inline int bm_fill(bitstream_t* bs, uint32_t bytes) {
    if (bs->b_off > bs->b_max)
        return 0;

    bs->bufsize += bytes;
    bs->b_max += bytes * 8;

    return 1;
}

static inline int bm_skip(bitstream_t* bs, uint32_t bits) {
    if (bs->b_off + bits > bs->b_max)
        return 0;

    bs->b_off += bits;

    return 1;
}

static inline int bm_pos(bitstream_t* bs) {
    return bs->b_off;
}

/* same as (1 << bits) - 1, but that seems to trigger some nasty UB when bits = 32
 * (though in theory (1 << 32) = 0, 0 - 1 = UINT_MAX, but gives 0 compiling in some cases, but not always) */
static const uint32_t MASK_TABLE_MSB[33] = {
        0x00000000, 0x00000001, 0x00000003, 0x00000007, 0x0000000f, 0x0000001f, 0x0000003f, 0x0000007f, 0x000000ff,
        0x000001ff, 0x000003ff, 0x000007ff, 0x00000fff, 0x00001fff, 0x00003fff, 0x00007fff, 0x0000ffff, 0x0001ffff,
        0x0003ffff, 0x0007ffff, 0x000fffff, 0x001fffff, 0x003fffff, 0x007fffff, 0x00ffffff, 0x01ffffff, 0x03ffffff,
        0x07ffffff, 0x0fffffff, 0x1fffffff, 0x3fffffff, 0x7fffffff, 0xffffffff
};

/* Read bits (max 32) from buf and update the bit offset. Order is BE (MSB). */
static inline int bm_get(bitstream_t* ib, uint32_t bits, uint32_t* value) {
    uint32_t shift, pos, mask;
    uint64_t val; //TODO: could use u32 with some shift fiddling
    int left;

    if (bits > 32 || ib->b_off + bits > ib->b_max) {
        *value = 0;
        return 0;
    }

    pos = ib->b_off / 8;                        // byte offset
    shift = ib->b_off % 8;                      // bit sub-offset

#if 0 //naive approach
    int bit_val, bit_buf;

    val = 0;
    for (int i = 0; i < bits; i++) {
        bit_buf = (1U << (8-1-shift)) & 0xFF;   // bit check for buf
        bit_val = (1U << (bits-1-i));           // bit to set in value

        if (ib->buf[pos] & bit_buf)             // is bit in buf set?
            val |= bit_val;                     // set bit

        shift++;
        if (shift % 8 == 0) {                   // new byte starts
            shift = 0;
            pos++;
        }
    }
#else
    mask = MASK_TABLE_MSB[bits];    // to remove upper in highest byte

    left = 0;
    if (bits == 0)
        val = 0;
    else 
        val = ib->buf[pos+0];
    left = 8 - (bits + shift);
    if (bits + shift > 8) {
        val = (val << 8u) | ib->buf[pos+1];
        left = 16 - (bits + shift);
        if (bits + shift > 16) {
            val = (val << 8u) | ib->buf[pos+2];
            left = 24 - (bits + shift);
            if (bits + shift > 24) {
                val = (val << 8u) | ib->buf[pos+3];
                left = 32 - (bits + shift);
                if (bits + shift > 32) {
                    val = (val << 8u) | ib->buf[pos+4];
                    left = 40 - (bits + shift);
                }
            }
        }
    }
    val = ((val >> left) & mask);
#endif

    *value = val;
    ib->b_off += bits;
    return 1;
}

static inline uint32_t bm_read(bitstream_t* ib, uint32_t bits) {
    uint32_t value;
    int res = bm_get(ib, bits, &value);
    if (!res)
        return 0;
    return value;
}

/* Write bits (max 32) to buf and update the bit offset. Order is BE (MSB). */
static inline int bm_put(bitstream_t* ob, uint32_t bits, uint32_t value) {
    uint32_t shift, pos;
    int i, bit_val, bit_buf;

    if (bits > 32 || ob->b_off + bits > ob->b_max)
        return 0;

    pos = ob->b_off / 8;                        // byte offset
    shift = ob->b_off % 8;                      // bit sub-offset

    for (i = 0; i < bits; i++) {
        bit_val = (1U << (bits-1-i));           // bit check for value
        bit_buf = (1U << (8-1-shift)) & 0xFF;   // bit to set in buf

        if (value & bit_val)                    // is bit in val set?
            ob->buf[pos] |= bit_buf;            // set bit
        else
            ob->buf[pos] &= ~bit_buf;           // unset bit

        shift++;
        if (shift % 8 == 0) {                   // new byte starts
            shift = 0;
            pos++;
        }
    }

    ob->b_off += bits;
    return 1;
}

#endif
