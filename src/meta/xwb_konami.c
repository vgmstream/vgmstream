#include "meta.h"
#include "../util/meta_utils.h"
#include "../coding/coding.h"


/* .XWB - from Otomedius (AC) */
VGMSTREAM* init_vgmstream_xwb_konami(STREAMFILE* sf) {

    /* checks */
    int file_id = read_u16le(0x00, sf);
    if (file_id < 0x01 || file_id > 0x40) //observed max is 0x3d
        return NULL;
    int entries = read_u16le(0x02, sf);
    if (entries < 1 || entries > 2)
        return NULL;
    if (!check_extensions(sf, "xwb"))
        return NULL;

    // format doesn't look much like actual XACT .xwb and it was made after many Konami Xbox games, but comes with a fake-ish .xgs too
    meta_header_t h = {
        .meta = meta_XWB_KONAMI
    };

    h.target_subsong = sf->stream_index;
    if (h.target_subsong == 0)
        h.target_subsong = 1;
    h.total_subsongs = entries;

    uint32_t offset = 0x04;
    uint32_t stream_offset = 0x04 + 0x14 * entries;
    for (int i = 0; i < entries; i++) {
        uint32_t offset = 0x04 + 0x14 * i;
        uint32_t chunk_size = read_u32le(offset + 0x00, sf);
        // 04: always 0
        h.stream_size   = read_u32le(offset + 0x08, sf);
        h.loop_start    = read_u32le(offset + 0x0c, sf);
        h.loop_end      = read_u32le(offset + 0x10, sf) + h.loop_start;

        h.stream_offset = stream_offset;
        stream_offset += chunk_size;
        offset += 0x14;

        if (i + 1 == h.target_subsong)
            break;
    }

    if (h.stream_size == 0)
        return NULL;

    // fmt header (size 0x12) before data
    offset = h.stream_offset;
    h.stream_offset += 0x12;

    if (read_u16le(offset + 0x00, sf) != 0x0001)
        return NULL;
    h.channels      = read_u16le(offset + 0x02, sf);
    h.sample_rate   = read_u16le(offset + 0x04, sf);
    // 08: avg bitrate
    // 0c: block size
    // 0e: bps
    if (read_u16le(offset + 0x0e, sf) != 16) //bps
        return NULL;
    // 10: usually 0x5F18

    h.loop_flag     = h.loop_end > 0;
    h.num_samples   = pcm16_bytes_to_samples(h.stream_size, h.channels);
    h.loop_start    = pcm16_bytes_to_samples(h.loop_start, h.channels);
    h.loop_end      = pcm16_bytes_to_samples(h.loop_end, h.channels);

    h.coding = coding_PCM16LE;
    h.layout = layout_interleave;
    h.interleave = 0x02;
    h.has_subsongs = true;

    h.open_stream = true;
    h.sf = sf;

    return alloc_metastream(&h);
}
