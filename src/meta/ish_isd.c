#include "meta.h"
#include "../coding/coding.h"
#include "../util/meta_utils.h"


/* ISH+ISD - from various games [Chaos Field (GC), Pokemon XD: Gale of Darkness (GC)] */
VGMSTREAM* init_vgmstream_ish_isd(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;

    /* checks */
    if (!is_id32be(0x00,sf, "I_SF"))
        return NULL;
    if (!check_extensions(sf, "ish"))
        return NULL;

    meta_header_t h = {0};
    h.meta = meta_ISH_ISD,

    //04: tool date in hex? (0x2004 01 09)
    h.sample_rate   = read_s32be(0x08,sf);
    h.num_samples   = read_s32be(0x0C,sf);
    //10: stream size
    h.channels      = read_s32be(0x14,sf);
    h.interleave    = read_u32be(0x18,sf);
    h.loop_flag     = read_s32be(0x1C,sf) != 0;
    h.loop_start    = read_s32be(0x20,sf);
    h.loop_end      = read_s32be(0x24,sf);
    //28: padding
    h.coefs_offset  = 0x40;
    h.coefs_spacing = 0x40;
    h.big_endian = true;

    //TODO: loop start may be set mid-frame, which bytes_to_samples doesn't handle
    //h.loop_start = dsp_bytes_to_samples(h.loop_start, h.channels);
    //h.loop_end = dsp_bytes_to_samples(h.loop_end, h.channels);
    h.loop_start = h.loop_start * 14 / 0x08 / h.channels;
    h.loop_end = h.loop_end * 14 / 0x08 / h.channels;

    h.stream_offset = 0x00;

    h.sf_head = sf;
    h.sf_body = open_streamfile_by_ext(sf,"isd");
    if (!h.sf_body) goto fail;

    h.coding = coding_NGC_DSP;
    h.layout = layout_interleave;
    h.open_stream = true;

    vgmstream = alloc_metastream(&h);
    close_streamfile(h.sf_body);
    return vgmstream;
fail:
    close_streamfile(h.sf_body);
    close_vgmstream(vgmstream);
    return NULL;
}
