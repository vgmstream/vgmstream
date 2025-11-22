#include "meta.h"
#include "../coding/coding.h"
#include "../util/meta_utils.h"
#include "../util/spu_utils.h"


/* SVS - SeqVagStream from Square games [Unlimited Saga (PS2) music] */
VGMSTREAM* init_vgmstream_svs(STREAMFILE* sf) {

    /* checks */
    if (!is_id32be(0x00,sf, "SVS\0"))
        return NULL;
    /* .bgm: from debug strings (music%3.3u.bgm)
     * .svs: header id (probably ok like The Bouncer's .vs, there are also refs to "vas") */
    if (!check_extensions(sf, "bgm,svs"))
        return NULL;

    meta_header_t h = {
        .meta = meta_SVS,
    };
    /* 0x04: flags (1=stereo?, 2=loop) */
    //h.loop_start    = read_s32le(0x08,sf) * 28; /* frame count (0x10 * ch) */
    h.loop_end      = read_s32le(0x0c,sf) * 28; /* frame count (not exact num_samples when no loop) */
    int pitch       = read_s32le(0x10,sf); /* usually 0x1000 = 48000 */
    /* 0x14: volume? */
    /* 0x18: file id (may be null) */
    /* 0x1c: null */
    h.stream_offset = 0x20;
    h.stream_size = get_streamfile_size(sf) - h.stream_offset;

    h.channels = 2;
    h.sample_rate = spu2_pitch_to_sample_rate_rounded(pitch); // music = ~44100, ambience = 48000 (rounding makes more sense but not sure)
    h.num_samples = ps_bytes_to_samples(h.stream_size, h.channels);
    /* loop start/end on the same frame rarely happens too (ex. file_id 63 SVS), perhaps loop should be +1 */
    h.loop_flag = (h.loop_start  > 0); /* min is 1 */

    h.coding = coding_PSX;
    h.layout = layout_interleave;
    h.interleave = 0x10;

    h.sf = sf;
    h.open_stream = true;
    return alloc_metastream(&h);
}
