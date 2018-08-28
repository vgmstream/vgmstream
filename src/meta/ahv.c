#include "meta.h"
#include "../coding/coding.h"

/* AHV - from Amuze games [Headhunter (PS2)] */
VGMSTREAM * init_vgmstream_ahv(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    size_t data_size, channel_size, interleave;
    int loop_flag, channel_count;


    /* checks (.ahv: from names in bigfile) */
    if ( !check_extensions(streamFile,"ahv") )
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x41485600) /* "AHV\0" */
        goto fail;

    start_offset = 0x800;
    data_size = get_streamfile_size(streamFile) - start_offset;
    interleave = read_32bitLE(0x10,streamFile);
    channel_count = (interleave != 0) ? 2 : 1;
    channel_size = read_32bitLE(0x08,streamFile);
    loop_flag = 0;
    /* VAGp header after 0x14 */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_AHV;
    vgmstream->sample_rate = read_32bitLE(0x0c,streamFile);
    vgmstream->num_samples = ps_bytes_to_samples(channel_size,1);

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;
    if (interleave)
        vgmstream->interleave_last_block_size = (data_size % (interleave*channel_count)) / channel_count;

    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
