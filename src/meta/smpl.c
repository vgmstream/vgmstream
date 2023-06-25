#include "meta.h"
#include "../coding/coding.h"

/* SMPL - from Homura (PS2) */
VGMSTREAM* init_vgmstream_smpl(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset, channel_size;
    int loop_flag, channels;

    /* checks*/
    if (!is_id32be(0x00,sf, "SMPL"))
        return NULL;

    /* .v0: left channel, .v1: right channel
     * .smpl: header id */
    if (!check_extensions(sf,"v0,v1") )
       return NULL;

    /* 0x04: version (VAG-clone) */
    channels = 1;
    loop_flag = (read_s32le(0x30,sf) != 0); /* .v1 doesn't have loop points */
    start_offset = 0x40;
    channel_size = read_u32be(0x0c,sf) - 0x10;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_s32be(0x10,sf);
    vgmstream->num_samples = ps_bytes_to_samples(channel_size*channels, channels);
    vgmstream->loop_start_sample = read_s32le(0x30,sf);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->meta_type = meta_SMPL;
    vgmstream->allow_dual_stereo = 1;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_none;

    read_string(vgmstream->stream_name,0x10+1, 0x20,sf);

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
