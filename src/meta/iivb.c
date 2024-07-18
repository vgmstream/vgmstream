#include "meta.h"
#include "../util.h"
#include "../util/meta_utils.h"


/* IIVB - from Vingt-et-un Systems games [Langrisser III (PS2), Ururun Quest: Koiyuuki (PS2)] */
VGMSTREAM* init_vgmstream_iivb(STREAMFILE* sf) {

    /* checks */
    if (!is_id32be(0x00, sf, "BVII")) /* IIVB LE, given extension */
        return NULL;
    if (!check_extensions(sf,"ivb"))
        return NULL;


    meta_header_t h = {
        .meta = meta_IIVB,
    };
    h.chan_size     = read_u32le(0x04,sf);
    h.sample_rate   = read_s32be(0x08,sf); /* big endian? */
    // 0c: empty

    h.channels = 2;
    h.stream_offset = 0x10;
    h.num_samples = ps_bytes_to_samples(h.chan_size, 1);

    h.coding = coding_PSX;
    h.layout = layout_interleave;
    h.interleave = h.chan_size;

    h.sf = sf;
    h.open_stream = true;

    return alloc_metastream(&h);
}
