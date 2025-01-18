#include "meta.h"
#include "../util/meta_utils.h"
#include "../coding/coding.h"


/* i3DS - interleaved dsp [F1 2011 (3DS)] */
VGMSTREAM* init_vgmstream_i3ds(STREAMFILE* sf) {

    /* checks */
    if (!is_id32be(0x00,sf, "i3DS"))
        return NULL;
    if (!check_extensions(sf, "3ds"))
        return NULL;

    meta_header_t h = {
        .meta = meta_I3DS
    };

    // 04: data start? (0x10)
    h.data_size     = read_u32le(0x08,sf);
    h.interleave    = read_u32le(0x0c,sf);

    // 2 mono CWAV headers pasted together then "DATA" then stream, no loop info but many tracks repeat
    if (!is_id32be(0x10,sf, "CWAV"))
        return NULL;

    h.sample_rate   = read_s32le(0x10 + 0x4c, sf);
    h.num_samples   = read_s32le(0x10 + 0x54, sf);
    h.coefs_offset  = 0x10 + 0x7c;
    h.coefs_spacing = 0xC0;
    //h.hists_offset  = 0x00; //?
    //h.hists_spacing = h.coefs_spacing;

    // interleaved data starts from 0x10 after DATA (chunk *2), so unsure if header's offset are actually used
    uint32_t chdt_offset = 0x08 + 0x08; //read_u32le(0x10 + 0x6c, sf) + 0x08;

    h.channels = h.interleave ? 2 : 1;
    h.stream_offset = 0x10 + 0xc0 * h.channels + chdt_offset;
    h.stream_size = h.data_size - h.stream_offset;
    if (h.interleave > 0) 
        h.interleave_last = (h.stream_size % (h.interleave * h.channels)) / h.channels;

    h.coding = coding_NGC_DSP;
    h.layout = layout_interleave;
    h.open_stream = true;
    h.sf = sf;

    return alloc_metastream(&h);
}
