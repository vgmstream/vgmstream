#include "meta.h"
#include "../coding/coding.h"

/* 04SW - found in Driver: Parallel Lines (Wii) */
VGMSTREAM * init_vgmstream_xa_04sw(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;
    size_t file_size, data_size;


    /* checks */
    /* ".04sw" is just the ID, the real filename inside the file uses .XA */
    if (!check_extensions(streamFile,"xa,04sw"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x30345357) /* "04SW" */
        goto fail;

    /* after the ID goes a semi-standard DSP header */
    if (read_32bitBE(0x10,streamFile) != 0) goto fail; /* should be non looping */
    loop_flag = 0;
    /* not in header it seems so just dual header check */
    channel_count = (read_32bitBE(0x04,streamFile) == read_32bitBE(0x64,streamFile)) ? 2 : 1;

    start_offset = read_32bitBE(0x04 + 0x60*channel_count,streamFile);

    file_size = get_streamfile_size(streamFile);
    data_size = read_32bitBE(0x04 + 0x60*channel_count + 0x04,streamFile);
    if (data_size+start_offset != file_size) goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitBE(0x0c,streamFile);
    vgmstream->num_samples = read_32bitBE(0x04,streamFile);

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = channel_count == 1 ? layout_none : layout_interleave;
    vgmstream->interleave_block_size = 0x8000;
    vgmstream->interleave_last_block_size = (read_32bitBE(0x08,streamFile) / 2 % vgmstream->interleave_block_size + 7) / 8 * 8;

    dsp_read_coefs_be(vgmstream,streamFile,0x20, 0x60);
    /* the initial history offset seems different thatn standard DSP and possibly always zero */

    vgmstream->meta_type = meta_XA_04SW;
    /* the rest of the header has unknown values (several repeats) and the filename */


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
