#include "meta.h"
#include "../coding/coding.h"

/* ALP - from High Voltage games [LEGO Racers (PC), NBA Inside Drive 2000 (PC)] */
VGMSTREAM* init_vgmstream_alp(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels, sample_rate;


    /* checks */
    if (!is_id32be(0x00,sf, "ALP "))
        goto fail;
    /* .tun: stereo 
     * .pcm: mono */
    if (!check_extensions(sf,"tun,pcm"))
        goto fail;

    start_offset = read_u32le(0x04, sf) + 0x08;
    if (!is_id32be(0x08,sf, "ADPC")) /* codec, probably */
        goto fail;
    /* 0x0c: "M\0\0" */
    channels = read_u8(0x0f, sf);

    /* NBA Inside Drive 2000 (PC) */
    if (start_offset >= 0x14)
        sample_rate = read_s32le(0x10, sf); /* still 22050 though */
    else
        sample_rate = 22050;

    loop_flag = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_ALP;
    vgmstream->channels = channels;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = ima_bytes_to_samples(get_streamfile_size(sf) - start_offset, channels);

    vgmstream->coding_type = coding_HV_IMA;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x01;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
