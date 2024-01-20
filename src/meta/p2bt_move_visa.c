#include "meta.h"
#include "../coding/coding.h"


/* P2BT/MOVE/VISA - from Konami/KCE Studio games [Pop'n Music 7/8/Best (PS2), AirForce Delta Strike (PS2)] */
VGMSTREAM* init_vgmstream_p2bt_move_visa(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t data_offset;
    int loop_flag, channels, sample_rate, interleave;
    uint32_t loop_start, data_size;


    /* checks */
    if (!is_id32be(0x00,sf, "P2BT") && 
        !is_id32be(0x00,sf, "MOVE") && 
        !is_id32be(0x00,sf, "VISA"))
        return NULL;
    /* .psbt/move: header id (no apparent exts)
     * .vis: actual extension found in AFDS and other KCES games */
    if (!check_extensions(sf, "p2bt,move,vis"))
        return NULL;

    /* (header is the same with different IDs, all may be used within a single same game) */
    /* 04: 07FC? */
    sample_rate = read_s32le(0x08,sf);
    loop_start = read_s32le(0x0c,sf);
    data_size = read_u32le(0x10,sf); /* without padding */
    interleave = read_u32le(0x14,sf); /* usually 0x10, sometimes 0x400 */
    /* 18: 1? */
    /* 1c: 0x10? */
    channels = read_s32le(0x20,sf);
    /* 24: 1? */
    /* 28: stream name (AFDS), same as basename + original ext */

    loop_flag = (loop_start != 0);
    data_offset = 0x800;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;

    vgmstream->num_samples = ps_bytes_to_samples(data_size, channels);
    vgmstream->loop_start_sample = ps_bytes_to_samples(loop_start, channels);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;

    if (vgmstream->interleave_block_size)
        vgmstream->interleave_last_block_size = (data_size % (vgmstream->interleave_block_size * channels)) / channels;
    read_string(vgmstream->stream_name,0x10+1, 0x28, sf);

    vgmstream->meta_type = meta_P2BT_MOVE_VISA;

    if (!vgmstream_open_stream(vgmstream, sf, data_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
