#include "meta.h"
#include "../coding/coding.h"

/* WAV2 - from Infrogrames North America games [Slave Zero (PC) (PS2)] */
VGMSTREAM * init_vgmstream_wv2(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    size_t data_size;
    int loop_flag, channel_count;


    /* checks */
    if ( !check_extensions(streamFile,"wv2") )
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x57415632) /* "WAV2" */
        goto fail;

    start_offset = 0x1c;
    data_size = get_streamfile_size(streamFile) - start_offset;
    channel_count = read_8bit(0x0c,streamFile);
    loop_flag = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_WV2;
    vgmstream->sample_rate = read_32bitLE(0x10,streamFile);
    vgmstream->num_samples = ima_bytes_to_samples(data_size,channel_count); /* also 0x18 */

    vgmstream->coding_type = coding_DVI_IMA_int;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0xFA;

    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
