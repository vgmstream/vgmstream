#include "meta.h"
#include "../coding/coding.h"

/* .MYSPF - from U-Sing (Wii) */
VGMSTREAM * init_vgmstream_myspd(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    int loop_flag = 0, channel_count;
    off_t start_offset;
    size_t channel_size;

    /* check extension, case insensitive */
    if (!check_extensions(streamFile,"myspd"))
        goto fail;

    channel_count = 2;
    start_offset = 0x20;
    channel_size = read_32bitBE(0x00,streamFile);

    /* check size */
	if ((channel_size * channel_count + start_offset) != get_streamfile_size(streamFile))
		goto fail;

	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	vgmstream->num_samples = ima_bytes_to_samples(channel_size*channel_count, channel_count);
    vgmstream->sample_rate = read_32bitBE(0x04,streamFile);

    vgmstream->meta_type = meta_MYSPD;
	vgmstream->coding_type = coding_IMA_int;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = channel_size;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    return vgmstream;

fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
