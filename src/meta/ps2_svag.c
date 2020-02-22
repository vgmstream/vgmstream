#include "meta.h"
#include "../coding/coding.h"

/* SVAG - from Konami Tokyo games [OZ (PS2), Neo Contra (PS2), Silent Hill 2 (PS2)] */
VGMSTREAM * init_vgmstream_ps2_svag(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    size_t data_size;
    int loop_flag, channel_count;


    /* checks */
    if (!check_extensions(streamFile, "svag"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x53766167) /* "Svag" */
        goto fail;

    channel_count = read_16bitLE(0x0C,streamFile); /* always 2? ("S"tereo vag?) */
    loop_flag = (read_32bitLE(0x14,streamFile)==1);

    /* header repeated at 0x400 presumably for stereo */
    if (channel_count > 1 && read_32bitBE(0x400,streamFile) != 0x53766167) /* "Svag" */
        goto fail;

    start_offset = 0x800;
    data_size = read_32bitLE(0x04,streamFile);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(0x08,streamFile);

    vgmstream->num_samples = ps_bytes_to_samples(read_32bitLE(0x04,streamFile), vgmstream->channels);
    if(vgmstream->loop_flag) {
        vgmstream->loop_start_sample = ps_bytes_to_samples(read_32bitLE(0x18,streamFile)*vgmstream->channels, vgmstream->channels);
        vgmstream->loop_end_sample = vgmstream->num_samples;
    }

    vgmstream->meta_type = meta_PS2_SVAG;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitLE(0x10,streamFile);
    if (vgmstream->interleave_block_size)
        vgmstream->interleave_last_block_size = (data_size % (vgmstream->interleave_block_size*vgmstream->channels)) / vgmstream->channels;


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
