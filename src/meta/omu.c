#include "meta.h"

/* IMU - found in Alter Echo (PS2) */
VGMSTREAM* init_vgmstream_ps2_omu(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;


    /* checks */
    if (!is_id32be(0x00,sf, "OMU "))
        goto fail;

    if (!check_extensions(sf,"omu"))
        goto fail;

    if (!is_id32be(0x08,sf, "FRMT"))
        goto fail;

    loop_flag = 1;
    channels = read_u8(0x14,sf);
    start_offset = 0x40;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_s32le(0x10,sf);
    vgmstream->num_samples = (read_u32le(0x3C,sf) / (vgmstream->channels*2));
    vgmstream->loop_start_sample = 0;
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x200;
    vgmstream->meta_type = meta_PS2_OMU;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
