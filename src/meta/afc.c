#include "meta.h"
#include "../util/endianness.h"
#include "../util/meta_utils.h"


/* .AFC - from Nintendo games [Super Mario Sunshine (GC), The Legend of Zelda: Wind Waker (GC), Pikmin (Switch)] */
VGMSTREAM* init_vgmstream_afc(STREAMFILE* sf) {

    /* checks */
    /* .afc: common
     * .stx: Pikmin (GC/Switch) */
    if (!check_extensions(sf, "afc,stx"))
        return NULL;

    meta_header_t hdr = {0};
    hdr.big_endian = guess_endian16(0x0a,sf);

    read_u32_t read_u32 = hdr.big_endian ? read_u32be : read_u32le;
    read_s32_t read_s32 = hdr.big_endian ? read_s32be : read_s32le;
    read_u16_t read_u16 = hdr.big_endian ? read_u16be : read_u16le;

    if (read_u32(0x00, sf) > get_streamfile_size(sf)) // size without padding
        return NULL;
    hdr.num_samples = read_s32(0x04,sf);
    hdr.sample_rate = read_u16(0x08,sf);
    if (read_u16(0x0a, sf) != 4) /* bps? */
        return NULL;
    if (read_u16(0x0c, sf) != 16) /* samples per frame? */
        return NULL;
    // 0x0e: always 0x1E?

    hdr.loop_flag   = read_s32(0x10, sf); // 1
    hdr.loop_start  = read_s32(0x14, sf);
    // 0x18: null
    // 0x20: null
    hdr.stream_offset = 0x20;

    hdr.channels = 2;
    hdr.loop_end = hdr.num_samples;

    hdr.coding = coding_AFC;
    hdr.layout = layout_interleave;
    hdr.interleave = 0x09;

    hdr.sf = sf;
    hdr.open_stream = true;

    hdr.meta = meta_AFC;
    return alloc_metastream(&hdr);
}
