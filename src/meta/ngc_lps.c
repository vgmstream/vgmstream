#include "meta.h"
#include "../util/meta_utils.h"
#include "../coding/coding.h"

/* .LPS - from Rave Master (GC) */
VGMSTREAM* init_vgmstream_lps(STREAMFILE* sf) {

    /* checks */
    uint32_t data_size = read_u32be(0x00, sf);
    if (data_size + 0xE0 != get_streamfile_size(sf))
        return NULL;

    if (read_u32be(0x04, sf) != 0x01)
        return NULL;
    if (read_u32be(0x08, sf) != 0x10000000)
        return NULL;
    if (read_u32be(0x0c, sf) != 0x00)
        return NULL;
    if (!check_extensions(sf, "lps"))
        return NULL;

    meta_header_t h = {0};
    h.meta = meta_LPS;

    //TODO: standard(?) DSP header, maybe handle like others
    h.num_samples   = read_s32be(0x20 + 0x00,sf);
    // 04: nibbles
    h.sample_rate   = read_s32be(0x20 + 0x08,sf);
    h.loop_flag     = read_s16be(0x20 + 0x0c,sf) == 0x0001;
    h.loop_start    = read_u32be(0x20 + 0x10,sf);
    h.loop_end      = read_s32be(0x20 + 0x14,sf);
    h.coefs_offset  = 0x20 + 0x1c;
    h.hists_offset  = 0x20 + 0x1c + 0x20 + 0x04;

    h.loop_start    = dsp_nibbles_to_samples(h.loop_start);
    h.loop_end      = dsp_nibbles_to_samples(h.loop_end); //+ 1;

    h.channels      = 1;
    h.allow_dual_stereo = true;
    h.big_endian = true;
    h.stream_offset = 0xE0;

    h.coding = coding_NGC_DSP;
    h.layout = layout_none;
    h.open_stream = true;
    h.sf = sf;

    return alloc_metastream(&h);
}
