#include "meta.h"
#include "../coding/coding.h"

/* RSTM - from Rockstar games [Midnight Club 3, Bully - Canis Canim Edit (PS2)] */
VGMSTREAM * init_vgmstream_ps2_rstm(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;

    /* check extension (.rsm: in filelist, .rstm: renamed to header id) */
    if ( !check_extensions(streamFile,"rsm,rstm") )
        goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x5253544D) /* "RSTM" */
        goto fail;

    loop_flag = (read_32bitLE(0x24,streamFile)!=0xFFFFFFFF);
    channel_count = read_32bitLE(0x0C,streamFile);
    start_offset = 0x800;
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(0x08,streamFile);
    vgmstream->num_samples = ps_bytes_to_samples(read_32bitLE(0x20,streamFile),channel_count);
    vgmstream->loop_start_sample = ps_bytes_to_samples(read_32bitLE(0x24,streamFile),channel_count);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x10;
    vgmstream->meta_type = meta_PS2_RSTM;

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
