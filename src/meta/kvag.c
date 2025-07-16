#include "meta.h"
#include "../util/meta_utils.h"

/* KVAG - Mattel Hyperscan games [Marvel Heroes (Hyperscan), X-Men (Hyperscan)] */
VGMSTREAM* init_vgmstream_hyperscan_kvag(STREAMFILE* sf) {

    /* checks */
    if (!is_id32be(0x00,sf, "KVAG"))
        return NULL;
    if (!check_extensions(sf,"bvg"))
        return NULL;

    meta_header_t h = {0};
    h.meta = meta_KVAG;

    h.stream_size   = read_u32le(0x04, sf);
    h.sample_rate   = read_s32le(0x08, sf);
    h.channels      = read_s16le(0x0c, sf) + 1;
    h.stream_offset = 0x0e;

    if (h.channels > 2) // 2ch is rare [Ben 10's Theme22ks]
        return NULL;

    h.num_samples = ima_bytes_to_samples(h.stream_size, h.channels);

    h.coding = coding_DVI_IMA;
    h.layout = layout_none;
    h.open_stream = true;
    h.sf = sf;

    return alloc_metastream(&h);
}
