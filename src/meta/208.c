#include "meta.h"
#include "../coding/coding.h"

/* .208 - from Ocean game(s?) [Last Rites (PC)] */
VGMSTREAM * init_vgmstream_208(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count, sample_rate;
    size_t data_size;


    /* checks */
    if (!check_extensions(streamFile, "208"))
        goto fail;
    /* possible validation: (0x04 == 0 and 0xcc == 0x1F7D984D) or 0x04 == 0xf0 and 0xcc == 0) */
    if (!((read_32bitLE(0x04,streamFile) == 0x00 && read_32bitBE(0xcc,streamFile) == 0x1F7D984D) ||
          (read_32bitLE(0x04,streamFile) == 0xF0 && read_32bitBE(0xcc,streamFile) == 0x00000000)))
        goto fail;

    start_offset    = read_32bitLE(0x00,streamFile);
    data_size       = read_32bitLE(0x0c,streamFile);
    sample_rate     = read_32bitLE(0x34,streamFile);
    channel_count   = read_32bitLE(0x3C,streamFile); /* assumed */
    loop_flag = 0;

    if (start_offset + data_size != get_streamfile_size(streamFile))
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_208;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = pcm_bytes_to_samples(data_size, channel_count, 8);
    vgmstream->coding_type = coding_PCM8_U;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x1;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
