#include "meta.h"
#include "../util/meta_utils.h"
#include "../coding/coding.h"

/* .NST - from Animaniacs: The Great Edgar Hunt (GC) */
VGMSTREAM* init_vgmstream_nst_monster(STREAMFILE* sf) {

    /* checks */
    if (read_u32be(0x00,sf) != 1)
        return NULL;
    // .nst: original
    // .dsp: renamed for plugins (to be removed?)
    if (!check_extensions(sf, "nst,dsp"))
        return NULL;

    // DSP header but second is just a dummy, both channels use the same coef table (0x20)
    if (read_u32be(0x00,sf) != read_u32be(0x54,sf))
        return NULL;
    if (read_u32be(0x04,sf) != read_u32be(0x58,sf))
        return NULL;
    if (read_u32be(0x08,sf) != read_u32be(0x5C,sf))
        return NULL;
    if (read_u32be(0x0C,sf) != read_u32be(0x60,sf))
        return NULL;

    meta_header_t h = {0};
    h.meta = meta_NST_MONSTER;

    h.num_samples   = read_s32be(0x08, sf);
    h.sample_rate   = read_s32be(0x14, sf);

    h.channels = 2;
    h.interleave = 0x10;
    h.coefs_offset  = 0x20;
    h.coefs_spacing = 0x00;
    h.big_endian = true;
    //h.hists_offset  = 0x00; //?
    //h.hists_spacing = h.coefs_spacing;

    h.stream_offset = 0xAC;

    h.coding = coding_NGC_DSP;
    h.layout = layout_interleave;
    h.open_stream = true;
    h.sf = sf;

    return alloc_metastream(&h);
}
