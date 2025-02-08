#include "meta.h"
#include "../coding/coding.h"


/* .IMA - Blitz Games early games [Lilo & Stitch: Trouble in Paradise (PC)] */
VGMSTREAM* init_vgmstream_ima(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels, num_samples, sample_rate;


    /* checks */
    if (read_u32be(0x00,sf) != 0x02000000) /* version? */
        return NULL;
    if (read_u32be(0x04,sf) != 0)
        return NULL;
    if (!check_extensions(sf, "ima"))
        return NULL;

    num_samples = read_s32le(0x08, sf);
    channels    = read_s32le(0x0c,sf);
    sample_rate = read_s32le(0x10, sf);

    loop_flag  = 0;
    start_offset = 0x14;

    if (channels > 1)  /* unknown interleave */
        return NULL;
    if (sample_rate < 11025 || sample_rate > 44100)  /* arbitrary values */
        return NULL;
    if (num_samples != ima_bytes_to_samples(get_streamfile_size(sf) - start_offset, channels))
        return NULL;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_IMA;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;

    vgmstream->coding_type = coding_BLITZ_IMA;
    vgmstream->layout_type = layout_none;

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
