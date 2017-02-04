#include "meta.h"
#include "../util.h"

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

    vgmstream->coding_type = coding_DVI_IMA;
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

/* ADP - from Omikron: The Nomad Soul (PC/DC) */
VGMSTREAM * init_vgmstream_pc_adp_otns(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, datasize;
    int loop_flag = 0, channel_count, stereo_flag;

    if (!check_extensions(streamFile,"adp")) goto fail;

    /* no ID, only a basic 0x10 header with filesize and nulls; do some extra checks */
    datasize = read_32bitLE(0x00,streamFile) & 0x00FFFFFF; /*24 bit*/
    if (datasize + 0x10 != streamFile->get_size(streamFile)
            && read_32bitLE(0x04,streamFile) != 0
            && read_32bitLE(0x08,streamFile) != 0
            && read_32bitLE(0x10,streamFile) != 0)
        goto fail;

    stereo_flag = read_8bit(0x03, streamFile);
    if (stereo_flag > 1 || stereo_flag < 0) goto fail;
    channel_count = stereo_flag ? 2 : 1;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    start_offset = 0x10;
    vgmstream->channels = channel_count;
    vgmstream->sample_rate = 22050;
    vgmstream->num_samples = channel_count== 1 ? datasize*2 : datasize;

    vgmstream->coding_type = coding_OTNS_IMA;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_OTNS_ADP;

    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
