#ifndef _VORBIS_BITREADER_H
#define _VORBIS_BITREADER_H

/* Simple bitreader for Vorbis' bit format.
 * Kept in .h since it's slightly faster (compiler can optimize statics better) */


typedef struct {
    uint8_t* buf;           /* buffer to read/write */
    size_t bufsize;         /* max size of the buffer */
    uint32_t b_off;         /* current offset in bits inside the buffer */
} bitstream_t;

/* convenience util */
static void init_bitstream(bitstream_t* b, uint8_t* buf, size_t bufsize) {
    b->buf = buf;
    b->bufsize = bufsize;
    b->b_off = 0;
}

/* same as (1 << bits) - 1, but that seems to trigger some nasty UB when bits = 32
 * (though in theory (1 << 32) = 0, 0 - 1 = UINT_MAX, but gives 0 compiling in some cases, but not always) */
static const uint32_t MASK_TABLE[33] = {
        0x00000000, 0x00000001, 0x00000003, 0x00000007, 0x0000000f, 0x0000001f, 0x0000003f, 0x0000007f, 0x000000ff,
        0x000001ff, 0x000003ff, 0x000007ff, 0x00000fff, 0x00001fff, 0x00003fff, 0x00007fff, 0x0000ffff, 0x0001ffff,
        0x0003ffff, 0x0007ffff, 0x000fffff, 0x001fffff, 0x003fffff, 0x007fffff, 0x00ffffff, 0x01ffffff, 0x03ffffff,
        0x07ffffff, 0x0fffffff, 0x1fffffff, 0x3fffffff, 0x7fffffff, 0xffffffff
};

/* Read bits (max 32) from buf and update the bit offset. Vorbis packs values in LSB order and byte by byte.
 * (ex. from 2 bytes 00100111 00000001 we can could read 4b=0111 and 6b=010010, 6b=remainder (second value is split into the 2nd byte) */
static int rv_bits(bitstream_t* ib, uint32_t bits, uint32_t* value) {
    uint32_t shift, mask, pos, val;

    if (bits > 32 || ib->b_off + bits > ib->bufsize * 8)
        goto fail;

    pos = ib->b_off / 8;        /* byte offset */
    shift = ib->b_off % 8;      /* bit sub-offset */
    mask = MASK_TABLE[bits];    /* to remove upper in highest byte */

    val = ib->buf[pos+0] >> shift;
    if (bits + shift > 8) {
        val |= ib->buf[pos+1] << (8u - shift);
        if (bits + shift > 16) {
            val |= ib->buf[pos+2] << (16u - shift);
            if (bits + shift > 24) {
                val |= ib->buf[pos+3] << (24u - shift);
                if (bits + shift > 32) {
                    val |= ib->buf[pos+4] << (32u - shift); /* upper bits are lost (shifting over 32) */
                }
            }
        }
    }

    *value = (val & mask);

    ib->b_off += bits;

    return 1;
fail:
    VGM_LOG_ONCE("BITREADER: read fail\n");
    *value = 0;
    return 0;
}

#ifndef BITSTREAM_READ_ONLY
/* Write bits (max 32) to buf and update the bit offset. Vorbis packs values in LSB order and byte by byte.
 * (ex. writing 1101011010 from b_off 2 we get 01101011 00001101 (value split, and 11 in the first byte skipped)*/
static int wv_bits(bitstream_t* ob, uint32_t bits, uint32_t value) {
    uint32_t shift, mask, pos;

    if (bits > 32 || ob->b_off + bits > ob->bufsize*8)
        goto fail;

    pos = ob->b_off / 8;        /* byte offset */
    shift = ob->b_off % 8;      /* bit sub-offset */
    mask = (1 << shift) - 1;    /* to keep lower bits in lowest byte */

    ob->buf[pos+0] =  (value << shift) | (ob->buf[pos+0] & mask);
    if (bits + shift > 8) {
        ob->buf[pos+1] = value >> (8 - shift);
        if (bits + shift > 16) {
            ob->buf[pos+2] = value >> (16 - shift);
            if (bits + shift > 24) {
                ob->buf[pos+3] = value >> (24 - shift);
                if (bits + shift > 32) {
                    /* upper bits are set to 0 (shifting unsigned) but shouldn't matter */
                    ob->buf[pos+4] = value >> (32 - shift);
                }
            }
        }
    }

    ob->b_off += bits;
    return 1;
fail:
    VGM_LOG_ONCE("BITREADER: write fail\n");
    return 0;
}
#endif

#endif
