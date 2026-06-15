#include "layout.h"
#include "../vgmstream.h"
#include "../util/reader_sf.h"
#include "../coding/coding.h"

/* CyberFlix DreamFactory streamed audio (disk-stream movies).
 *
 * The soundtrack is stored as many small audio chunks scattered among video frames, played in
 * the order listed by the "loop block" in container 1. Rather than concatenating hundreds of
 * sub-streams, this blocked layout walks that loop list directly: each block is one chunk, and
 * the current loop-list entry position is carried in current_block_offset (entries are 26 bytes).
 *
 * Loop block (container 1 payload): u32 unk, u16 orderCount, s16 order[], then at +0x06+260:
 *   u16 loopCount, +4 pad, loopCount * { u32 unk, u16 blockId (+4), +2, u8 nameLen, char[15] }.
 * blockId indexes the global container offset table at 0x400. Each chunk re-anchors its codec
 * state, so blocks decode correctly back to back.
 */

#define DF_HEADER_SIZE  0x400
#define DF_ORDER_REGION 260
#define DF_LOOP_ENTRY   26
#define DF_BLOCK_END    UINT32_MAX

void block_update_cf_df(off_t block_offset, VGMSTREAM* vgmstream) {
    STREAMFILE* sf = vgmstream->ch[0].streamfile;

    vgmstream->current_block_offset = block_offset;

    /* end sentinel (only reached on over-read/seek past the last block) */
    if (block_offset == DF_BLOCK_END) {
        vgmstream->current_block_size = 0;
        vgmstream->current_block_samples = 0;
        vgmstream->next_block_offset = DF_BLOCK_END;
        return;
    }

    /* loop list geometry, re-derived from the file (no stored state) */
    off_t loop_payload = read_u32le(DF_HEADER_SIZE + 0x01 * 0x04, sf) + 0x08;
    off_t first_entry = loop_payload + 0x06 + DF_ORDER_REGION + 0x04;
    int loop_count = read_u16le(loop_payload + 0x06 + DF_ORDER_REGION, sf);
    int index = (int)((block_offset - first_entry) / DF_LOOP_ENTRY);

    /* resolve entry's audio chunk */
    int block_id = read_u16le(block_offset + 0x04, sf);
    off_t chunk_pos = read_u32le(DF_HEADER_SIZE + block_id * 0x04, sf);
    off_t header_pos = chunk_pos + 0x08;
    uint32_t data_offset = read_u32le(header_pos + 0x2C, sf);
    uint32_t uncompressed = read_u32le(header_pos + 0x24, sf);

    vgmstream->ch[0].offset = header_pos + data_offset;
    vgmstream->current_block_size = read_u32le(chunk_pos + 0x04, sf) - data_offset;

    if (vgmstream->coding_type == coding_CF_DF_DPCM_V41)
        vgmstream->current_block_samples = uncompressed / 2;
    else
        vgmstream->current_block_samples = uncompressed;

    if (index + 1 < loop_count)
        vgmstream->next_block_offset = block_offset + DF_LOOP_ENTRY;
    else
        vgmstream->next_block_offset = DF_BLOCK_END;
}

/* CyberFlix DreamFactory v5 SOUN
 *
 * A SOUN payload (base H = container + 0x08) holds a block-offset table at H+0x2c with
 * H+0x28 entries; each entry is an input byte offset relative to H, so block k spans
 * [H + table[k], H + table[k+1]) (the last runs to the SOUN payload end = container_size).
 * The codec is fixed per stream (set as coding_type by the meta) and codec state RESETS at
 * every block:
 *   - IMA  (coding_CF_DF_V5_IMA):  3-byte header (s16 predictor + u8 step), data at +3;
 *                                  a block with step > 0x58 produces no samples.
 *   - v4.0 (coding_CF_DF_V5_ADPCM): byte[0] seeds the running sample (not emitted), data +1.
 *   - v4.1 (coding_CF_DF_DPCM_V41): accumulator reset to 0, data at +0.
 *
 * current_block_offset walks the *table entries* (H+0x2c + k*4); H is recovered from
 * channel_start_offset (= the first table entry the meta opened at). See src/meta/cf_df.c.
 */

#define DF_V5_TABLE_BASE   0x2c   /* block-offset table at H + 0x2c */
#define DF_V5_BLOCK_COUNT  0x28   /* block count at H + 0x28 */


void block_update_cf_df_v5(off_t block_offset, VGMSTREAM* vgmstream) {
    STREAMFILE* sf = vgmstream->ch[0].streamfile;

    vgmstream->current_block_offset = block_offset;

    if (block_offset == DF_BLOCK_END) {
        vgmstream->current_block_size = 0;
        vgmstream->current_block_samples = 0;
        vgmstream->next_block_offset = DF_BLOCK_END;
        return;
    }

    off_t table_base = vgmstream->ch[0].channel_start_offset; /* = H + 0x2c */
    off_t H = table_base - DF_V5_TABLE_BASE;
    int count = read_u32le(H + DF_V5_BLOCK_COUNT, sf);
    int index = (int)((block_offset - table_base) / 0x04);

    uint32_t rel = read_u32le(block_offset, sf);
    uint32_t next_rel = (index + 1 < count)
            ? read_u32le(block_offset + 0x04, sf)
            : read_u32le(H - 0x04, sf); /* container_size @ pos+0x04 = SOUN payload end */
    int block_size = (int)(next_rel - rel);
    off_t block_data = H + rel;

    int32_t samples;
    switch (vgmstream->coding_type) {
        case coding_CF_DF_V5_IMA: {
            int step_index = read_u8(block_data + 0x02, sf);
            vgmstream->ch[0].adpcm_history1_32 = read_s16le(block_data + 0x00, sf);
            vgmstream->ch[0].adpcm_step_index  = step_index;
            vgmstream->ch[0].offset = block_data + 0x03;
            /* step > 0x58 -> the decoder writes nothing (whole block skipped) */
            samples = (step_index > 0x58) ? 0 : (1 + 2 * (block_size - 3));
            break;
        }
        case coding_CF_DF_V5_ADPCM: /* v4.0 */
            vgmstream->ch[0].adpcm_history1_16 = read_s8(block_data, sf); /* seed, not emitted */
            vgmstream->ch[0].offset = block_data + 0x01;
            samples = cf_df_v5_v40_block_samples(sf, block_data, block_size);
            break;
        case coding_CF_DF_DPCM_V41:
        default:
            vgmstream->ch[0].adpcm_history1_16 = 0; /* reset accumulator per block */
            vgmstream->ch[0].offset = block_data;
            samples = block_size; /* one sample per input byte */
            break;
    }

    vgmstream->current_block_size = block_size;
    vgmstream->current_block_samples = samples;
    vgmstream->next_block_offset = (index + 1 < count) ? (block_offset + 0x04) : DF_BLOCK_END;
}
