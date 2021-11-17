#include "meta.h"
#include "../coding/coding.h"

/* .MUS - Vicious Cycle games [Dinotopia: The Sunstone Odyssey (GC/Xbox), Robotech: Battlecry (PS2/Xbox)] */
VGMSTREAM* init_vgmstream_mus_vc(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels, sample_rate;
    int big_endian, type;
    int32_t(*read_32bit)(off_t, STREAMFILE*) = NULL;


    /* checks */
    if (read_u32be(0x00,sf) != 0xFBBFFBBF &&    /* BE */
        read_u32le(0x00,sf) != 0xFBBFFBBF)      /* LE */
        goto fail;

    if (!check_extensions(sf, "mus"))
        goto fail;

    if (read_u32be(0x08,sf) != 0xBBBBBBBB ||
        read_u32be(0x14,sf) != 0xBBBBBBBB ||
        read_u32be(0x2c,sf) != 0xBEBEBEBE)
        goto fail;

    big_endian = (read_u32be(0x00,sf) == 0xFBBFFBBF);
    read_32bit = big_endian ? read_32bitBE : read_32bitLE;

    type = read_32bit(0x04, sf);
    /* 0x08: pseudo size? */
    /* other fields may be chunk sizes and lesser stuff */
    /* 0x88: codec header */

    channels = read_32bit(0x54,sf); /* assumed */
    if (channels != 1) goto fail;
    sample_rate = read_32bit(0x58,sf);
    loop_flag = 1; /* most files repeat except small jingles, but smaller ambient tracks also repeat */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_MUS_VC;
    vgmstream->sample_rate = sample_rate;

    switch(type) {
        case 0x01:
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_none;
            vgmstream->num_samples = dsp_bytes_to_samples(read_32bit(0xB0,sf), vgmstream->channels);
            vgmstream->loop_start_sample = 0;
            vgmstream->loop_end_sample = vgmstream->num_samples;

            start_offset = 0xB8;
            dsp_read_coefs_be(vgmstream,sf,0x88,0x00);
            dsp_read_hist_be (vgmstream,sf,0xac,0x00);
            break;

        case 0x02:
            vgmstream->coding_type = coding_XBOX_IMA;
            vgmstream->layout_type = layout_none;
            vgmstream->num_samples = xbox_ima_bytes_to_samples(read_32bit(0x9a,sf), vgmstream->channels);
            vgmstream->loop_start_sample = 0;
            vgmstream->loop_end_sample = vgmstream->num_samples;

            start_offset = 0x9e;
            break;

        default:
            goto fail;
    }

    read_string(vgmstream->stream_name,0x14, 0x34,sf); /* repeated at 0x64, size at 0x30/0x60 */

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
