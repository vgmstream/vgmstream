#include "meta.h"
#include "../coding/coding.h"


/* .MYSPD - from U-Sing (Wii) */
VGMSTREAM* init_vgmstream_myspd(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    int loop_flag = 0, channels;
    uint32_t start_offset, channel_size;

    /* checks */
    /* .myspd: actual extension */
    if (!check_extensions(sf,"myspd"))
        return NULL;

    channels = 2;
    start_offset = 0x20;
    channel_size = read_s32be(0x00,sf);

    /* check size */
    if (channel_size * channels + start_offset != get_streamfile_size(sf))
        goto fail;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->num_samples = ima_bytes_to_samples(channel_size*channels, channels);
    vgmstream->sample_rate = read_s32be(0x04,sf);

    vgmstream->meta_type = meta_MYSPD;
    vgmstream->coding_type = coding_IMA_mono;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = channel_size;

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;

    return vgmstream;

fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
