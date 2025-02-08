#include "meta.h"


/* MPC3 - from Paradigm games [Spy Hunter (PS2), MX Rider (PS2), Terminator 3 (PS2)] */
VGMSTREAM* init_vgmstream_mpc3(STREAMFILE* sf) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;

    /* checks */
    if (!is_id32be(0x00,sf, "MPC3"))
        return NULL;
    if (read_u32be(0x04,sf) != 0x00011400) /* version? */
        return NULL;
    if (!check_extensions(sf,"mc3"))
        return NULL;

    start_offset = 0x1c;
    loop_flag = 0;
    channels = read_u32le(0x08, sf);
    if (channels > 2) /* decoder max */
        return NULL;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_MPC3;
    vgmstream->coding_type = coding_MPC3;
    vgmstream->layout_type = layout_none;

    vgmstream->sample_rate = read_s32le(0x0c, sf);
    vgmstream->num_samples = read_s32le(0x10, sf) * 10; /* sizes in sub-blocks of 10 samples (without headers) */
    vgmstream->interleave_block_size = (read_u32le(0x14, sf) * 0x04 * channels) + 0x04;
    if (read_u32le(0x18, sf) + start_offset != get_streamfile_size(sf))
        goto fail;


    /* open the file for reading */
    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
