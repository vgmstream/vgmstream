#include "meta.h"
#include "../coding/coding.h"
#include "../util/meta_utils.h"

/* MADP - from Capcom 3DS games */
VGMSTREAM* init_vgmstream_madp(STREAMFILE* sf) {

    /* checks */
    if (!is_id32be(0x00,sf, "MADP"))
        return NULL;
    if (!check_extensions(sf, "mca"))
        return NULL;

    meta_header_t h = {0};
    h.meta = meta_MADP;

    int version         = read_u16le(0x04, sf);
    h.channels          = read_u16le(0x08, sf);
    h.interleave        = read_u16le(0x0a, sf); // assumed, only seen 0x100
    h.num_samples       = read_s32le(0x0c, sf);
    h.sample_rate       = read_s32le(0x10, sf);
    h.loop_start        = read_s32le(0x14, sf);
    h.loop_end          = read_s32le(0x18, sf);
    h.head_size         = read_u32le(0x1c, sf); // v3=loop related?, v5=partial size?
    h.stream_size       = read_u32le(0x20, sf);
    // 24: duration (f32)
    // rest: varies between versions

    int cues = 0;
    if (version >= 0x04) {
        cues = read_u16le(0x28, sf); //seems to be some kind of seek table with start ps + hist per channel
        // 0x2a: id-ish value? (same for all files in a game)
    }

    // format is kind of inconsistent between games but the following seems to work
    if (version == 3)
        h.head_size = get_streamfile_size(sf) - h.stream_size; // probably 0x2c + 0x30*ch
    h.coefs_spacing = 0x30;
    uint32_t coefs_start = (h.head_size - h.coefs_spacing * h.channels);
    h.coefs_offset = coefs_start + cues * 0x14;
    // hist + start/loop ps seem to follow after coefs

    switch(version) {
        case 0x03: // Resident Evil Mercenaries 3D, Super Street Fighter IV 3D
            h.stream_offset = h.head_size;
            break;

        case 0x04: // EX Troopers, Ace Attourney 5
            h.stream_offset = get_streamfile_size(sf) - h.stream_size; // usually head_size but not for some MH3U songs
            break;

        case 0x05: // Ace Attourney 6, Monster Hunter Generations
            h.stream_offset = read_u32le(coefs_start - 0x04, sf);
            break;

        default:
            return NULL;
    }

    h.loop_flag = h.loop_end > 0;
    if (h.loop_end > h.num_samples) // some MH3U songs, somehow
        h.loop_end = h.num_samples;

    h.coding = coding_NGC_DSP;
    h.layout = layout_interleave;
    h.open_stream = true;
    h.sf = sf;

    return alloc_metastream(&h);
}
