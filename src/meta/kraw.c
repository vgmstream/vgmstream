#include "meta.h"
#include "../coding/coding.h"
#include "../util/meta_utils.h"

/* KRAW - from Geometry Wars: Galaxies (Wii) */
VGMSTREAM* init_vgmstream_kraw(STREAMFILE* sf) {

    /* checks */
    if (!is_id32be(0x00,sf, "kRAW"))
        return NULL;
    // .kRAW: actual extension
    if (!check_extensions(sf, "kraw"))
        return NULL;

    meta_header_t h = {0};
    h.data_size     = read_u32be(0x04,sf);

    h.meta  = meta_KRAW;

    h.stream_offset = 0x08;
    h.channels      = 1;
    h.sample_rate   = 32000;
    h.num_samples   = pcm16_bytes_to_samples(h.data_size, h.channels);
    h.allow_dual_stereo = true;

    h.coding = coding_PCM16BE;
    h.layout = layout_none;
    h.open_stream = true;
    h.sf = sf;

    return alloc_metastream(&h);
}
