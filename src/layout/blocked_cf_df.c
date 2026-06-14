#include "layout.h"
#include "../vgmstream.h"
#include "../util/reader_sf.h"

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
