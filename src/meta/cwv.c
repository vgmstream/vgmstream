#include "meta.h"
#include "../util/endianness.h"
#include "../util/meta_utils.h"


/* .CWV - from Nintendo games [Rhythm Paradise Groove (Switch)] */
VGMSTREAM* init_vgmstream_cwv(STREAMFILE* sf) {

    /* checks */
    uint32_t file_size = get_streamfile_size(sf);
    if (file_size < 0x1000) // min aligment
        return NULL;
    if (!check_extensions(sf, "cwv"))
        return NULL;

    // .cwv are loose files with a footer, fully read and decoded in memory. There doesn't seem to be
    // debug strings or anything to identify this format, but OG code checks that extension is .cwv before decoding.
    // Decoder also aligns dst buffer to 0x1000, and seems to copy the footer than to the decoded data.
    uint32_t footer_offset = file_size - 0x100;

    meta_header_t hdr = {0};

    uint32_t flags  = read_u32le(footer_offset + 0x00, sf);
    // OG code does check that only bitflag 1 is set
    if ((flags & 0xFFFFFFFE) != 2)
        return NULL;

    hdr.sample_rate = read_s32le(footer_offset + 0x04, sf);
    hdr.num_samples = read_s32le(footer_offset + 0x08, sf);
    uint32_t mode   = read_s32le(footer_offset + 0x0C, sf);
    hdr.loop_start  = read_s32le(footer_offset + 0x10, sf);
    hdr.loop_end    = read_s32le(footer_offset + 0x14, sf);
    // 18: f32 volume? (usually 0.7~1.0)
    // 1c: f32 pan? (usually 1.0)
    // 20: f32 pitch? (usually 0)
    // 24: u32 (usually 0 or 1)
    // 28: f32 (usually 0.2~0.5)
    // 2c: f32? (usually 0)
    // 30: f32 (usually 1.0)
    // 34+: 0 until stream name
    hdr.name_offset = footer_offset + 0x4c; // size 0xB4

    if (hdr.sample_rate < 32000 || hdr.sample_rate > 48000)
        return NULL;
    if (hdr.num_samples < 0 || hdr.num_samples > 0x10000000)
        return NULL;
    if (mode != 0x02)
        return NULL;

    hdr.channels = flags & 0x01 ? 2 : 1;
    hdr.loop_flag = hdr.loop_end > 0; // 0 if not set
    hdr.loop_end += hdr.loop_start;

    hdr.coding = coding_CWV;
    hdr.layout = layout_interleave;
    hdr.interleave = 0x01;

    hdr.sf = sf;
    hdr.open_stream = true;

    hdr.meta = meta_CWV;
    return alloc_metastream(&hdr);
}
