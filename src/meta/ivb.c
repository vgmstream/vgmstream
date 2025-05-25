#include "meta.h"
#include "../util.h"
#include "../util/meta_utils.h"
#include "ivb_streamfile.h"


/* IVB - from Metro PS2 games [Bomberman Jetters (PS2), Dance Summit 2001: Bust A Move (PS2)] */
VGMSTREAM* init_vgmstream_ivb(STREAMFILE* sf) {

    /* checks */
    if (!is_id32be(0x00, sf, "IVB\0"))
        return NULL;
    if (!check_extensions(sf,"ivb"))
        return NULL;

    meta_header_t h = {0};
    h.meta = meta_IVB;

    // N stereo tracks, treat as subsongs instead of channels as both may have different total time
    // and aren't meant to play at once, though data is padded so tracks have same size

    h.total_subsongs = read_s32le(0x04, sf); // always 2 tracks
    h.interleave = read_s32le(0x08, sf);
    // 0c: null

    // per track
    // 00: channel size
    // 04: channel blocks (may be less than channel size due to padding and different per track)
    // 08: size of last block (1 channel)
    // 0c: null

    int target_subsong = sf->stream_index;
    if (target_subsong == 0)
        target_subsong = 1;

    uint32_t head_offset = 0x10 + (target_subsong - 1) * 0x10;
    //uint32_t chan_size    = read_u32le(head_offset + 0x00, sf); //with padding
    uint32_t chan_blocks    = read_u32le(head_offset + 0x04, sf);
    uint32_t last_size      = read_u32le(head_offset + 0x08, sf); // last block without padding

    h.channels = 2;
    h.sample_rate = 44100;
    h.has_subsongs = true;

    h.coding = coding_PSX;
    h.layout = layout_interleave;

    h.stream_offset = 0x00;
    h.stream_size = (chan_blocks - 1) * h.interleave * h.channels + last_size * h.channels;

    h.num_samples = ps_bytes_to_samples(h.stream_size, h.channels);

    h.sf = setup_ivb_streamfile(sf, 0x800, h.total_subsongs, target_subsong - 1, h.interleave * h.channels);
    h.open_stream = true;

    VGMSTREAM* v = alloc_metastream(&h);
    close_streamfile(h.sf);
    return v;
}
