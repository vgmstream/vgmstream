#include "meta.h"
#include "../coding/coding.h"

/* IDVI - from Capcom's Eldorado Gate Volume 1-7 (DC) */
VGMSTREAM * init_vgmstream_dc_idvi(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;

    /* check extension (.dvi: original, .idvi: renamed to header id) */
    if ( !check_extensions(streamFile,"dvi,idvi") )
        goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x49445649) /* "IDVI" */
        goto fail;

    loop_flag = (read_32bitLE(0x0C,streamFile) != 0);
    channel_count = read_32bitLE(0x04,streamFile); /* always 2? */
    start_offset = 0x800;
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x08,streamFile);
    vgmstream->num_samples = ima_bytes_to_samples(get_streamfile_size(streamFile) - start_offset, channel_count);
    vgmstream->loop_start_sample = read_32bitLE(0x0C,streamFile);
    vgmstream->loop_end_sample = ima_bytes_to_samples(get_streamfile_size(streamFile) - start_offset, channel_count);

    vgmstream->coding_type = coding_DVI_IMA_int;
    vgmstream->meta_type = meta_DC_IDVI;

	/* Calculating the short block... */
	if (channel_count > 1) {
		vgmstream->interleave_block_size = 0x400;
		vgmstream->interleave_smallblock_size = ((get_streamfile_size(streamFile)-start_offset)%(vgmstream->channels*vgmstream->interleave_block_size))/vgmstream->channels;
        vgmstream->layout_type = layout_interleave_shortblock;
    } else {
        vgmstream->layout_type = layout_none;
    }

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
