#include "meta.h"
#include "../util/meta_utils.h"

/* WPD - from Shuffle! (PC) */
VGMSTREAM* init_vgmstream_wpd(STREAMFILE* sf) {

    /* checks */
    if (!is_id32be(0x00,sf, " DPW"))
        return NULL;
    if (!check_extensions(sf, "wpd"))
        return NULL;

    meta_header_t h = {0};
    h.meta = meta_WPD;
    h.channels      = read_u32le(0x04,sf); /* always 2? */
    // 08: always 2?
    // 0c: bits per sample (16)
    h.sample_rate   = read_s32le(0x10,sf); /* big endian? */
    h.data_size     = read_u32le(0x14,sf);
    // 18: PCM fmt (codec 0001, channels, srate, bitrate...)

    h.stream_offset = 0x30;
    h.num_samples = pcm16_bytes_to_samples(h.data_size, h.channels);

    h.coding = coding_PCM16LE;
    h.layout = layout_interleave;
    h.interleave = 0x02;

    h.sf = sf;
    h.open_stream = true;

    return alloc_metastream(&h);
}
