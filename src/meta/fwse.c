#include "meta.h"
#include "../coding/coding.h"

/* FWSE - Capcom's MT Framework V1.x sound file */
VGMSTREAM *init_vgmstream_fwse(STREAMFILE *streamFile) {
    VGMSTREAM *vgmstream = NULL;
    uint32_t version, file_size, buffer_offset, 
        channel_count, sample_count, sample_rate;

    if (!check_extensions(streamFile,"fwse"))
        goto fail;

    if ((read_32bitLE(0x00,streamFile)) != 0x45535746)
        goto fail;

    version = read_32bitLE(0x04,streamFile);

    if (version != 2)
        goto fail;

    file_size = read_32bitLE(0x08,streamFile);
    buffer_offset = read_32bitLE(0x0C,streamFile); 
    channel_count = read_32bitLE(0x10,streamFile);

    if (channel_count > 1)
        goto fail;

    sample_count = read_32bitLE(0x14,streamFile);
    sample_rate = read_32bitLE(0x18,streamFile);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, 0);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_FWSE;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = sample_count;
    vgmstream->coding_type = coding_MTF_IMA;
    vgmstream->layout_type = channel_count == 1 ? layout_none : layout_interleave;
    vgmstream->interleave_block_size = 1;


    if (!vgmstream_open_stream(vgmstream,streamFile,buffer_offset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
