#include "meta.h"
#include "../util/meta_utils.h"

/* SSND - The 3DS Company games [Warriors of Might & Magic (PS2), Portal Runner (PS2), ] */
VGMSTREAM* init_vgmstream_ssnd(STREAMFILE* sf) {

    /* checks */
    if (!is_id32be(0x00, sf, "SSND"))
        return NULL;
    if (!check_extensions(sf,"snd"))
        return NULL;

    meta_header_t h = {0};
    h.stream_offset = read_u32le(0x04,sf) + 0x08;
    uint16_t codec  = read_u16le(0x08,sf);
    h.channels      = read_u16le(0x0a,sf);
    //0c: bps? (always 16)
    h.sample_rate   = read_u32le(0x0e,sf);
    h.interleave    = read_u32le(0x12,sf);
    h.num_samples   = read_s32le(0x16,sf);
    // rest: padding (may be null with stream_offset = 0x1a)

    h.loop_flag     = true; // force full loop //TODO: needed?
    h.loop_start    = 0;
    h.loop_end      = h.num_samples;

    h.stream_size   = get_streamfile_size(sf) - h.stream_offset;
    if (h.interleave)
        h.interleave_last = (h.stream_size % (h.interleave * h.channels)) / h.channels;

    h.layout = layout_interleave;
    switch(codec) {
        case 0x00: // Heroes of Might and Magic: Quest for the DragonBone Staff (PS2)
            h.coding = coding_PCM16LE;
            break;
        case 0x01: // others
            h.coding = coding_DVI_IMA_mono;
            break;
        default:
            return NULL;
    }

    h.sf = sf;
    h.open_stream = true;

    h.meta = meta_SSND;
    return alloc_metastream(&h);
}
