#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "../util/meta_utils.h"

/* VGS - from Harmonix games [Guitar Hero II (PS2), Guitar Hero Encore: Rocks the 80s (PS2)] */
VGMSTREAM* init_vgmstream_vgs(STREAMFILE *sf) {

    /* checks */
    if (!is_id32be(0x00,sf, "VgS!"))
        return NULL;
    // 0x04: version?
    if (!check_extensions(sf,"vgs"))
        return NULL;

    meta_header_t h = {0};
    h.meta = meta_VGS;

    /* contains N streams, which can have one less frame, or half frame and sample rate */
    for (int i = 0; i < 8; i++) {
        int stream_sample_rate = read_s32le(0x08 + 0x08 * i + 0x00,sf);
        uint32_t stream_frame_count = read_u32le(0x08 + 0x08 * i + 0x04,sf);
        uint32_t stream_data_size = stream_frame_count * 0x10;

        if (stream_sample_rate == 0)
            break;

        if (!h.sample_rate || !h.chan_size) {
            h.sample_rate = stream_sample_rate;
            h.chan_size = stream_data_size;
        }

        /* some streams end 1 frame early */
        if (h.chan_size - 0x10 == stream_data_size) {
            h.chan_size -= 0x10;
        }

        /* Guitar Hero II sometimes uses half sample rate for last stream */
        if (h.sample_rate != stream_sample_rate) {
            VGM_LOG("VGS: ignoring stream %i\n", i);
            //total_streams++; // todo handle substreams
            break;
        }

        h.channels++;
    }

    h.stream_offset = 0x80;

    h.num_samples = ps_bytes_to_samples(h.chan_size, 1);

    h.coding = coding_PSX_badflags; // flag = stream/channel number
    h.layout = layout_blocked_vgs;
    h.open_stream = true;
    h.sf = sf;

    return alloc_metastream(&h);
}

/* .vgs - from Harmonix games [Karaoke Revolution (PS2), EyeToy: AntiGrav (PS2)] */
VGMSTREAM* init_vgmstream_vgs_old(STREAMFILE* sf) {

    /* checks */
    int channels = read_s32le(0x00,sf);
    if (channels < 1 || channels > 4)
        return NULL;

    // .vgs: actual extension in bigfiles
    if (!check_extensions(sf,"vgs"))
        return NULL;

    meta_header_t h = {0};
    h.meta = meta_VGS;

    h.channels      = read_s32le(0x00, sf);
    h.sample_rate   = read_s32le(0x04, sf);
    int frame_count = read_u32le(0x08, sf);
    // 0c: usually 0, sometimes garbage

    h.stream_offset = 0x10;
    h.stream_size   = get_streamfile_size(sf) - h.stream_offset;

    if (frame_count * h.channels * 0x10 > h.stream_size)
        return NULL;

    h.num_samples   = ps_bytes_to_samples(h.stream_size, channels);
    h.interleave    = 0x2000;

    h.coding = coding_PSX;
    h.layout = layout_interleave;
    h.open_stream = true;
    h.sf = sf;

    return alloc_metastream(&h);
}
