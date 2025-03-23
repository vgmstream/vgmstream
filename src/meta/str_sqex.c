#include "meta.h"
#include "../util/meta_utils.h"
#include "../coding/coding.h"


/* STR - from Final Fantasy Crystal Chronicles (GC) */
VGMSTREAM* init_vgmstream_str_sqex(STREAMFILE* sf) {

    /* checks */
    if (!is_id32be(0x00,sf, "STR\0"))
        return NULL;
    if (!check_extensions(sf, "str"))
        return NULL;

    if (read_u32be(0x04,sf) != 0)
        return NULL;
    if (read_u32be(0x08,sf) != get_streamfile_size(sf))
        return NULL;

    meta_header_t h = {0};
    h.meta = meta_STR_SQEX;

    h.num_samples = read_s32be(0x0C, sf) * 14;
    // 10: always -1 (loop point?)
    h.sample_rate   = read_u32be(0x14, sf) != 0 ? 44100 : 32000; // unknown value
    h.channels      = read_s32be(0x18, sf);
    // 1c: volume? (128)
    h.coefs_offset  = 0x20;
    h.coefs_spacing = 0x2e;
    h.big_endian = true;
    // 40: initial ps
    // 48: hist?

    h.interleave    = 0x1000;
    h.stream_offset = 0x1000;

    h.coding = coding_NGC_DSP;
    h.layout = layout_interleave;
    h.open_stream = true;
    h.sf = sf;

    return alloc_metastream(&h);
}
