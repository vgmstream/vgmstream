#ifndef _BITSTREAM_MSB_H
#define _BITSTREAM_MSB_H

#include <stdint.h>

/* Simple bitreader for MPEG/standard bit style, in 'most significant byte' (MSB) format.
 * Example: with 0x1234 = 00010010 00110100, reading 5b + 6b = 00010 010001
 *  (first upper 5b, then next lower 3b and next upper 3b = 6b)
 *
 * Kept in .h since it's slightly faster (compiler can optimize statics better using default compile flags).
 * Assumes bufs aren't that big (probable max ~0x20000000)
 */

typedef struct {
    uint8_t* buf;           // buffer to read/write
    uint32_t bufsize;       // max size
    uint32_t _b_max;        // max size in bits
    uint32_t _b_off;        // current offset in bits inside the buffer

    bool error;             // attempted to read/write past max data
} bitstream_t;

/* convenience util */
static inline void bm_setup(bitstream_t* bs, uint8_t* buf, uint32_t bufsize) {
    bs->buf = buf;
    bs->bufsize = bufsize;
    bs->_b_max = bufsize * 8;
    bs->_b_off = 0;
    bs->error = false;
}


static inline int bm_fill(bitstream_t* bs, uint32_t bytes) {
    if (bs->_b_off > bs->_b_max)
        return 0;

    bs->bufsize += bytes;
    bs->_b_max += bytes * 8;

    return 1;
}

static inline int bm_set(bitstream_t* bs, uint32_t b_off) {
    if (bs->_b_off > bs->_b_max)
        return 0;

    bs->_b_off = b_off;

    return 1;
}


static inline int bm_skip(bitstream_t* bs, uint32_t bits) {
    if (bs->_b_off + bits > bs->_b_max) {
        bs->error = true;
        return 0;
    }

    bs->_b_off += bits;

    return 1;
}

#if 0
static inline void bm_align(bitstream_t* bs, uint32_t bits) {
    if (bits == 0)
        return;

    int left = bs->_b_off % bits;
    if (left == 0)
        return;

    bm_skip(bs, bits - left);
}
#endif

static inline int bm_pos(bitstream_t* bs) {
    return bs->_b_off;
}


static const uint32_t MASK_TABLE_MSB[33] = {
        0x00000000, 0x00000001, 0x00000003, 0x00000007, 0x0000000f, 0x0000001f, 0x0000003f, 0x0000007f, 0x000000ff,
        0x000001ff, 0x000003ff, 0x000007ff, 0x00000fff, 0x00001fff, 0x00003fff, 0x00007fff, 0x0000ffff, 0x0001ffff,
        0x0003ffff, 0x0007ffff, 0x000fffff, 0x001fffff, 0x003fffff, 0x007fffff, 0x00ffffff, 0x01ffffff, 0x03ffffff,
        0x07ffffff, 0x0fffffff, 0x1fffffff, 0x3fffffff, 0x7fffffff, 0xffffffff
};

/* Read bits (max 32) from buf and update the bit offset.
 * Order is BE (MSB). */
static inline int bm_get(bitstream_t* ib, uint32_t bits, uint32_t* value) {

    // removing this check (manually validating enough buf) doesn't seem to affect performance much
    if (bits > 32 || bits > ib->_b_max - ib->_b_off) {
        *value = 0;
        ib->error = true;
        return 0;
    }

    if (bits == 0) {
        *value = 0;
        return 1;
    }

    // Simple approach, considering typical case is bits <8
    // Other approaches (u64 local or global cache w/ shift-mask, switch+fallthrough) and micro
    // optimizations (no table) don't seem to improve performance (optimized out or branch prediction?)

    uint32_t pos = ib->_b_off / 8;          // byte offset
    uint32_t shift = ib->_b_off % 8;        // bit sub-offset
    uint32_t mask = MASK_TABLE_MSB[bits];   // to remove upper in highest byte

    uint64_t val = ib->buf[pos+0]; //TODO: could use u32 with some shift fiddling
    int left = 8 - (bits + shift);
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

    *value = val;
    ib->_b_off += bits;
    return 1;
}

static inline uint32_t bm_read(bitstream_t* ib, uint32_t bits) {
    uint32_t value;
    /*int res =*/ bm_get(ib, bits, &value);
    //if (!res)
    //    return 0;
    return value;
}

/* Write bits (max 32) to buf and update the bit offset.
 * Order is BE (MSB). */
static inline int bm_put(bitstream_t* ob, uint32_t bits, uint32_t value) {

    if (bits > 32 || bits > ob->_b_max - ob->_b_off) {
        ob->error = true;
        return 0;
    }

    if (bits == 0) {
        return 1;
    }

    // see bl_get

    uint32_t pos = ob->_b_off / 8;                  // byte offset
    uint32_t shift = ob->_b_off % 8;                // bit sub-offset

    for (int i = 0; i < bits; i++) {
        int bit_val = (1U << (bits-1-i));           // bit check for value
        int bit_buf = (1U << (8-1-shift)) & 0xFF;   // bit to set in buf

        if (value & bit_val)                        // is bit in val set?
            ob->buf[pos] |= bit_buf;                // set bit
        else
            ob->buf[pos] &= ~bit_buf;               // unset bit

        shift++;
        if (shift % 8 == 0) {                       // new byte starts
            shift = 0;
            pos++;
        }
    }

    ob->_b_off += bits;
    return 1;
}

static inline void bm_pad(bitstream_t* bs, uint32_t bits) {
    if (bits == 0)
        return;

    int left = bs->_b_off % bits;
    if (left == 0)
        return;

    int padding = bits - left;
    bm_put(bs, padding, 0);
}

#endif
