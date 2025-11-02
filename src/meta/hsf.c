#include "meta.h"
#include "../util/meta_utils.h"
#include "../util/spu_utils.h"

/* HSF - 'SoundBox' driver games (by CAPS?) [EX Jinsei Game (PS2), Lowrider (PS2), Professional Drift: D1 Grand Prix Series (PS2)] */
VGMSTREAM* init_vgmstream_hsf(STREAMFILE* sf) {

    /* checks */
    int version;
    if (is_id32be(0x00, sf, "HSF\0")) {
        version = 1; // "SBX driver version 1.0.0" [EX Jinsei Game (PS2)] / "2.1.0" [Lowrider (PS2)]
    }
    else if (is_id32be(0x00, sf, "HSF ")) {
        version = 3; // "SBX driver version 3.2.0" 
    }
    else {
        return NULL;
    }

    /* .hsf: actual extension in exes */
    if (!check_extensions(sf,"hsf"))
        return NULL;

    meta_header_t h = {0};
    //04: 0x00 in EX Jinsei Game, 0x03 in others (flags? sfx only in .hsb sound banks)
    h.sample_rate   = read_s32le(0x08,sf);
    h.interleave    = read_u32le(0x0c,sf);

    if (version < 3) { // pitch (48000 or 44100)
        h.sample_rate = spu2_pitch_to_sample_rate_rounded(h.sample_rate);
    }

    h.stream_offset = 0x10;
    h.stream_size   = get_streamfile_size(sf) - h.stream_offset;
    h.channels      = 2;
    h.num_samples   = ps_bytes_to_samples(h.stream_size, h.channels);

    h.coding = coding_PSX;
    h.layout = layout_interleave;

    h.sf = sf;
    h.open_stream = true;

    h.meta = meta_HSF;
    return alloc_metastream(&h);
}
