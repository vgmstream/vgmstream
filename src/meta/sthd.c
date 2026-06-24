#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "../util/layout_utils.h"

/* STHD - from Dream Factory games [Kakuto Chojin (Xbox), Dinosaur Hunting (Xbox), Phantom Dust Remaster (PC)] */
VGMSTREAM* init_vgmstream_sthd(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset;
    int loop_flag, channels, sample_rate;
    int loop_start_block, loop_end_block;
    uint32_t build_date;

    /* checks */
    if (!is_id32be(0x00,sf, "STHD"))
        return NULL;
    if (!check_extensions(sf, "stx"))
        return NULL;

    /* all blocks have STHD and have these values up to 0x20, size 0x800 */
    start_offset = read_u16le(0x04,sf); /* next block in header, data offset in other blocks */
    channels = read_s16le(0x06,sf);
    build_date = read_u32le(0x08,sf); /* in hex (0x20030610 = 2003-06-10) */
    // 0x0c: ? (1 in Dinosaur Hunting, otherwise 0)

    if (start_offset != 0x0800 || channels > 8)
        return NULL;

    // 0x10: total blocks
    // 0x12: block number
    // 0x14: null
    // 0x16: channel size (0 in header block)
    // 0x18: block number + 1?
    loop_start_block = read_u16le(0x1a,sf);
    loop_end_block   = read_u16le(0x1c,sf);

    loop_flag = loop_start_block != 0xFFFF;
    /* may be a bug since STHD blocks don't reach max (loop start seems fine) [Phantom Dust Remaster (PC)] */
    if (build_date >= 0x20170000)
        loop_end_block--;

    /* channel info (first block only), seem to be repeated up to max 8 channels */
    sample_rate = read_s32le(0x20, sf);
    // 0x24/28: volume/pan? (not always set)
    // 0x210: stream name for both channels (same as file)


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_STHD;
    vgmstream->sample_rate = sample_rate;

    vgmstream->coding_type = build_date >= 0x20170000 ? // no apparent flags [Phantom Dust Remaster (PC)]
          coding_PCM16LE :
          coding_XBOX_IMA_mono;
    vgmstream->layout_type = layout_blocked_sthd;

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;

    {
        blocked_counter_t cfg = {0};
        cfg.offset = start_offset;
        if (loop_flag && loop_start_block > 0 && loop_end_block > 0) {
            /* header block = 0 */
            cfg.loop_flag = loop_flag;
            cfg.loop_start_block = loop_start_block - 1;
            cfg.loop_end_block = loop_end_block - 1;
        }

        blocked_count_samples(vgmstream, sf, &cfg);
    }

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
