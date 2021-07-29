#include "meta.h"
#include "../coding/coding.h"

/* .208 - from Ocean game(s?) [Last Rites (PC)] */
VGMSTREAM* init_vgmstream_208(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, data_size;
    int loop_flag, channels, sample_rate;


    /* checks */
    if (!check_extensions(sf, "208"))
        goto fail;
    /* possible validation: (0x04 == 0 and 0xcc == 0x1F7D984D) or 0x04 == 0xf0 and 0xcc == 0) */
    if (!((read_u32le(0x04,sf) == 0x00 && read_u32be(0xcc,sf) == 0x1F7D984D) ||
          (read_u32le(0x04,sf) == 0xF0 && read_u32be(0xcc,sf) == 0x00000000)))
        goto fail;

    start_offset    = read_s32le(0x00,sf);
    data_size       = read_s32le(0x0c,sf);
    sample_rate     = read_s32le(0x34,sf);
    channels        = read_s32le(0x3C,sf); /* assumed */
    loop_flag = 0;

    if (start_offset + data_size != get_streamfile_size(sf))
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_208;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = pcm8_bytes_to_samples(data_size, channels);
    vgmstream->coding_type = coding_PCM8_U;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x1;

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
