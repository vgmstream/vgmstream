#include "meta.h"

/* ADP - from Omikron: The Nomad Soul (PC/DC) */
VGMSTREAM * init_vgmstream_pc_adp_otns(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, datasize;
    int loop_flag = 0, channel_count, stereo_flag;

    if (!check_extensions(streamFile,"adp")) goto fail;

    /* no ID, only a basic 0x10 header with filesize and nulls; do some extra checks */
    datasize = read_32bitLE(0x00,streamFile) & 0x00FFFFFF; /*24 bit*/
    if (datasize + 0x10 != streamFile->get_size(streamFile)
            || read_32bitLE(0x04,streamFile) != 0
            || read_32bitLE(0x08,streamFile) != 0
            || read_32bitLE(0x0c,streamFile) != 0)
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
