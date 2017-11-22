#include "meta.h"
#include "../coding/coding.h"

/* EZWAVE - EZ2DJ (Arcade) */
VGMSTREAM * init_vgmstream_ezw(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
	off_t start_offset, data_size;
    int loop_flag, channel_count;

    /* check extension, case insensitive */
    if ( !check_extensions(streamFile,"ezw"))
        goto fail;


    loop_flag = 0;
	channel_count = read_8bit(0x0, streamFile);
	data_size = read_32bitLE(0xE,streamFile);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;


	start_offset = 0x12;
    vgmstream->sample_rate = read_32bitLE(0x2,streamFile);
	vgmstream->coding_type = coding_PCM16LE;
    vgmstream->num_samples = data_size/(channel_count*2);
	vgmstream->layout_type = layout_interleave;
	vgmstream->interleave_block_size = 0x2;
    vgmstream->meta_type = meta_EZW;

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
