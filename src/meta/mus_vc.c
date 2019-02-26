#include "meta.h"
#include "../coding/coding.h"

/* .MUS - Vicious Cycle games [Dinotopia: The Sunstone Odyssey (GC/Xbox), Robotech: Battlecry (PS2/Xbox)] */
VGMSTREAM * init_vgmstream_mus_vc(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count, sample_rate;
    int big_endian, type;
    int32_t(*read_32bit)(off_t, STREAMFILE*) = NULL;


    /* checks */
    if (!check_extensions(streamFile, "mus"))
        goto fail;

    if (read_32bitBE(0x08,streamFile) != 0xBBBBBBBB &&
        read_32bitBE(0x14,streamFile) != 0xBBBBBBBB &&
        read_32bitBE(0x2c,streamFile) != 0xBEBEBEBE)
        goto fail;

    big_endian = (read_32bitBE(0x00,streamFile) == 0xFBBFFBBF);
    read_32bit = big_endian ? read_32bitBE : read_32bitLE;

    type = read_32bit(0x04, streamFile);
    /* 0x08: pseudo size? */
    /* other fields may be chunk sizes and lesser stuff */
    /* 0x88: codec header */

    channel_count = read_32bit(0x54,streamFile); /* assumed */
    if (channel_count != 1) goto fail;
    sample_rate = read_32bit(0x58,streamFile);
    loop_flag = 1; /* most files repeat except small jingles, but smaller ambient tracks also repeat */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_MUS_VC;
    vgmstream->sample_rate = sample_rate;

    switch(type) {
        case 0x01:
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_none;
            vgmstream->num_samples = dsp_bytes_to_samples(read_32bit(0xB0,streamFile), vgmstream->channels);
            vgmstream->loop_start_sample = 0;
            vgmstream->loop_end_sample = vgmstream->num_samples;

            start_offset = 0xB8;
            dsp_read_coefs_be(vgmstream,streamFile,0x88,0x00);
            dsp_read_hist_be (vgmstream,streamFile,0xac,0x00);
            break;

        case 0x02:
            vgmstream->coding_type = coding_XBOX_IMA;
            vgmstream->layout_type = layout_none;
            vgmstream->num_samples = xbox_ima_bytes_to_samples(read_32bit(0x9a,streamFile), vgmstream->channels);
            vgmstream->loop_start_sample = 0;
            vgmstream->loop_end_sample = vgmstream->num_samples;

            start_offset = 0x9e;
            break;

        default:
            goto fail;
    }

    read_string(vgmstream->stream_name,0x14, 0x34,streamFile); /* repeated at 0x64, size at 0x30/0x60 */

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
