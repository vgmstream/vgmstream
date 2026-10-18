/* Decodes IS22 (Interleaved Siren22) a.k.a ITU-T G.719 (standardization of Polycom's Siren 22).
 *
 * Namco's version (decompiled from various exes) is very similar to the reference decoder:
 * - uses the float point ref implementation (ref. also has a fixed point version)
 * - almost untouched tables, functions, structs (same ABI with no removals/reordering), etc
 * - some functions are inlined, which suggests ref's separate .c files were partially fused together
 * - uses a hacky (C++?) bitreader, while ref. converts the bitstream to a G.192 "bit array"
 *   - functions only change 'short** bitstream' args to the new bitreader, which still read bit by bit
 *   - ref.'s functions read the bitstream but don't advance it. OG replicates it by reading normally (not peeking)
 *     then seeking back to base position at the end of the function (bitstream is moved by the caller)
 * - changes most stack arrays to a scratch pad/arena allocator ('alloc'/'free' ops are pointer +/- ops)
 *   - data + data_size is passed to decode_frame by args, but then 'passed' to other fns via globals (thread unsafe?)
 *   - arrays are alloc's when needed, rather than at the beginning of each function
 * - added encrypted mode (exactly like IS14)
 * - removed 'bad reads' handling (like IS14), where the codec is supposed to repeat old coeffs in those cases
 * - minor details from compiler optimizations: for 0 <> memset, short flag <> u8 flag, short i <> i64, SIMD, etc
 * - removed encoder parts
 *
 * Similar to Namco's G722.1 (IS14) but less changed overall. This decoder follows Namco's bitstream but the arena alloc,
 * in practice being a cleaned up ref decoder. A few potential UB/overflow issues were fixed too.
 */

// Reference decoder: https://www.itu.int/itu-t/recommendations/rec.aspx?rec=9688
/*--------------------------------------------------------------------------*/
/* ITU-T G.722.1 Fullband Extension Annex X. Source Code                    */ // [sic]
/* (c) 2008 Ericsson AB. and Polycom Inc.                                   */
/* All rights reserved.                                                     */
/*--------------------------------------------------------------------------*/

/* Decoding info:
 * - read frame flags
 * - unpack scalefactors ('norms')
 * - calculate QU bit resolution per band (bitalloc)
 * - unpack coef indexes per band
 * - dequantize spectrum coefs
 * - fill missing coefs with noise
 * - deinterleave sub-blocks (in some cases)
 * - transform (IMDCT) + overlap prev frames
*/

#include <stdlib.h>
#include "g719_dec.h"
#include "g719_internal.h"

//-------------------------------------------------------------------------------------------------
// BITREADER

/* Bitreader somewhat similar to OG, but not exactly (here it checks max src). Reads in LSB order byte by byte.
 * OG's vtable also has some helpers (free, write, bitpos, reset...) that aren't needed here.
 * Normally you'd want this inline'd but it's a separate vtable in OG.
 */

void bg_init(bitstream_t* is, const unsigned char* src, int src_size) {
    if (src == NULL || src_size < 0)
        src_size = 0;

    is->src = src;
    is->src_size = src_size;
    is->curr = src;
    is->bitpos = 0;
    is->error = 0;
}

static int bg_left(bitstream_t* is) {
    return is->src_size * 8 - bg_tell(is);
}

int bg_tell(bitstream_t* is) {
    return (int)(is->curr - is->src) * 8 + is->bitpos;
}

void bg_seek(bitstream_t* is, int pos) {
    int max = is->src_size * 8;

    if (pos < 0 || pos > max) {
        is->error = 1;
        pos = (pos < 0) ? 0 : max;
    }

    is->curr = is->src + (pos >> 3);
    is->bitpos = pos & 7;
}

void bg_skip(bitstream_t* is, int bits) {
    int pos = bg_tell(is);

    // OG allows negative skip, not sure if used
    if ((bits > 0 && bits > is->src_size * 8 - pos) || (bits < 0 && -bits > pos)) {
        is->error = 1;
        bg_seek(is, bits < 0 ? 0 : is->src_size * 8);
        return;
    }

    bg_seek(is, pos + bits);
}

int bg_read1(bitstream_t* is) {
    if (bg_left(is) <= 0) {
        is->error = 1;
        return 0;
    }

    int bit = (*is->curr >> is->bitpos) & 1;
    is->bitpos++;

    if (is->bitpos == 8) {
        is->bitpos = 0;
        is->curr++;
    }

    return bit;
}

//-------------------------------------------------------------------------------------------------
// API

//TODO rename
struct g719_handle_t {
    DecoderState state;
    int frame_size;
};

g719_handle_t* g719_init(int frame_size) {

    if (frame_size <= 0 || frame_size > G719_MAX_FRAME_SIZE)
        return NULL;

    g719_handle_t* handle = calloc(1, sizeof(g719_handle_t));
    if (!handle)
        return NULL;

    handle->frame_size = frame_size;

    decoder_init(&handle->state, handle->frame_size * 8);

    return handle;
}

void g719_reset(g719_handle_t* handle) {
    decoder_init(&handle->state, handle->frame_size * 8);
}

void g719_free(g719_handle_t* handle) {
    free(handle);
}

void g719_decode_frame(g719_handle_t* handle, unsigned char* code_words, short* sbuf) {

    // OG: handles interleaved channel frames and other control stuff, but handled differently here

    // normally here 'code_words' would be copied to a 'short code_bits[G719_MAX_FRAME_SIZE * 8]' array,
    // where 0 = G192_BIT0 = 0x7F and 1 = G192_BIT1 = 0x81 read byte by byte LSB style (a.k.a a G.192 bitstream).
    // ex.  6D DB -> 01101101 11011011, read as 10110110 11011011 (0=7F, 1=81)
    //   01101101 -> 81 7f 81 81 7f 81 81 7f (inverted)
    //   11011011 -> 81 81 7f 81 81 7f 81 81 (inverted)

    // Not replicated here since no games use it. Just AES 192, see Namco's g7721
    //if (encrypted) {
    //    s14aes_decrypt(handle->aes, data);
    //}

    decode_frame(code_words, handle->frame_size, 0, sbuf, &handle->state);
}
