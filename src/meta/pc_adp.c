#include "meta.h"

/* ADP - from Balls of Steel */
VGMSTREAM * init_vgmstream_pc_adp_bos(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag = 0;
	int channel_count;

    if (!check_extensions(streamFile,"adp")) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x41445021) /* "ADP!" */
        goto fail;

    loop_flag = (-1 != read_32bitLE(0x08,streamFile));
    channel_count = 1;
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
    start_offset = 0x18;
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x0C,streamFile);
    vgmstream->num_samples = read_32bitLE(0x04,streamFile);
    if (loop_flag) {
        vgmstream->loop_start_sample = read_32bitLE(0x08,streamFile);
        vgmstream->loop_end_sample = vgmstream->num_samples;
    }

    vgmstream->coding_type = coding_DVI_IMA_int;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_BOS_ADP;

    // 0x10, 0x12 - both initial history?
    //vgmstream->ch[0].adpcm_history1_32 = read_16bitLE(0x10,streamFile);
    // 0x14 - initial step index?
    //vgmstream->ch[0].adpcm_step_index = read_32bitLE(0x14,streamFile);

    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
