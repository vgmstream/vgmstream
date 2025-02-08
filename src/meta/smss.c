#include "meta.h"
#include "../util/meta_utils.h"
#include "../coding/coding.h"


/* SMSS - from Tiny Toon Adventures: Defenders of the Universe (PS2) */
VGMSTREAM* init_vgmstream_smss(STREAMFILE *sf) {

    if (!is_id32be(0x00,sf, "SMSS"))
        return NULL;
    if (!check_extensions(sf, "vsf"))
        return NULL;

    meta_header_t h = {0};
    h.meta = meta_SMSS;

    // 04: header size?
    h.interleave    = read_u32le(0x08,sf);
    h.channels      = read_s32le(0x0c,sf);
    h.sample_rate   = read_s32le(0x10,sf);
    // 14: null?
    h.loop_start    = read_u32le(0x18,sf);
    h.loop_end      = read_u32le(0x1c,sf);
    // rest: padding


    h.loop_flag     = h.loop_start > 0;
    h.stream_offset = 0x800;
    h.stream_size   = get_streamfile_size(sf) - h.stream_offset;

    h.num_samples   = ps_bytes_to_samples(h.stream_size, h.channels);
    h.loop_start    = ps_bytes_to_samples(h.loop_start, 1);
    h.loop_end      = ps_bytes_to_samples(h.loop_end, 1);

    h.coding = coding_PSX;
    h.layout = layout_interleave;
    h.open_stream = true;
    h.sf = sf;

    return alloc_metastream(&h);
}
