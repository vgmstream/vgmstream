#include "meta.h"
#include "../coding/coding.h"

/* .DSF - from Ocean game(s?) [Last Rites (PC)] */
VGMSTREAM * init_vgmstream_dsf(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count, sample_rate;
    size_t data_size;


    /* checks */
    if (!check_extensions(streamFile, "dsf"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x4F434541 &&  /* "OCEA" */
        read_32bitBE(0x00,streamFile) != 0x4E204453 &&  /* "N DS" */
        read_32bitBE(0x00,streamFile) != 0x41000000)    /* "A\0\0\0" */
        goto fail;

    /* 0x10(2): always 1? */
    /* 0x12(4): total nibbles / 0x10? */
    /* 0x16(4): always 0? */
    start_offset    = read_32bitLE(0x1a,streamFile);
    sample_rate     = read_32bitLE(0x1e,streamFile);
    channel_count   = read_32bitLE(0x22,streamFile) + 1;
    data_size       = get_streamfile_size(streamFile) - start_offset;
    loop_flag = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_DSF;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = ((data_size / 0x08 / channel_count) * 14); /* bytes-to-samples */
    vgmstream->coding_type = coding_DSA;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x08;

    read_string(vgmstream->stream_name,0x20+1, 0x26,streamFile);

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
