#include "meta.h"
#include "../util/meta_utils.h"

/* VMS - from Davilex games [Autobahn Raser: Police Madness (PS2)] */
VGMSTREAM* init_vgmstream_vms(STREAMFILE* sf) {

    /* checks */
    if (!is_id32be(0x00, sf, "VMS "))
        return NULL;
    if (!check_extensions(sf,"vms"))
        return NULL;

    meta_header_t h = {0};
    //04: channels? version?
    h.channels      = read_u8(0x08,sf); //always 1?
    //0c: total frames
    h.interleave    = read_u32le(0x10,sf);
    h.sample_rate   = read_s32le(0x14,sf);
    // 08: 0x20?
    h.stream_offset = read_u32le(0x1c,sf);
    // 20: VAGp header

    h.stream_size   = get_streamfile_size(sf) - h.stream_offset;
    h.num_samples   = ps_bytes_to_samples(h.stream_size, h.channels);
    // some tracks have flags and do full loops, but other that don't need to loop set them too
    //h.loop_flag = ps_find_loop_offsets(sf, h.stream_offset, h.stream_size, h.channels, h.interleave, &h.loop_start, &h.loop_end);

    h.coding = coding_PSX;
    h.layout = layout_interleave;

    h.sf = sf;
    h.open_stream = true;

    h.meta = meta_VMS;
    return alloc_metastream(&h);
}
