#include "meta.h"
#include "../coding/coding.h"

/* .VAI - from Asobo Studio games [Ratatouille (GC)] */
VGMSTREAM * init_vgmstream_vai(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    size_t data_size;
    int loop_flag, channel_count;


    /* checks */
    if ( !check_extensions(streamFile,"vai") )
        goto fail;

    start_offset = 0x4060;
    data_size = get_streamfile_size(streamFile) - start_offset;
    if (read_32bitBE(0x04,streamFile) != data_size)
        goto fail;

    channel_count = 2;
    loop_flag = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_VAI;
    vgmstream->sample_rate = read_32bitBE(0x00,streamFile);
    vgmstream->num_samples = dsp_bytes_to_samples(data_size,channel_count);

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x4000;
    dsp_read_coefs_be(vgmstream,streamFile,0x0c,0x20);

    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
