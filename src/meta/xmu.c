#include "meta.h"
#include "../coding/coding.h"

/* XMU - found in Alter Echo (Xbox) */
VGMSTREAM* init_vgmstream_xmu(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    size_t start_offset;
    int loop_flag, channel_count;
    size_t data_size;

    /* checks */
    if (!is_id32be(0x00,sf, "XMU "))
        goto fail;

    if (!check_extensions(sf,"xmu"))
        goto fail;

    if (!is_id32be(0x08,sf, "FRMT"))
        goto fail;


    start_offset = 0x800;
    channel_count = read_u8(0x14,sf); /* always stereo files */
    loop_flag = read_u8(0x16,sf); /* no Loop found atm */
    data_size = read_u32le(0x7FC,sf); /* next to "DATA" */

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_XMU;
    vgmstream->sample_rate = read_s32le(0x10,sf);
    vgmstream->num_samples = xbox_ima_bytes_to_samples(data_size, vgmstream->channels);
    vgmstream->loop_start_sample = 0;
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_XBOX_IMA;
    vgmstream->layout_type = layout_none;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
