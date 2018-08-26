#include "meta.h"
#include "../coding/coding.h"

/* RS03 - from Metroid Prime 2 (GC) */
VGMSTREAM * init_vgmstream_rs03(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    size_t data_size;
    int channel_count, loop_flag;


    /* checks */
    if (!check_extensions(streamFile, "dsp"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x52530003) /* "RS03" */
        goto fail;

    channel_count = read_32bitBE(0x04,streamFile);
    if (channel_count != 1 && channel_count != 2) goto fail;
    loop_flag = read_16bitBE(0x14,streamFile);

    start_offset = 0x60;
    data_size = (get_streamfile_size(streamFile) - start_offset);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitBE(0x0c,streamFile);
    vgmstream->num_samples = read_32bitBE(0x08,streamFile);
    if (loop_flag) {
        vgmstream->loop_start_sample = dsp_bytes_to_samples(read_32bitBE(0x18,streamFile), 1);
        vgmstream->loop_end_sample = dsp_bytes_to_samples(read_32bitBE(0x1c,streamFile), 1);
    }

    vgmstream->meta_type = meta_DSP_RS03;
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x8f00;
    if (vgmstream->interleave_block_size)
        vgmstream->interleave_last_block_size = ((data_size % (vgmstream->interleave_block_size*vgmstream->channels))/2+7)/8*8;

    dsp_read_coefs_be(vgmstream,streamFile,0x20,0x20);


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
