#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* Decompresses Circus's custom LZ used in XPCM as a machine state for streaming,
 * that may break during any step. Original code decompress at once the full thing
 * into memory so it's simpler. */

#define LZXPCM_OK               0
#define LZXPCM_ERROR            -1
#define LZXPCM_WINDOW_SIZE      (1 << 16)


typedef enum {
    READ_FLAGS,
    COPY_LITERAL,
    READ_TOKEN,
    PARSE_TOKEN,
    SET_MATCH,
    COPY_MATCH
} lzxpcm_state_t;

typedef struct {
    lzxpcm_state_t state;

    uint32_t flags;
    uint8_t token;
    int values_pos;
    int offset_pos;
    int match_len;
    int match_pos;

    int window_pos;
    uint8_t window[LZXPCM_WINDOW_SIZE];
} lzxpcm_context_t;

typedef struct {
    lzxpcm_context_t ctx;

    uint8_t *next_out;      /* next bytes to write (reassign when avail is 0) */
    int avail_out;          /* bytes available at next_out */
    int total_out;          /* written bytes, for reference (set to 0 per call if needed) */

    const uint8_t *next_in; /* next bytes to read (reassign when avail is 0) */
    int avail_in;           /* bytes available at next_in */
    int total_in;           /* read bytes, for reference (set to 0 per call if needed) */
} lzxpcm_stream_t;


static void lzxpcm_reset(lzxpcm_stream_t* strm) {
    memset(strm, 0, sizeof(lzxpcm_stream_t));
}

/* Decompress src into dst, returning a code and number of bytes used. Caller must handle
 * stop (when no more input data or all data has been decompressed) as LZXPCM has no end marker. */
static int lzxpcm_decompress(lzxpcm_stream_t* strm) {
    lzxpcm_context_t* ctx = &strm->ctx;
    uint8_t* dst = strm->next_out;
    const uint8_t* src = strm->next_in;
    int dst_size = strm->avail_out;
    int src_size = strm->avail_in;
    int dst_pos = 0;
    int src_pos = 0;
    uint8_t next_val;


    while (1) {
        /* mostly linear state machine, but it may break anytime when reaching dst or src
         * end, and resume from same state in next call */
        switch(ctx->state) {

            case READ_FLAGS:
                if (src_pos >= src_size)
                    goto buffer_end;

                ctx->flags >>= 1;

                if ((ctx->flags & 0x0100) == 0) {
                    ctx->flags = 0xFF00 | src[src_pos++];
                }

                if (ctx->flags & 1)
                    ctx->state = COPY_LITERAL;
                else
                    ctx->state = READ_TOKEN;
                break;

            case COPY_LITERAL:
                if (src_pos >= src_size || dst_pos >= dst_size)
                    goto buffer_end;
                next_val = src[src_pos++];

                dst[dst_pos++] = next_val;

                ctx->window[ctx->window_pos++] = next_val;
                if (ctx->window_pos == LZXPCM_WINDOW_SIZE)
                    ctx->window_pos = 0;

                ctx->state = READ_FLAGS;
                break;

            case READ_TOKEN:
                if (src_pos >= src_size)
                    goto buffer_end;
                ctx->token = src[src_pos++];

                ctx->values_pos = 0;

                ctx->state = PARSE_TOKEN;
                break;

            case PARSE_TOKEN:
                if (ctx->token >= 0xC0) {
                    ctx->match_len  = ((ctx->token >> 2) & 0x0F) + 4; /* 6b */

                    if (src_pos >= src_size)
                        goto buffer_end;
                    ctx->offset_pos  = src[src_pos++]; /* upper 2b + lower 8b */
                    ctx->offset_pos |= ((ctx->token & 3) << 8);

                }
                else if (ctx->token >= 0x80) {
                    ctx->match_len  = ((ctx->token >> 5) & 3) + 2; /* 2b */

                    ctx->offset_pos  = ctx->token & 0x1F; /* 5b */
                    if (ctx->offset_pos == 0) {
                        if (src_pos >= src_size)
                            goto buffer_end;
                        ctx->offset_pos = src[src_pos++];
                    }
                }
                else if (ctx->token == 0x7F) {
                    if (ctx->values_pos == 0) {
                        if (src_pos >= src_size)
                            goto buffer_end;
                        ctx->match_len  = (src[src_pos++] << 0u);
                        ctx->values_pos++;
                    }

                    if (ctx->values_pos == 1) {
                        if (src_pos >= src_size)
                            goto buffer_end;
                        ctx->match_len |= (src[src_pos++] << 8u);
                        ctx->match_len += 2;
                        ctx->values_pos++;
                    }
                    
                    if (ctx->values_pos == 2) {
                        if (src_pos >= src_size)
                            goto buffer_end;
                        ctx->offset_pos  = (src[src_pos++] << 0u);
                        ctx->values_pos++;
                    }

                    if (ctx->values_pos == 3) {
                        if (src_pos >= src_size)
                            goto buffer_end;
                        ctx->offset_pos |= (src[src_pos++] << 8u);
                        ctx->values_pos++;
                    }
                }
                else {
                    ctx->match_len = ctx->token + 4;

                    if (ctx->values_pos == 0) {
                        if (src_pos >= src_size)
                            goto buffer_end;
                        ctx->offset_pos  = (src[src_pos++] << 0u);
                        ctx->values_pos++;
                    }

                    if (ctx->values_pos == 1) {
                        if (src_pos >= src_size)
                            goto buffer_end;
                        ctx->offset_pos |= (src[src_pos++] << 8u);
                        ctx->values_pos++;
                    }
                }

                ctx->state = SET_MATCH;
                break;

            case SET_MATCH: 
                ctx->match_pos = ctx->window_pos - ctx->offset_pos;
                if (ctx->match_pos < 0) /* circular buffer so negative is from window end */
                    ctx->match_pos = LZXPCM_WINDOW_SIZE + ctx->match_pos;

                ctx->state = COPY_MATCH;
                break;

            case COPY_MATCH: 
                 while (ctx->match_len > 0) {
                    if (dst_pos >= dst_size)
                        goto buffer_end;

                    next_val = ctx->window[ctx->match_pos++];
                    if (ctx->match_pos == LZXPCM_WINDOW_SIZE)
                        ctx->match_pos = 0;

                    dst[dst_pos++] = next_val;

                    ctx->window[ctx->window_pos++] = next_val;
                    if (ctx->window_pos == LZXPCM_WINDOW_SIZE)
                        ctx->window_pos = 0;

                    ctx->match_len--;
                };

                ctx->state = READ_FLAGS;
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

    return LZXPCM_OK;
fail:
    return LZXPCM_ERROR;
}


#if 0
/* non-streamed form that XPCM originally uses, assumes buffers are big enough */
static int lzxpcm_decompress_full(uint8_t* dst, size_t dst_size, const uint8_t* src, size_t src_size) {
	int src_pos = 0;
	int dst_pos = 0;
	uint32_t flags = 0;


	while (src_pos < src_size && dst_pos < dst_size) {
		flags >>= 1;

		if ((flags & 0x0100) == 0) {
			flags = 0xFF00 | src[src_pos++];
		}
		
		if (flags & 1) {
            /* uncompressed byte per bit */
			dst[dst_pos++] = src[src_pos++];
		}
		else { 
            /* compressed data */
			uint32_t length;
			uint32_t offset;
			const uint32_t token = src[src_pos++];

			if (token >= 0xC0) {
				length = ((token >> 2) & 0x0F) + 4; /* 6b */

				offset = ((token & 3) << 8) | src[src_pos++]; /* upper 2b + lower 8b */
			}
			else if (token >= 0x80) {
				length = ((token >> 5) & 3) + 2; /* 2b */

				offset = token & 0x1F; /* 5b */
				if (offset == 0) {
					offset = src[src_pos++];
				}
			}
			else if (token == 0x7F) {
				length = (uint16_t)(src[src_pos] | src[src_pos+1] << 8u) + 2;
				src_pos += 2;

				offset = (uint16_t)(src[src_pos] | src[src_pos+1] << 8u);
				src_pos += 2;
			}
			else {
				length = token + 4;

				offset = (uint16_t)(src[src_pos] | src[src_pos+1] << 8u);
				src_pos += 2;
			}

			if (dst_pos + length > dst_size) {
				length = dst_size - dst_pos;
			}

			for (int i = 0; i < length; i++) {
				dst[dst_pos] = dst[dst_pos - offset];
				dst_pos++;
			}
		}
	}

    return 0;
}
#endif
