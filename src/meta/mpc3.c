#include "meta.h"
#include "../util/meta_utils.h"


/* MPC3 - from Paradigm games [Spy Hunter (PS2), MX Rider (PS2), Terminator 3 (PS2)] */
VGMSTREAM* init_vgmstream_mpc3(STREAMFILE* sf) {

    /* checks */
    if (!is_id32be(0x00,sf, "MPC3"))
        return NULL;
    if (read_u32be(0x04,sf) != 0x00011400) /* version 1.14? */
        return NULL;
    if (!check_extensions(sf,"mc3"))
        return NULL;

    meta_header_t hdr = {0};

    hdr.channels        = read_u32le(0x08, sf);
    hdr.sample_rate     = read_s32le(0x0c, sf);
    hdr.num_samples     = read_s32le(0x10, sf);
    hdr.interleave      = read_u32le(0x14, sf); // block
    hdr.stream_size     = read_u32le(0x18, sf); // block size
    hdr.stream_offset   = 0x1c;

    if (hdr.channels > 2) /* decoder max */
        return NULL;
    if (!check_file_size(sf, hdr.stream_offset, hdr.stream_size) )
        return NULL;

    if (hdr.num_samples > 0xFFFFFFF)
        return NULL;
    hdr.num_samples *= 10; // sizes in sub-blocks of 10 samples (without headers)

    hdr.interleave = (hdr.interleave * 0x04 * hdr.channels) + 0x04;
    if (hdr.interleave < 0x0c) // div-by-zero in decoder calcs
        return NULL;

    hdr.meta = meta_MPC3;
    hdr.coding = coding_MPC3;
    hdr.layout = layout_none;

    hdr.sf = sf;
    hdr.open_stream = true;

    return alloc_metastream(&hdr);
}
