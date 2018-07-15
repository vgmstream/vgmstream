#include "meta.h"
#include "../coding/coding.h"

/* RRDS - from (some) NST games [Ridge Racer (DS), Metroid Prime Hunters - First Hunt (DS)] */
VGMSTREAM * init_vgmstream_nds_rrds(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;

    /* checks */
    /* .rrds: made-up extension (files come from a bigfile and don't have filenames/extension) */
    if (!check_extensions(streamFile, ",rrds"))
        goto fail;

    if ((read_32bitLE(0x00,streamFile)+0x18) != get_streamfile_size(streamFile))
        goto fail;

    loop_flag = (read_32bitLE(0x14,streamFile) != 0); //todo not correct for MPH: First Hunt?
    channel_count = 1;
    start_offset = 0x1c;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;


    vgmstream->sample_rate = read_32bitLE(0x08,streamFile);
    vgmstream->num_samples = ima_bytes_to_samples(read_32bitLE(0x00,streamFile)-start_offset,channel_count);
    if (loop_flag) {
        vgmstream->loop_start_sample = ima_bytes_to_samples(read_32bitLE(0x14,streamFile)-start_offset,channel_count);
        vgmstream->loop_end_sample = vgmstream->num_samples;
    }

    vgmstream->meta_type = meta_NDS_RRDS;
    vgmstream->coding_type = coding_IMA_int;
    vgmstream->layout_type = layout_none;

    {
        vgmstream->ch[0].adpcm_history1_16 = read_16bitLE(0x18,streamFile);
        vgmstream->ch[0].adpcm_step_index = read_16bitLE(0x1a,streamFile);
        if (vgmstream->ch[0].adpcm_step_index < 0 || vgmstream->ch[0].adpcm_step_index > 88)
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
