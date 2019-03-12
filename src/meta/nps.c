#include "meta.h"
#include "../coding/coding.h"

/* NPFS - found in Namco PS2/PSP games [Tekken 5 (PS2), Venus & Braves (PS2), Ridge Racer (PSP)] */
VGMSTREAM * init_vgmstream_nps(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;

    /* checks */
    /* .nps: referenced extension (ex. Venus & Braves data files)
     * .npsf: header id (Namco Production Sound File?) */
    if ( !check_extensions(streamFile,"nps,npsf"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x4E505346) /* "NPSF" */
        goto fail;

    loop_flag = (read_32bitLE(0x14,streamFile) != 0xFFFFFFFF);
    channel_count = read_32bitLE(0x0C,streamFile);
    start_offset = (off_t)read_32bitLE(0x10,streamFile);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->channels = read_32bitLE(0x0C,streamFile);
    vgmstream->sample_rate = read_32bitLE(0x18,streamFile);
    vgmstream->num_samples = ps_bytes_to_samples(read_32bitLE(0x08,streamFile), 1); /* single channel data */
    if(vgmstream->loop_flag) {
        vgmstream->loop_start_sample = read_32bitLE(0x14,streamFile);
        vgmstream->loop_end_sample = ps_bytes_to_samples(read_32bitLE(0x08,streamFile), 1);
    }

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitLE(0x04,streamFile) / 2;
    vgmstream->meta_type = meta_NPS;
    read_string(vgmstream->stream_name,STREAM_NAME_SIZE, 0x34,streamFile);

    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
