#include "meta.h"
#include "../coding/coding.h"
#include "../util/meta_utils.h"

/* .VGV - from Rune: Viking Warlord (PS2) */
VGMSTREAM* init_vgmstream_vgv(STREAMFILE* sf) {

    /* checks */
    if (read_u32le(0x00,sf) < 22050 || read_u32le(0x00,sf) > 48000) // always 22050?
        return NULL;
    if (read_f32le(0x04,sf) == 0.0 || read_f32le(0x04,sf) > 500.0) //duration, known max is ~432.08 = 7:12
        return NULL;
    if (read_u32le(0x08,sf) != 0x00 || read_u32le(0x0c,sf) != 0x00)
        return NULL;

    if (!check_extensions(sf, "vgv"))
        return NULL;

    meta_header_t h = {0};
    h.meta  = meta_VGV;

    h.channels      = 1;
    h.sample_rate   = read_s32le(0x00, sf);
    h.stream_offset = 0x10;     
    h.stream_size   = get_streamfile_size(sf);
    h.num_samples   = ps_bytes_to_samples(h.stream_size, h.channels);

    h.coding = coding_PSX;
    h.open_stream = true;
    h.sf = sf;

    return alloc_metastream(&h);
}
