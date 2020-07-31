#ifndef _MPEG_BITREADER_H
#define _MPEG_BITREADER_H

/* Simple bitreader for MPEG/standard bit format.
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

/* Read bits (max 32) from buf and update the bit offset. Order is BE (MSF). */
static int rb_bits(bitstream_t* ib, uint32_t bits, uint32_t* value) {
    uint32_t shift, pos, val;
    int i, bit_buf, bit_val;

    if (bits > 32 || ib->b_off + bits > ib->bufsize * 8)
        goto fail;

    pos = ib->b_off / 8;        /* byte offset */
    shift = ib->b_off % 8;      /* bit sub-offset */

    val = 0;
    for (i = 0; i < bits; i++) {
        bit_buf = (1U << (8-1-shift)) & 0xFF;   /* bit check for buf */
        bit_val = (1U << (bits-1-i));           /* bit to set in value */

        if (ib->buf[pos] & bit_buf)             /* is bit in buf set? */
            val |= bit_val;                     /* set bit */

        shift++;
        if (shift % 8 == 0) {                   /* new byte starts */
            shift = 0;
            pos++;
        }
    }

    *value = val;
    ib->b_off += bits;
    return 1;
fail:
    VGM_LOG("BITREADER: read fail\n");
    *value = 0;
    return 0;
}

#ifndef BITSTREAM_READ_ONLY
/* Write bits (max 32) to buf and update the bit offset. Order is BE (MSF). */
static int wb_bits(bitstream_t* ob, uint32_t bits, uint32_t value) {
    uint32_t shift, pos;
    int i, bit_val, bit_buf;

    if (bits > 32 || ob->b_off + bits > ob->bufsize * 8)
        goto fail;

    pos = ob->b_off / 8; /* byte offset */
    shift = ob->b_off % 8; /* bit sub-offset */

    for (i = 0; i < bits; i++) {
        bit_val = (1U << (bits-1-i));     /* bit check for value */
        bit_buf = (1U << (8-1-shift)) & 0xFF;   /* bit to set in buf */

        if (value & bit_val)                /* is bit in val set? */
            ob->buf[pos] |= bit_buf;        /* set bit */
        else
            ob->buf[pos] &= ~bit_buf;       /* unset bit */

        shift++;
        if (shift % 8 == 0) {               /* new byte starts */
            shift = 0;
            pos++;
        }
    }

    ob->b_off += bits;
    return 1;
fail:
    return 0;
}
#endif

#endif
