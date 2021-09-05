#ifndef _XNB_LZ4MG_H_
#define _XNB_LZ4MG_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* Decompresses LZ4 from MonoGame. The original C lib has a lot of modes and configs, but
 * MonoGame only uses the core 'block' part, which is a fairly simple LZ77 (has one command
 * to copy literal and window values, with variable copy lengths).
 *
 * This is a basic re-implementation (not tuned for performance) for practice/test purposes,
 * that handles streaming decompression as a state machine since we can run out of src or dst
 * bytes anytime and LZ4 allows any copy length, with copy window as a circular buffer. Not
 * sure what's the best/standard way to do it though. Info:
 * - https://github.com/lz4/lz4
 * - https://github.com/MonoGame/MonoGame/blob/develop/MonoGame.Framework/Utilities/Lz4Stream/Lz4DecoderStream.cs
 */

#define LZ4MG_OK                0
#define LZ4MG_ERROR             -1
#define LZ4MG_WINDOW_SIZE       (1 << 16)
#define LZ4MG_WINDOW_BYTES      2
#define LZ4MG_MIN_MATCH_LEN     4
#define LZ4MG_VARLEN_MARK       15
#define LZ4MG_VARLEN_CONTINUE   255


typedef enum {
    READ_TOKEN,
    READ_LITERAL,
    COPY_LITERAL,
    READ_OFFSET,
    READ_MATCH,
    SET_MATCH,
    COPY_MATCH
} lz4mg_state_t;

typedef struct {
    lz4mg_state_t state;

    uint8_t token;
    int literal_len;
    int offset_cur;
    int offset_pos;
    int match_len;
    int match_pos;

    int window_pos;
    uint8_t window[LZ4MG_WINDOW_SIZE];
} lz4mg_context_t;

typedef struct {
    lz4mg_context_t ctx;

    uint8_t *next_out;      /* next bytes to write (reassign when avail is 0) */
    int avail_out;          /* bytes available at next_out */
    int total_out;          /* written bytes, for reference (set to 0 per call if needed) */

    const uint8_t *next_in; /* next bytes to read (reassign when avail is 0) */
    int avail_in;           /* bytes available at next_in */
    int total_in;           /* read bytes, for reference (set to 0 per call if needed) */
} lz4mg_stream_t;

static void lz4mg_reset(lz4mg_stream_t* strm) {
    memset(strm, 0, sizeof(lz4mg_stream_t));
}

/* Decompress src into dst, returning a code and number of bytes used. Caller must handle
 * stop (when no more input data or all data has been decompressed) as LZ4 has no end markers. */
static int lz4mg_decompress(lz4mg_stream_t* strm) {
    lz4mg_context_t* ctx = &strm->ctx;
    uint8_t* dst = strm->next_out;
    const uint8_t* src = strm->next_in;
    int dst_size = strm->avail_out;
    int src_size = strm->avail_in;
    int dst_pos = 0;
    int src_pos = 0;
    uint8_t next_len, next_val;


    while (1) {
        /* mostly linear state machine, but it may break anytime when reaching dst or src
         * end, and resume from same state in next call */
        switch(ctx->state) {

            case READ_TOKEN:
                if (src_pos >= src_size)
                    goto buffer_end;
                ctx->token = src[src_pos++];

                ctx->literal_len = (ctx->token >> 4) & 0xF;
                if (ctx->literal_len == LZ4MG_VARLEN_MARK)
                    ctx->state = READ_LITERAL;
                else
                    ctx->state = COPY_LITERAL;
                break;

            case READ_LITERAL:
                do {
                    if (src_pos >= src_size)
                        goto buffer_end;
                    next_len = src[src_pos++];
                    ctx->literal_len += next_len;
                } while (next_len == LZ4MG_VARLEN_CONTINUE);

                ctx->state = COPY_LITERAL;
                break;

            case COPY_LITERAL:
                while (ctx->literal_len > 0) { /* may be 0 */
                    if (src_pos >= src_size || dst_pos >= dst_size)
                        goto buffer_end;
                    next_val = src[src_pos++];

                    dst[dst_pos++] = next_val;

                    ctx->window[ctx->window_pos++] = next_val;
                    if (ctx->window_pos == LZ4MG_WINDOW_SIZE)
                        ctx->window_pos = 0;

                    ctx->literal_len--;
                };

                /* LZ4 is designed to reach EOF with a literal in this state with some empty values */

                ctx->offset_cur = 0;
                ctx->offset_pos = 0;
                ctx->state = READ_OFFSET;
                break;

            case READ_OFFSET:
                do {
                    if (src_pos >= src_size)
                        goto buffer_end;
                    ctx->offset_pos |= (src[src_pos++] << ctx->offset_cur*8);
                    ctx->offset_cur++;
                } while (ctx->offset_cur < LZ4MG_WINDOW_BYTES);

                ctx->match_len = (ctx->token & 0xF);
                if (ctx->match_len == LZ4MG_VARLEN_MARK)
                    ctx->state = READ_MATCH;
                else
                    ctx->state = SET_MATCH;
                break;

            case READ_MATCH:
                do {
                    if (src_pos >= src_size)
                        goto buffer_end;
                    next_len = src[src_pos++];
                    ctx->match_len += next_len;
                } while (next_len == LZ4MG_VARLEN_CONTINUE);

                ctx->state = SET_MATCH;
                break;

            case SET_MATCH:
                ctx->match_len += LZ4MG_MIN_MATCH_LEN;

                ctx->match_pos = ctx->window_pos - ctx->offset_pos;
                if (ctx->match_pos < 0) /* circular buffer so negative is from window end */
                    ctx->match_pos = LZ4MG_WINDOW_SIZE + ctx->match_pos;

                ctx->state = COPY_MATCH;
                break;

            case COPY_MATCH:
                 while (ctx->match_len > 0) {
                    if (dst_pos >= dst_size)
                        goto buffer_end;

                    next_val = ctx->window[ctx->match_pos++];
                    if (ctx->match_pos == LZ4MG_WINDOW_SIZE)
                        ctx->match_pos = 0;

                    dst[dst_pos++] = next_val;

                    ctx->window[ctx->window_pos++] = next_val;
                    if (ctx->window_pos == LZ4MG_WINDOW_SIZE)
                        ctx->window_pos = 0;

                    ctx->match_len--;
                };

                ctx->state = READ_TOKEN;
                break;

            default:
                goto fail;
        }
    }

buffer_end:
    strm->next_out  += dst_pos;
    strm->next_in   += src_pos;
    strm->avail_out -= dst_pos;
    strm->avail_in  -= src_pos;
    strm->total_out += dst_pos;
    strm->total_in  += src_pos;

    return LZ4MG_OK;
fail:
    return LZ4MG_ERROR;
}

#if 0
/* non-streamed form for reference, assumes buffers are big enough */
static void decompress_lz4mg(uint8_t* dst, size_t dst_size, const uint8_t* src, size_t src_size) {
    size_t src_pos = 0;
    size_t dst_pos = 0;
    uint8_t token;
    int literal_len, match_len, next_len;
    int match_pos, match_offset;
    int i;

    while (src_pos < src_size && dst_pos < dst_size) {

        token = src[src_pos++];
        if (src_pos > src_size)
            break;

        /* handle literals */
        literal_len = token >> 4;
        if (literal_len == 15) {
            do {
                next_len = src[src_pos++];
                literal_len += next_len;
                if (src_pos > src_size)
                    break;
            } while (next_len == 255);
        }

        for (i = 0; i < literal_len; i++) {
            dst[dst_pos++] = src[src_pos++];
        }

        /* can happen at EOF */
        if (dst_pos >= dst_size)
            break;

        /* handle window matches */
        match_offset  = src[src_pos++];
        match_offset |= (src[src_pos++] << 8);

        match_len = (token & 0xF);
        if (match_len == 15) {
            do {
                next_len = src[src_pos++];
                match_len += next_len;
                if (src_pos > src_size)
                    break;
            } while (next_len == 255);
        }
        match_len += 4; /* min len */

        match_pos = dst_pos - match_offset;
        for(i = 0; i < match_len; i++) {
            dst[dst_pos++] = dst[match_pos++]; /* note RLE with short offsets */
        }
    }
}
#endif

#endif /* _XNB_LZ4MG_H_ */
