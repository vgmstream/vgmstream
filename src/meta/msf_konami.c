#include "meta.h"
#include "../coding/coding.h"


/* MSFC - Konami (Armature?) variation [Metal Gear Solid 2 HD (PS3), Metal Gear Solid 3 HD (PS3)] */
VGMSTREAM* init_vgmstream_msf_konami(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    uint32_t codec;
    int loop_flag, channels, sample_rate;
    size_t data_size;


    /* checks */
    if (!check_extensions(sf,"msf"))
        goto fail;
    if (!is_id32be(0x00,sf,"MSFC"))
        goto fail;

    start_offset = 0x20;

    codec = read_u32be(0x04,sf);
    channels = read_s32be(0x08,sf);
    sample_rate = read_s32be(0x0c,sf);
    data_size = read_u32be(0x10,sf); /* without header */
    if (data_size + start_offset != get_streamfile_size(sf))
        goto fail;
    loop_flag = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_MSF_KONAMI;
    vgmstream->sample_rate = sample_rate;

    switch (codec) {
        case 0x01:
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x10;

            vgmstream->num_samples = ps_bytes_to_samples(data_size, channels);
            break;

        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
