/* Decodes Inti Creates' BIGRP files with custom codecs, found in games using their current 
 * "Inti Creates Engine" ("ICE") / "Imperial Engine" ('IMP'?). Engine's name is said to be
 * the latter (ICE being the earlier iteration) but debug info still shows the former.
 * Reverse engineered from various exes (if you use this as a base credit it, please).
 * 
 * This code tries to follow the original closely for documentation purposes, with some extra 
 * error control (original doesn't check zlib errors or buf sizes) plus a few extra structs/functions
 * that were likely inline'd (such as bitreaders). */

//TODO change to streaming decoder
// Currently lib expects most data in memory. Due to how format is designed it's not the
// easiest thing to change, to be fixed it later:
// - data is divided into 2 blocks (intro+body) that are decoded separatedly
//   (streaming should read up to block max)
// - code data isn't divided into frames, just keeps reading from the file buf
// - "range" decoder has linear data, and should be easy enough to stream, but it's rarely used.
// - "dct" decoder has a big chunk (+30%) of codebook data at the beginning of each block, then
//   code data *but* decoder reads simultaneously from both places. Files can be rather big
//   (2 mins = 6mb, codebooks = ~1.5mb). Would need to pre-read all the codebooks (still big) then
//   stream data, or seek around to codebooks (thrashes FILE buffers).


#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "ice_decoder_icelib.h" 

/* use miniz (API-compatible) to avoid adding external zlib just for this codec
 * - https://github.com/richgel999/miniz */
#include "../util/miniz.h" 
//#include "zlib.h"

#define ICESND_MAX_CHANNELS    2


/* ************************************************************ */
/* COMMON */
/* ************************************************************ */

static inline uint8_t get_u8(const uint8_t* p) {
    uint8_t ret;
    ret  = ((uint16_t)(const uint8_t)p[0]) << 0;
    return ret;
}

static inline uint16_t get_u16le(const uint8_t* p) {
    uint16_t ret;
    ret  = ((uint16_t)(const uint8_t)p[0]) << 0;
    ret |= ((uint16_t)(const uint8_t)p[1]) << 8;
    return ret;
}

static inline uint32_t get_u32le(const uint8_t* p) {
    uint32_t ret;
    ret  = ((uint32_t)(const uint8_t)p[0]) << 0;
    ret |= ((uint32_t)(const uint8_t)p[1]) << 8;
    ret |= ((uint32_t)(const uint8_t)p[2]) << 16;
    ret |= ((uint32_t)(const uint8_t)p[3]) << 24;
    return ret;
}

/* bigrp entry info as read from header */
typedef struct {
    uint32_t hash1; /* usually matches filename, different files vary on bytes, seems internally used to identify files */
    uint32_t hash2; /* group id? repeated in several files, doesn't seem used */
    uint32_t codec; /* 00: range, 01: metadata, 02: midi, 03: DCT */

    /* rest of header varies per codec (padded until ~0x40 for all) */
    /* codec 01 entry: */
    /* - offset */
    /* - size */

    /* codec 02 entry: */
    /* - config? (big value, same for all entries) */
    /* - midi offset */
    /* - midi size */
    /* - midi config? (~0x30C0, possibly some size) */
    /* - midi config? (~0x4036, then divided by 180.0) */

    /* codec 00/03 entry */
    int sample_rate;
    int channels;
    int spf;        /* always 16, internally used for pcm size calculations */
    int unknown;    /* some kind of low-ish value, volume? (seen 0x40~0x7F) */

    int loop_flag;
    int frame_codes; /* 0x64 in codec 00, 0x00 in codec 03 (possibly "RangeBlockSize") */

    /* has one "intro" block then one "body" block; intro block may be zero in full loops/no loops */
    uint32_t intro_samples;
    uint32_t intro_zsize;
    uint32_t intro_offset;
    uint32_t body_samples;
    uint32_t body_zsize;
    uint32_t body_offset;

    /* rest: padding until entry_size */

} bigrp_entry_t;

/* base bigrp header and extra config */
typedef struct {
    uint32_t head_size;
    uint32_t entry_size;
    int total_subsongs;
    uint32_t dummy;

} bigrp_header_t;

/* block/data format (handled later):
 * 
 * codec 00 (range):
 * - zlib block (32b deflated size + zlib data)
 *
 * codec 03 (dct)
 * - codeinfo block (see codeinfo_parse)
 * - zlib tables
 * - data
 *
 * codec 01 (data?)
 * - ?
 *   Not parsed as audio in the IcePlayer init. Usually paired with midis, has several sections and some
 *   point to entries using hash1, so probably soundfont config, but not all bigrp with midis have this.
 *
 * codec 02 (midi)
 * - standard midi (MThd)
 */
 

/* OG code casts buffer to this struct, read in a more portable fashion */
static int bigrp_entry_parse(bigrp_entry_t* entry, const uint8_t* buf, int buf_size) {

    if (buf_size < 0x34)
        goto fail;

    entry->hash1        = get_u32le(buf + 0x00);
    entry->hash2        = get_u32le(buf + 0x04);
    entry->codec        = get_u32le(buf + 0x08); /* u8 in decoder, read fully to validate */

    switch (entry->codec) {
        case ICESND_CODEC_RANGE:
        case ICESND_CODEC_DCT:
            entry->sample_rate      = get_u32le(buf + 0x0c);
            entry->channels         = get_u8   (buf + 0x10);
            entry->spf              = get_u8   (buf + 0x11);
            entry->unknown          = get_u16le(buf + 0x12);
            entry->loop_flag        = get_u32le(buf + 0x14); /* u8 in decoder, read fully to validate */
            entry->frame_codes      = get_u32le(buf + 0x18); /* codec 00 only (includes N channels) */

            entry->intro_samples    = get_u32le(buf + 0x1c);
            entry->intro_zsize      = get_u32le(buf + 0x20);
            entry->intro_offset     = get_u32le(buf + 0x24);
            entry->body_samples     = get_u32le(buf + 0x28);
            entry->body_zsize       = get_u32le(buf + 0x2c);
            entry->body_offset      = get_u32le(buf + 0x30);

            if (entry->sample_rate < 2000 || entry->sample_rate > 48000) /* just in case */
                goto fail;
            if (entry->channels < 1 || entry->channels > 2 || entry->spf != 16) /* not seen */
                goto fail;
            if (entry->frame_codes != 0 && entry->frame_codes != 0x64)
                goto fail;
            if (entry->frame_codes % entry->channels != 0) /* assumed that N codes = N/chs samples */
                goto fail;
            if (entry->loop_flag != 0 && entry->loop_flag != 1)
                goto fail;

            /* probably wouldn't matter, same with other sizes */
            if (entry->intro_samples == 0 && entry->body_samples == 0)
                goto fail;

            if (entry->channels > 1 && entry->codec == ICESND_CODEC_RANGE) /* not seen */
                goto fail;
            break;

        case ICESND_CODEC_DATA:
        case ICESND_CODEC_MIDI:
        default:
            goto fail;
    }

    return ICESND_RESULT_OK;
fail:
    return ICESND_ERROR_HEADER;
}

/* read main .bigrp header. Earlier games used standard Nintendo's BFGRP and BCGRP, 
 * and this format seems kind of inspired by it, so presumably BIGRP = Binary Inti Group */
static int bigrp_header_parse(bigrp_header_t* hdr, const uint8_t* buf, int buf_size, int subsong) {

    if (buf_size < 0x0c)
        goto fail;


    /* read base header */
    hdr->head_size = get_u32le(buf + 0x00);
    hdr->entry_size = get_u32le(buf + 0x04);
    hdr->total_subsongs = get_u32le(buf + 0x08);

    if (hdr->head_size > buf_size)
        goto fail;

    if (hdr->head_size >= 0x10)
        hdr->dummy = get_u32le(buf + 0x0c);
    else
        hdr->dummy = 0x00;

    /* 0x0c: Bloodstained COTM (Vita/3DS), Mighty Gunvolt Burst (PC); 0x10: rest */
    if (hdr->head_size != 0x0c && hdr->head_size != 0x10)
        goto fail;
    /* same (no changes, after 0x34 is padding) */
    if (hdr->entry_size != 0x34 && hdr->entry_size != 0x40)
        goto fail;
    if (hdr->dummy != 0x00)
        goto fail;

    if (subsong < 1 || subsong > hdr->total_subsongs)
        goto fail;

    return ICESND_RESULT_OK;
fail:
    return ICESND_ERROR_HEADER;
}

/* original also tries to call's zlib free is needed (not set though) */
static void zlib_end(z_stream* strm, int* p_zlib_init) {
    if (*p_zlib_init) {
        inflateEnd(strm);

        /* original also tries to call's zlib free if needed (not set though) */

        *p_zlib_init = 0;
    }
}

static int zlib_init(z_stream* strm, int* p_zlib_init, const uint8_t* buf, int buf_size) {
    int err;

    zlib_end(strm, p_zlib_init);

    strm->zalloc = 0;
    strm->zfree = 0;
    strm->opaque = 0;
    strm->next_in = 0;
    strm->avail_in = 0;
    err = inflateInit(strm);
    if (err < 0) return ICESND_ERROR_SETUP;
    *p_zlib_init = 1;

    /* zlib data starts with the decompressed size */
    strm->next_in = buf + 0x04;
    strm->avail_in = buf_size - 0x04;

    return ICESND_RESULT_OK;
}


/* ************************************************************ */
/* RANGE */
/* ************************************************************ */

/* Inti Creates's "range" decoder, internally IceSoundCodecDecoderRange class. Seems similar to Sony's
 * "adaptive dynamic range coding" (ADRC) compression (not what's usually called "range coding").
 *
 * Data is zlibbed (though doesn't save much) then divided into VBR frames. Each frame has a 24-bit LE
 * header that defines a "range" (min..max) and quantized codes' bits (often 6 or 7), then 100 codes.
 * Unsigned codes just map to a relative value within the range, and final sample is range-min + range-value.
 * Stereo alternates L-R header then L R L R ... samples
 *
 * For example, if a range = [0, 6000], and bits = 6:
 * - code=0 > value=0 (min), code=63 > value=6000 (max), code=31 > value=2952 (in-between), ...
 */

#define RANGE_DECODE_BUFFER     0x800

typedef struct {
    const uint8_t* inbuf;
    int inbuf_size;
    int max_samples;
    int frame_codes;
    int samples_done;
    uint32_t outbuf_pos;
    uint8_t outbuf[RANGE_DECODE_BUFFER];
    z_stream strm;
    int codes_left;
    int bitpos; /* within curr_byte */
    int16_t range_min[ICESND_MAX_CHANNELS];
    int16_t range_max[ICESND_MAX_CHANNELS];
    uint16_t range_bits[ICESND_MAX_CHANNELS];
    uint16_t range_mask[ICESND_MAX_CHANNELS];
    uint8_t curr_byte;
    uint8_t spf; /* not needed to decode though */
    uint8_t channels;
    int zlib_init;

    /* extra */
    uint32_t outbuf_max; /* original has all data in memory, but we read zlib in chunks that may be smaller */

} range_handle_t;


static range_handle_t* range_decoder_open() {
    range_handle_t* ctx = NULL;

    //ctx = calloc(1, sizeof(range_handle_t));
    ctx = malloc(sizeof(range_handle_t));
    if (!ctx) goto fail;

    ctx->inbuf = NULL;
    ctx->inbuf_size = 0;
    ctx->max_samples = 0;
    ctx->frame_codes = 0;
    ctx->spf = 0;
    ctx->zlib_init = 0;

    return ctx;
fail:
    return NULL;
}

static void range_decoder_close(range_handle_t* ctx) {
    if (!ctx)
        return;

    zlib_end(&ctx->strm, &ctx->zlib_init);
    free(ctx);
}

static int range_decoder_reset(range_handle_t* ctx) {
    int err;

    err = zlib_init(&ctx->strm, &ctx->zlib_init, ctx->inbuf, ctx->inbuf_size);
    if (err < ICESND_RESULT_OK) return err;

    ctx->outbuf_pos = 0xFFFFFFFF; //sizeof(ctx->outbuf); /* force init */
    ctx->samples_done = 0;
    ctx->codes_left = 0;
    ctx->bitpos = 0;

    ctx->outbuf_max = 0;

    return ICESND_RESULT_OK;
}

static int range_decoder_block_setup(range_handle_t* ctx, const uint8_t* buf, int buf_size, bigrp_entry_t* etr, int max_samples) {
    int err;

    ctx->inbuf = buf;
    ctx->inbuf_size = buf_size;
    ctx->max_samples = max_samples;

    ctx->frame_codes = etr->frame_codes;
    ctx->spf = etr->spf;
    ctx->channels = etr->channels;

    err = range_decoder_reset(ctx);
    if (err < ICESND_RESULT_OK) return err;

    return ICESND_RESULT_OK;
}

#if 0
static void fill_zlib(z_stream* strm, data_t* data) {

    /* zlib sets this to 0 once consumed */
    if (strm->avail_in > 0)
        return;

    /* buffer is smaller than size, so this will be called N times */
    if (data->data_left) {
        int bytes = data->buf_size;
        if (bytes > data->data_left)
            bytes = data->data_left;

        strm->next_in = data->buf;
        strm->avail_in = data->cb.read(data->buf, 1, bytes, data->cb.arg);

        data->data_left -= strm->avail_in;
    }
}
#endif

/* get next byte from the zlib stream */
static void range_load_byte(range_handle_t* ctx) {

    if (ctx->outbuf_pos >= ctx->outbuf_max) { /* OG: >= sizeof(ctx->outbuf) */
        //fill_zlib(&ctx->strm, ctx->data);

        ctx->strm.avail_out = sizeof(ctx->outbuf);
        ctx->strm.next_out = ctx->outbuf;
        inflate(&ctx->strm, Z_NO_FLUSH);
        //if (err < Z_OK) return ICESND_ERROR_DECODER; /* OG: no error control (shouldn't matter) */

        ctx->outbuf_pos = 0;
        ctx->outbuf_max = sizeof(ctx->outbuf) - ctx->strm.avail_out;
    }

    ctx->curr_byte = ctx->outbuf[ctx->outbuf_pos];
    ctx->outbuf_pos++;
}

/* read 24-bit header, decompressing bytes if necessary */
static void range_load_header(range_handle_t* ctx, int ch) {
    uint32_t frame_header;

    range_load_byte(ctx);
    frame_header = ctx->curr_byte;

    range_load_byte(ctx);
    frame_header = (ctx->curr_byte << 8) | frame_header;

    range_load_byte(ctx);
    frame_header = (ctx->curr_byte << 16) | frame_header;

    /* get signed range and quantized bits for this frame */
    ctx->range_min[ch] = (frame_header >> 3) << 5;  /* & 0xFFE0, upper 11 bits (signed) */
    ctx->range_max[ch] = (frame_header >> 14) << 6; /* & 0xFFC0, upper 10 bits (signed) */ //(frame_header >> 8) & 0xffc0;
    ctx->range_bits[ch] = (frame_header & 7) + 1;
    ctx->range_mask[ch] = (1 << ctx->range_bits[ch]) - 1;
}

/* decode next bitstream's code, decompressing bytes if necessary */
static int16_t range_get_sample(range_handle_t* ctx, int ch) {
    int32_t code;
    int16_t delta;
    uint16_t mask = ctx->range_mask[ch];

    /* get next code of N-bits (doesn't seem to use an actual bitstream class) */
    if (ctx->bitpos == 0) {
        range_load_byte(ctx);
    }
    code = (ctx->curr_byte >> (ctx->bitpos)) & mask; 

    if (ctx->bitpos + ctx->range_bits[ch] > 8) {
        range_load_byte(ctx);
        code |= (ctx->curr_byte << (8 - ctx->bitpos)) & mask;

        ctx->bitpos = (ctx->bitpos + ctx->range_bits[ch]) - 8;
    }
    else {
        ctx->bitpos = (ctx->bitpos + ctx->range_bits[ch]) & 7;
    }

    /* calculate code's range value and final sample */
    delta = code * (ctx->range_max[ch] - ctx->range_min[ch]) / mask;
    return ctx->range_min[ch] + delta; /* no clamp */
}

/* decode N samples and copy to sbuf.
 * Internally decodes N samples at a time, and if asked for non-multiple number of samples it'll
 * stop and resume properly from last copied sample of those 16. Return 1 if no more samples left. */
static int range_decoder_decode(range_handle_t* ctx, int16_t* sbuf, const int max_done, int* p_done) {
    int ch;


    *p_done = 0;

    while (ctx->samples_done < ctx->max_samples) {

        /* read frame header */
        if (ctx->codes_left == 0) {
            for (ch = 0; ch < ctx->channels; ch++) {
                range_load_header(ctx, ch);
            }

            ctx->codes_left = ctx->frame_codes;
            if (ctx->samples_done + ctx->frame_codes > ctx->max_samples)
                ctx->codes_left = ctx->max_samples - ctx->samples_done;
            ctx->bitpos = 0;
        }

        /* decode frame samples */
        while (ctx->codes_left) {
            for (ch = 0; ch < ctx->channels; ch++) {
                *sbuf++ = range_get_sample(ctx, ch);
            }

            ctx->samples_done++;
            ctx->codes_left--;

            (*p_done)++;
            if (*p_done >= max_done) /* samples left */
                return ctx->samples_done >= ctx->max_samples; /* block done */
        }
    }

    return ctx->samples_done >= ctx->max_samples; /* block done */
}


/* ************************************************************ */
/* DCT */
/* ************************************************************ */

/* Inti Creates's "dct" decoder, internally IceSoundCodecDecoderDCT class, a pretty simple DCT codec.
 *
 * Header stores one codebook per band (max 16 * channels) of quantized bits, in zlibbed chunks of 4-bit nibbles.
 * Data is just a bitstream of up to 16 bands (L then R) of variable bit codes (max 16-bit) that depend on previous
 * 16 bands, dequantized using iDCT. Uses mid-side stereo but otherwise no other features like frames, scalefactors
 * or overlaps/delay. Samples are encoded directly as +-32768, 16 at a time.
 */

#define DCT_MAX_BANDS 16
#define DCT_MAX_TRANSFORM 8
#define DCT_MAX_PREV 4
#define DCT_MAX_PREV_MASK 0x3
#define DCT_CODEBOOK_BUFFER 0x100

typedef struct {
    const uint8_t* buf;
    int bitpos;
    int bitstart;
    int max_bits;
} dct_bitreader_t;

typedef struct {
    uint32_t table_size;
    uint8_t init_scale;
    uint8_t bands;
    uint8_t channels;
    uint8_t unused;
    uint32_t max_samples;
    uint32_t cbk_offset[ICESND_MAX_CHANNELS][DCT_MAX_BANDS];
    uint32_t cbk_size[ICESND_MAX_CHANNELS][DCT_MAX_BANDS];
    uint32_t data_start;
    uint32_t data_size;
} dct_codeinfo_t;

typedef struct {
    z_stream strm;
    uint8_t outbuf[DCT_CODEBOOK_BUFFER];
    dct_bitreader_t br;
    int zlib_init;
} dct_codebook_t;

typedef struct {
    dct_codeinfo_t codeinfo_mem; /* OG code just casts a pointer to this, keep struct around */
    dct_codeinfo_t* codeinfo;
    int samples_done;
    dct_codebook_t codebook[ICESND_MAX_CHANNELS][DCT_MAX_BANDS];
    dct_bitreader_t br;
    float transform[DCT_MAX_TRANSFORM][DCT_MAX_BANDS];
    float unused[DCT_MAX_TRANSFORM][DCT_MAX_BANDS]; /* ? */
    int16_t spectra[ICESND_MAX_CHANNELS][DCT_MAX_PREV][DCT_MAX_BANDS];
    int spectra_curr;
    int16_t sbuf_tmp[DCT_MAX_BANDS * ICESND_MAX_CHANNELS]; /* interleaved */
    float scales[16];
} dct_handle_t;


/* ****************************** */
/* IceSoundCodecDecoder(Bitreader) */

/* OG bitreader uses u32 buf and reads u32le at a time (to simplify shifting), and aligns for 32-bit,
 * since blocks pointer can start in the middle (whole file loaded in memory + non-padded blocks) */

static void dct_bitreader_init(dct_bitreader_t* ctx) {
    ctx->buf = NULL;
    ctx->bitpos = 0;
    ctx->bitstart = 0;
    ctx->max_bits = 0;
}

static void dct_bitreader_set(dct_bitreader_t* ctx, const uint8_t* buf, int buf_size) {
    //unsigned int* buf32 = (unsigned int *)(buf);
    //ctx->buf32 = (uin32_t*)((uint32_t)buf32 & 0xFFFFFFFC); /* align to 32-bit boundary */
    //ctx->bitstart = 8 * ((uint8_t)buf32 & 3); /* align to pointer start */

    ctx->buf = buf;
    ctx->bitstart = 8 * 0; /* non-aligned ok */
    ctx->bitpos = ctx->bitstart;
    ctx->max_bits = 8 * buf_size;
}

static int dct_bitreader_is_over(dct_bitreader_t* ctx) {
    return ctx->bitpos >= ctx->bitstart + ctx->max_bits;
}

static uint32_t dct_bitreader_get(dct_bitreader_t* ctx, int bits) {
    uint32_t code32;
    uint8_t shift;
    int pos;
    uint32_t mask;

    if (ctx->bitpos + bits > ctx->bitstart + ctx->max_bits) /* ? */
        return 0;

    pos = ctx->bitpos >> 3;
    shift = ctx->bitpos & 0x7; /* within u8 */

    code32 = ctx->buf[pos] >> shift;
    if (bits + shift > 8) {
        code32 |= ctx->buf[pos+1] << (8u - shift);
        if (bits + shift > 16) {
            code32 |= ctx->buf[pos+2] << (16u - shift);
            if (bits + shift > 24) {
                code32 |= ctx->buf[pos+3] << (24u - shift);
                if (bits + shift > 32) {
                    code32 |= ctx->buf[pos+4] << (32u - shift);
                }
            }
        }
    }

    //pos = ctx->br.bitpos >> 5;
    //shift = ctx->br.bitpos & 0x1f; /* within u32 */

    //code32 = ctx->buf32[pos] >> shift;
    //if (qbits + shift > 32) {
    //    code32 |= ctx->buf32[pos+1] << (32u - shift);
    //}

    ctx->bitpos += bits;

    mask = (((1 << bits)) - 1);
    return code32 & mask;
}

/* ****************************** */
/* IceSoundCodecDecoder(Codebook) */

static void dct_codebook_init(dct_codebook_t* ctx) {
    /* no alloc needed (part of dct_handle_t) */

    dct_bitreader_init(&ctx->br);

    ctx->zlib_init = 0;
}

static void dct_codebook_close(dct_codebook_t* ctx) {
    if (!ctx)
        return;

    zlib_end(&ctx->strm, &ctx->zlib_init);
    /* no free needed */
}

static int dct_codebook_reset(dct_codebook_t* ctx, const uint8_t* buf, int buf_size) {
    int err;

    err = zlib_init(&ctx->strm, &ctx->zlib_init, buf, buf_size);
    if (err < ICESND_RESULT_OK) return err;

    dct_bitreader_init(&ctx->br);

    return ICESND_RESULT_OK;
}

/* Read next quantized value's bits from zlibbed codebook. Data is in LSB order and codes 4-bits, like:
 *  0x10,0x32,0x54,0x76,0x98... = 0 1 2 3 4 5 6 7 8 9... */
static uint8_t dct_codebook_get_qbits(dct_codebook_t* ctx) {
    uint32_t qbits;

    if (dct_bitreader_is_over(&ctx->br)) {
        ctx->strm.avail_out = sizeof(ctx->outbuf);
        ctx->strm.next_out = ctx->outbuf;
        inflate(&ctx->strm, Z_NO_FLUSH);
        //if (err < Z_OK) return ICESND_ERROR_DECODER; /* OG: no error control (shouldn't matter) */

        dct_bitreader_set(&ctx->br, ctx->outbuf, sizeof(ctx->outbuf) - ctx->strm.avail_out);
    }

    qbits = dct_bitreader_get(&ctx->br, 4);

    return qbits;
}

/* *********************** */
/* IceSoundCodecDecoderDCT */

static dct_handle_t* dct_decoder_open() {
    int i, ch, band;
    dct_handle_t* ctx = NULL;

    //ctx = calloc(1, sizeof(dct_handle_t));
    ctx = malloc(sizeof(dct_handle_t));
    if (!ctx) goto fail;

    ctx->codeinfo = NULL;

    /* init all codebook's base values */
    for (ch = 0; ch < ICESND_MAX_CHANNELS; ch++) {
        for (band = 0; band < DCT_MAX_BANDS; band++) {
            dct_codebook_t* codebook = &ctx->codebook[ch][band];

            dct_codebook_init(codebook);
        }
    }

    dct_bitreader_init(&ctx->br);

    for (i = 0; i < DCT_MAX_BANDS; i++) {
        ctx->scales[i] = 1.0f;
    }

    return ctx;
fail:
    return NULL;
}

static void dct_decoder_close(dct_handle_t* ctx) {
    int ch, band;
    

    /* setup all codebook's zlib streams (even if not used, since we can close before anything is set) */
    for (ch = 0; ch < ICESND_MAX_CHANNELS; ch++) {
        for (band = 0; band < DCT_MAX_BANDS; band++) {
            dct_codebook_t* codebook = &ctx->codebook[ch][band];

            dct_codebook_close(codebook);
        }
    }

    free(ctx);
}

static int dct_decoder_reset(dct_handle_t* ctx, const uint8_t* buf) {
    int err;
    int ch, band;
    dct_codeinfo_t* ci = ctx->codeinfo;

    /* OG code doesn't pass buf (since reset is a virtual method), but rather codeinfo doubles as a pointer to data start */

    /* close all codebook's zlib streams */
    for (ch = 0; ch < ci->channels; ch++) {
        for (band = 0; band < ci->bands; band++) {
            dct_codebook_t* codebook = &ctx->codebook[ch][band];

            const uint8_t* cbk_start = buf + ci->cbk_offset[ch][band];
            int cbk_size = ci->cbk_size[ch][band];

            err = dct_codebook_reset(codebook, cbk_start, cbk_size);
            if (err < ICESND_RESULT_OK) return err;
        }
    }

    dct_bitreader_set(&ctx->br, buf + ci->data_start, ci->data_size);

    memset(ctx->spectra, 0, sizeof(ctx->spectra));
    ctx->samples_done = 0;
    ctx->spectra_curr = 0;

    return ICESND_RESULT_OK;
}

/* transform spectrum into samples (iDCT) */
static void dct_decoder_transform(dct_handle_t* ctx, int16_t* sbuf_tmp, int channel, int pos) {
    int i, band;
    float fbuf[16] = {0}; /* no need to init as it's written in band 0 but gcc complains */
    float f_curr;
    dct_codeinfo_t* ci = ctx->codeinfo;


    for (band = 0; band < ci->bands; band++) {
        /* scales seems fixed to 1.0, maybe a remnant */
        float coef = (float)ctx->spectra[channel][pos][band] * ctx->scales[band];

        /* optimized butterfly ops? */
        switch (band) {
            case 0:     /* bits 0000 */
                f_curr = ctx->transform[0][band] * coef;
                fbuf[0]  = f_curr;
                fbuf[1]  = f_curr;
                fbuf[2]  = f_curr;
                fbuf[3]  = f_curr;
                fbuf[4]  = f_curr;
                fbuf[5]  = f_curr;
                fbuf[6]  = f_curr;
                fbuf[7]  = f_curr;
                fbuf[8]  = f_curr;
                fbuf[9]  = f_curr;
                fbuf[10] = f_curr;
                fbuf[11] = f_curr;
                fbuf[12] = f_curr;
                fbuf[13] = f_curr;
                fbuf[14] = f_curr;
                fbuf[15] = f_curr;
                break;

            case 1:
            case 3:
            case 5:
            case 7:
            case 9:
            case 11:
            case 13:
            case 15:    /* bits xxx1 */
                f_curr = ctx->transform[0][band] * coef;
                fbuf[0]  += f_curr;
                fbuf[15] -= f_curr;

                f_curr = ctx->transform[1][band] * coef;
                fbuf[1]  += f_curr;
                fbuf[14] -= f_curr;

                f_curr = ctx->transform[2][band] * coef;
                fbuf[2]  += f_curr;
                fbuf[13] -= f_curr;

                f_curr = ctx->transform[3][band] * coef;
                fbuf[3]  += f_curr;
                fbuf[12] -= f_curr;

                f_curr = ctx->transform[4][band] * coef;
                fbuf[4]  += f_curr;
                fbuf[11] -= f_curr;

                f_curr = ctx->transform[5][band] * coef;
                fbuf[5]  += f_curr;
                fbuf[10] -= f_curr;

                f_curr = ctx->transform[6][band] * coef;
                fbuf[6]  += f_curr;
                fbuf[9]  -= f_curr;

                f_curr = ctx->transform[7][band] * coef;
                fbuf[7]  += f_curr;
                fbuf[8]  -= f_curr;
                break;

            case 2u:
            case 6u:
            case 10:
            case 14:    /* bits xx10 */
                f_curr = ctx->transform[0][band] * coef;
                fbuf[0]  += f_curr;
                fbuf[7]  -= f_curr;
                fbuf[8]  -= f_curr;
                fbuf[15] += f_curr;

                f_curr = ctx->transform[1][band] * coef;
                fbuf[1]  += f_curr;
                fbuf[6]  -= f_curr;
                fbuf[9]  -= f_curr;
                fbuf[14] += f_curr;

                f_curr = ctx->transform[2][band] * coef;
                fbuf[2]  += f_curr;
                fbuf[5]  -= f_curr;
                fbuf[10] -= f_curr;
                fbuf[13] += f_curr;

                f_curr = ctx->transform[3][band] * coef;
                fbuf[3]  += f_curr;
                fbuf[4]  -= f_curr;
                fbuf[11] -= f_curr;
                fbuf[12] += f_curr;
                break;

            case 4:
            case 12:    /* bits x100 */
                f_curr = ctx->transform[0][band] * coef;
                fbuf[0]  += f_curr;
                fbuf[3]  -= f_curr;
                fbuf[4]  -= f_curr;
                fbuf[7]  += f_curr;
                fbuf[8]  += f_curr;
                fbuf[11] -= f_curr;
                fbuf[12] -= f_curr;
                fbuf[15] += f_curr;

                f_curr = ctx->transform[1][band] * coef;
                fbuf[1]  += f_curr;
                fbuf[2]  -= f_curr;
                fbuf[5]  -= f_curr;
                fbuf[6]  += f_curr;
                fbuf[9]  += f_curr;
                fbuf[10] -= f_curr;
                fbuf[13] -= f_curr;
                fbuf[14] += f_curr;
                break;

            case 8:     /* bits 1000 */
                f_curr = ctx->transform[0][band] * coef;
                fbuf[0]  += f_curr;
                fbuf[1]  -= f_curr;
                fbuf[2]  -= f_curr;
                fbuf[3]  += f_curr;
                fbuf[4]  += f_curr;
                fbuf[5]  -= f_curr;
                fbuf[6]  -= f_curr;
                fbuf[7]  += f_curr;
                fbuf[8]  += f_curr;
                fbuf[9]  -= f_curr;
                fbuf[10] -= f_curr;
                fbuf[11] += f_curr;
                fbuf[12] += f_curr;
                fbuf[13] -= f_curr;
                fbuf[14] -= f_curr;
                fbuf[15] += f_curr;
                break;
            default:
                break;
        }
    }

    /* copy float samples to sbuf samples */
    for (i = 0; i < DCT_MAX_BANDS; i++) {
        float sample = roundf(fbuf[i]);
        /* interleaved (L: 0,2,4,6,8... R:1,3,5,7...) */
        sbuf_tmp[channel + ci->channels * i] = (int16_t)sample; /* no clamp */
    }
}

/* read current code from the bitstream, in LE byte order */
static int16_t dct_decoder_get_code(dct_handle_t* ctx, uint8_t qbits) {
    uint32_t code32;
    int16_t code16; /* also ok as int32 */

    /* get code from bitstream */
    if (qbits <= 0) { /* no resolution: 1-bit where 0 = 0 and 1 = -1 */
        code32 = dct_bitreader_get(&ctx->br, 1);

        code16 = (int16_t)((int16_t)code32 << 15) >> 15; /* sign extend */
    }
    else {
        code32 = dct_bitreader_get(&ctx->br, qbits);

        code16 = code32; /* qbits max 0..15 */
        if (code16 < (1 << (qbits - 1))) /* negative encoding */
            code16 = code16 - (1 << qbits);
    }

    return code16;
}

/* read codes for this channel */
static void dct_decoder_dequantize(dct_handle_t* ctx, int channel, int pos) {
    int band;
    dct_codeinfo_t* ci = ctx->codeinfo;

    int16_t* spectra = ctx->spectra[channel][pos];
    int16_t* spectra_prev1 = ctx->spectra[channel][(pos - 1) & DCT_MAX_PREV_MASK];
    int16_t* spectra_prev2 = ctx->spectra[channel][(pos - 2) & DCT_MAX_PREV_MASK];


    for (band = 0; band < ci->bands; band++) {
        uint8_t qbits; /* common 7~10 bits, sometimes 12 too */
        int16_t code; /* also ok as int32 */

        /* get next code's resolution and code */
        qbits = dct_codebook_get_qbits(&ctx->codebook[channel][band]);
        code = dct_decoder_get_code(ctx, qbits);

        /* calc final value based on previous */
        spectra[band] = code + (int16_t)(2 * spectra_prev1[band]) - spectra_prev2[band];
    }
}

/* restore L/R bands based on mid channel + side differences, ratio 1.0 + copy to final buffer */
static void dct_decoder_ms_stereo(dct_handle_t* ctx, int16_t* sbuf_tmp) {
    int i;
    dct_codeinfo_t* ci = ctx->codeinfo;

    for (i = 0; i < DCT_MAX_BANDS; i++) {
        int16_t sample_l = sbuf_tmp[0 + ci->channels * i];
        int16_t sample_r = sbuf_tmp[1 + ci->channels * i];
        ctx->sbuf_tmp[0 + ci->channels * i] = sample_l + sample_r;
        ctx->sbuf_tmp[1 + ci->channels * i] = sample_l - sample_r;
    }
}

/* decode N samples and copy to sbuf.
 * Internally decodes 16 samples at a time, and if asked for non-multiple number of samples it'll
 * stop and resume properly from last copied sample of those 16. Return 1 if no more samples left. */
static int dct_decoder_decode(dct_handle_t* ctx, int16_t* sbuf, const int max_done, int* p_done) {
    int ch;
    int16_t sbuf_loc[DCT_MAX_BANDS * ICESND_MAX_CHANNELS]; /* interleaved */
    int16_t* sbuf_tmp;
    dct_codeinfo_t* ci = ctx->codeinfo;
    int samples_left;


    *p_done = 0;
    samples_left = max_done;
    if (samples_left > ci->max_samples - ctx->samples_done)
        samples_left = ci->max_samples - ctx->samples_done;

    /* 2ch uses a tmp buffer to handle MS stereo */
    if (ci->channels == 1)
        sbuf_tmp = ctx->sbuf_tmp;
    else
        sbuf_tmp = sbuf_loc;

    while (ctx->samples_done < ci->max_samples) {

        if (!samples_left)
            return ctx->samples_done >= ci->max_samples;

        /* decode 16 samples (every 16 samples) */
        if ((ctx->samples_done & 0xF) == 0) {

            for (ch = 0; ch < ci->channels; ch++) {
                dct_decoder_dequantize(ctx, ch, ctx->spectra_curr);

                dct_decoder_transform(ctx, sbuf_tmp, ch, ctx->spectra_curr);
            }

            ctx->spectra_curr = (ctx->spectra_curr + 1) & DCT_MAX_PREV_MASK; /* 0..3 and back to 0 */

            if (ci->channels == 2)
                dct_decoder_ms_stereo(ctx, sbuf_tmp);
        }

        /* copy to output sbuf */
        {
            int sample_start;
            int samples_copied;

            /* start could be non-zero if max_done is non-multiple of 16 */
            sample_start = ctx->samples_done & 0xF;
            samples_copied = 16 - sample_start;
            if (samples_copied > samples_left)
                samples_copied = samples_left;

            /* copy to output sbuf */
            memcpy(sbuf, &ctx->sbuf_tmp[sample_start * ci->channels], sizeof(int16_t) * ci->channels * samples_copied);
            sbuf += samples_copied * ci->channels;

            ctx->samples_done += samples_copied;
            samples_left -= samples_copied;
            *p_done += samples_copied;
        }
    }

    return ctx->samples_done >= ci->max_samples; /* block done */
}


/* OG code casts buffer to this struct, read in a more portable fashion */
static int dct_codeinfo_parse(dct_codeinfo_t* ci, const uint8_t* buf, int buf_size) {
    int ch, i, pos;

    if (buf_size < 0x114)
        goto fail;

    ci->table_size      = get_u32le(buf + 0x00);
    ci->init_scale      = get_u8   (buf + 0x04);
    ci->bands           = get_u8   (buf + 0x05);
    ci->channels        = get_u8   (buf + 0x06);
    ci->unused          = get_u8   (buf + 0x07);
    ci->max_samples     = get_u32le(buf + 0x08);

    pos = 0x0c;

    for (ch = 0; ch < ICESND_MAX_CHANNELS; ch++) {
        for (i = 0; i < DCT_MAX_BANDS; i++) {
            ci->cbk_offset[ch][i] = get_u32le(buf + pos);
            pos += 0x04;
        }
    }

    for (ch = 0; ch < ICESND_MAX_CHANNELS; ch++) {
        for (i = 0; i < DCT_MAX_BANDS; i++) {
            ci->cbk_size[ch][i] = get_u32le(buf + pos);
            pos += 0x04;
        }
    }

    ci->data_start      = get_u32le(buf + 0x10c);
    ci->data_size       = get_u32le(buf + 0x110);

    if (ci->table_size > 0x114)
        goto fail;
    if (ci->bands < 1 || ci->bands > DCT_MAX_BANDS)
        goto fail;
    if (ci->channels < 1 || ci->channels > ICESND_MAX_CHANNELS)
        goto fail;
    if (ci->unused != 0x00)
        goto fail;

    if (buf_size < ci->data_start + ci->data_size)
        goto fail;

    return ICESND_RESULT_OK;
fail:
    return ICESND_ERROR_SETUP;
}

/* base DCT unique coefs, used below to init the full table (see opus' analysis.c) */
static const float DCT_TRANSFORM_COEFS[16] = {
    0.25f,       0.35185099f, 0.34676f,     0.33832899f,
    0.32664099f, 0.31180599f, 0.29396901f,  0.27329999f,
    0.25f,       0.224292f,   0.19642401f,  0.166664f,
    0.135299f,   0.102631f,   0.068975002f, 0.034653999f,
};

static const float DCT_TRANSFORM_SCALES[16] = {
     4.0,  6.0,  8.0, 10.0, 12.0, 12.0, 13.0, 15.0,
    16.0, 16.0, 20.0, 24.0, 28.0, 35.0, 41.0, 41.0
};

static const int DCT_TRANSFORM_STEPS[16] = {
    1, 8, 4, 8, 2, 8, 4, 8,
    1, 8, 4, 8, 2, 8, 4, 8,
};

/* re-calculate DCT table, that depends on a current intro/body chunk's scale value */
static int dct_decoder_block_setup(dct_handle_t* ctx, const uint8_t* buf, int buf_size, bigrp_entry_t* etr) {
    int i;
    int err;
    float scale;
    float dct_coefs[DCT_MAX_BANDS];
    dct_codeinfo_t* ci = &ctx->codeinfo_mem;


    /* portable init */
    err = dct_codeinfo_parse(ci, buf, buf_size);
    if (err < ICESND_RESULT_OK) return err;

    /* pre-calculate scaled coefs (mini optimization?) */
    scale = ci->init_scale;
    for (i = 0; i < DCT_MAX_BANDS; i++) {
        dct_coefs[i] = DCT_TRANSFORM_COEFS[i] * scale;
    }

    /* transform for N=16, k=0..8? */
    for (i = 0; i < DCT_MAX_BANDS; i++) {
        int steps = DCT_TRANSFORM_STEPS[i];
        int step;
        int pos = i;

        for (step = 0; step < steps; step++) {
            float coef;

            switch ((pos >> 4) & 3) {
                case 1:
                    coef = -dct_coefs[16 - (pos & 0xF)];
                    break;
                case 2:
                    coef = -dct_coefs[(pos & 0xF)];
                    break;
                case 3:
                    coef = +dct_coefs[16 - (pos & 0xF)];
                    break;
                default:
                    coef = +dct_coefs[(pos & 0xF)];
                    break;
            }
            pos += 2 * i;

            //ctx->transform[step][i] = coef; /* somehow assigned twice originally? */
            ctx->transform[step][i] = DCT_TRANSFORM_SCALES[i] * coef;
        }
    }


    /* rest of setup */
    ctx->codeinfo = ci;

    err = dct_decoder_reset(ctx, buf);
    if (err < ICESND_RESULT_OK) return err;

    return ICESND_RESULT_OK;
}

/* ************************************************************ */
/* API */
/* ************************************************************ */
/* (not part of original code (but partially inspired by IceSSoundEng::IcePlayer) */

#define ICESND_BIGRP_SIZE 0x10
#define ICESND_ENTRY_SIZE 0x34
#define ICESND_BUF_SIZE 0x10000


struct icesnd_handle_t {
    /* config*/
    int target_subsong;
    icesnd_callback_t cb;

    /* state */
    bigrp_header_t hdr;
    bigrp_entry_t etr;

    void* decoder;
    int is_range;

    int intro_init;
    int body_init;
    int intro_done;

    /* absolute offset */
    int intro_offset;
    int body_offset;

    uint8_t* blkbuf;
    int blkbuf_size;
};


static int parse_header(icesnd_handle_t* ctx) {
    int err;
    uint8_t tmp[0x40];
    const uint8_t* buf;
    int buf_size;
    uint32_t offset;


    /* read common header size */
    offset = 0x00;
    if (ctx->cb.read) {
        ctx->cb.seek(ctx->cb.arg, offset, SEEK_SET);
        buf_size = ctx->cb.read(tmp, 1, 0x10, ctx->cb.arg);
        buf = tmp;
    }
    else {
        buf_size = ctx->cb.filebuf_size;
        buf = ctx->cb.filebuf + offset;
    }

    err = bigrp_header_parse(&ctx->hdr, buf, buf_size, ctx->target_subsong);
    if (err < ICESND_RESULT_OK) goto fail;

    /* read target entry */
    offset = ctx->hdr.head_size + ctx->hdr.entry_size * (ctx->target_subsong - 1);
    if (ctx->cb.read) {
        ctx->cb.seek(ctx->cb.arg, offset, SEEK_SET);
        buf_size = ctx->cb.read(tmp, 1, ctx->hdr.entry_size, ctx->cb.arg);
        buf = tmp;
    }
    else {
        if (offset > ctx->cb.filebuf_size) goto fail;
        buf = ctx->cb.filebuf + offset;
        buf_size = ctx->cb.filebuf_size - offset;
    }

    err = bigrp_entry_parse(&ctx->etr, buf, buf_size);
    if (err < ICESND_RESULT_OK) goto fail;

    if (ctx->etr.codec == ICESND_CODEC_RANGE || ctx->etr.codec == ICESND_CODEC_DCT) {
        ctx->intro_offset = offset + ctx->etr.intro_offset;
        ctx->body_offset = offset + ctx->etr.body_offset;
    }


    //TODO fix library later
    // see comment at top, but basically format is rather annoying to adapt as a streaming
    // decoder, and ran out of time. For now it reads a whole blocks (intro/body) at once. Sorry!

    /* prepare buf */
    if (ctx->cb.read) {
        int block_size = ctx->etr.body_zsize;
        if (block_size < ctx->etr.intro_zsize)
            block_size = ctx->etr.intro_zsize;
        if (block_size % 0x10 != 0) /* pad just in case */
            block_size = block_size + (0x10 - (block_size % 0x10));
        
        ctx->blkbuf_size = block_size;
        ctx->blkbuf = malloc(block_size);
        if (!ctx->blkbuf) goto fail;
    }

    return ICESND_RESULT_OK;
fail:
    return ICESND_ERROR_SETUP;
}

icesnd_handle_t* icesnd_init(int target_subsong, icesnd_callback_t* cb) {
    icesnd_handle_t* ctx = NULL;
    int err;

    ctx = calloc(1, sizeof(icesnd_handle_t));
    if (!ctx) goto fail;

    ctx->target_subsong = target_subsong;
    ctx->cb = *cb; /* memcpy */

    if (!cb->filebuf && !(cb->read && cb->seek))
        goto fail;

    err = parse_header(ctx);
    if (err < ICESND_RESULT_OK) goto fail;

    ctx->is_range = ctx->etr.codec == 0x00;
    if (ctx->is_range)
        ctx->decoder = range_decoder_open();
    else
        ctx->decoder = dct_decoder_open();
    if (!ctx->decoder) goto fail;

    icesnd_reset(ctx, 0);

    return ctx;
fail:
    icesnd_free(ctx);
    return NULL;
    //return ICESND_ERROR_SETUP;
}

void icesnd_free(icesnd_handle_t* ctx) {
    if (!ctx)
        return;

    if (ctx->decoder) {
        if (ctx->is_range)
            range_decoder_close(ctx->decoder);
        else
            dct_decoder_close(ctx->decoder);
    }

    free(ctx->blkbuf);
    free(ctx);
}

int icesnd_info(icesnd_handle_t* ctx, icesnd_info_t* info) {
    if (!ctx)
        goto fail;

    info->total_subsongs = ctx->hdr.total_subsongs;
    info->codec = ctx->etr.codec;
    info->sample_rate = ctx->etr.sample_rate;
    info->channels =  ctx->etr.channels;
    info->loop_start = ctx->etr.intro_samples;
    info->num_samples = ctx->etr.intro_samples + ctx->etr.body_samples;
    info->loop_flag = ctx->etr.loop_flag;

    return ICESND_RESULT_OK;
fail:
    return ICESND_ERROR_DECODE;
}

void icesnd_reset(icesnd_handle_t* ctx, int loop_start) {
    if (!ctx || !ctx->decoder)
        return;

    ctx->intro_init = 0;
    ctx->body_init = 0;
    ctx->intro_done = 0;

    /* skip intro block in some cases */
    if (ctx->etr.intro_samples == 0 || loop_start != 0)
        ctx->intro_done = 1;

    /* no need to reset decoder as will be done when block is set, plus
     * only reset properly when doing that */
}

static int setup_block(icesnd_handle_t* ctx, int intro) {
    int err;
    const uint8_t* buf;
    int buf_size;
    int block_offset = (intro ? ctx->intro_offset : ctx->body_offset);
    int block_size = (intro ? ctx->etr.intro_zsize : ctx->etr.body_zsize);
    int block_samples = (intro ? ctx->etr.intro_samples : ctx->etr.body_samples);

    if (ctx->cb.read) {
        /* could optimize by ignoring calls (intro > body > body...) but this kinda simulates streamings */
        if (block_size > ctx->blkbuf_size) /* can't happen but anyway */
            return ICESND_ERROR_DECODE;
        ctx->cb.seek(ctx->cb.arg, block_offset, SEEK_SET);
        buf_size = ctx->cb.read(ctx->blkbuf, 1, block_size, ctx->cb.arg);        
        buf = ctx->blkbuf;
    }
    else {
        if (ctx->cb.filebuf_size < block_offset + block_size)
            return ICESND_ERROR_DECODE;
        buf = ctx->cb.filebuf + block_offset;
        buf_size = ctx->cb.filebuf_size - block_offset;
    }


    if (ctx->is_range) {
        err = range_decoder_block_setup(ctx->decoder, buf, buf_size, &ctx->etr, block_samples);
    }
    else {
        err = dct_decoder_block_setup(ctx->decoder, buf, buf_size, &ctx->etr); /* max_samples info is in codebook table */
    }
    if (err < ICESND_RESULT_OK) return err;

    return ICESND_RESULT_OK;
}

int icesnd_decode(icesnd_handle_t* ctx, int16_t* sbuf, int max_samples) {
    int err;
    int samples_done, block_end;
    int samples_decoded = 0;
    if (!ctx)
        goto fail;

    while (max_samples > 0) {

        if (!ctx->intro_done) {
            if (!ctx->intro_init) {
                err = setup_block(ctx, 1);
                if (err < ICESND_RESULT_OK) return err;

                ctx->intro_init = 1;
            }
        }
        else {
            if (!ctx->body_init) {
                err = setup_block(ctx, 0);
                ctx->body_init = 1;
            }
        }

        if (ctx->is_range)
            block_end = range_decoder_decode(ctx->decoder, sbuf, max_samples, &samples_done);
        else
            block_end = dct_decoder_decode(ctx->decoder, sbuf, max_samples, &samples_done);

        max_samples -= samples_done;
        samples_decoded += samples_done;
        sbuf += samples_done * ctx->etr.channels;
        //ctx->curr_sample += samples_done; /* original keeps this around to test if intro block is done */

        if (block_end) {
            /* after first block (could check if this is the first block but whatevs) */
            ctx->intro_done = 1;

            /* intro end, or after body end to allow loops on next calls */
            if (ctx->etr.loop_flag)
                ctx->body_init = 0;
        }

        /* could be possible on block end if not reset */
        if (samples_done == 0)
            break;

        /* stop on on block boundary to ensure external caller may stop on loop end (could go on otherwise) */
        if (block_end)
            break;
    }


    return samples_decoded;
fail:
    return ICESND_ERROR_DECODE;
}
