#include "meta.h"
#include "../util/meta_utils.h"
#include "../util/spu_utils.h"

/* WMW - from Artoon games [Ghost Vibration (PS2)] */
VGMSTREAM* init_vgmstream_wmw(STREAMFILE* sf) {

    /* checks */
    if (!is_id32be(0x00, sf, "WMW "))
        return NULL;

    if (!check_extensions(sf,"wmw"))
        return NULL;

    meta_header_t h = {0};

    int format      =    read_u8(0x04,sf);
    h.loop_flag     =    read_u8(0x05,sf);
    //0x06: bps
    h.channels      =    read_u8(0x07,sf);
    h.sample_rate   = read_s32le(0x08,sf);
    h.stream_offset = read_u32le(0x0c,sf);
    h.stream_size   = read_u32le(0x10,sf);
    h.loop_start    = read_s32le(0x14,sf);
    h.loop_end      = read_s32le(0x18,sf);
    //0x1c: null
    //0x28/2c: step L/R (used? always 0x7F but already applied on decoder)

    // from debug code/strings format 1 is PCM (not seen); it also mentions "Aica Adpcm".
    if (format != 0x02) {
        return NULL; 
    }

    h.num_samples   = yamaha_bytes_to_samples(h.stream_size, h.channels);
    h.loop_start    = yamaha_bytes_to_samples(h.loop_start, h.channels);
    h.loop_end      = yamaha_bytes_to_samples(h.loop_end, h.channels);

    h.coding = coding_AICA;
    h.layout = layout_none;

    h.sf = sf;
    h.open_stream = true;

    h.meta = meta_WMW;
    VGMSTREAM* v = alloc_metastream(&h);
    if (!v) return NULL;

    v->codec_config = 1;  //high nibble = L
    return v;
}
