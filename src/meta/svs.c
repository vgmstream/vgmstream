#include "meta.h"
#include "../coding/coding.h"


/* SVS - SeqVagStream from Square games [Unlimited Saga (PS2) music] */
VGMSTREAM* init_vgmstream_svs(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int channels, loop_flag, pitch;


    /* checks */
    if (!is_id32be(0x00,sf, "SVS\0"))
        return NULL;
    /* .bgm: from debug strings (music%3.3u.bgm)
     * .svs: header id (probably ok like The Bouncer's .vs, there are also refs to "vas") */
    if (!check_extensions(sf, "bgm,svs"))
        return NULL;

    /* 0x04: flags (1=stereo?, 2=loop) */
    pitch = read_s32le(0x10,sf); /* usually 0x1000 = 48000 */
    /* 0x14: volume? */
    /* 0x18: file id (may be null) */
    /* 0x1c: null */

    loop_flag = (read_s32le(0x08,sf) > 0); /* loop start frame, min is 1 */
    channels = 2;
    start_offset = 0x20;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SVS;
    vgmstream->sample_rate = round10((48000 * pitch) / 4096); /* music = ~44100, ambience = 48000 (rounding makes more sense but not sure) */
    vgmstream->num_samples = ps_bytes_to_samples(get_streamfile_size(sf) - start_offset, channels);
    if (loop_flag) {
        vgmstream->loop_start_sample = read_s32le(0x08,sf) * 28; /* frame count (0x10*ch) */
        vgmstream->loop_end_sample = read_s32le(0x0c,sf) * 28; /* frame count, (not exact num_samples when no loop) */
        /* start/end on the same frame rarely happens too (ex. file_id 63 SVS), perhaps loop should be +1 */
    }

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x10;

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
