#include "meta.h"
#include "../util.h"

/* SPSD - Naomi GD-ROM streams [Guilty Gear X (Naomi), Crazy Taxi (Naomi), Virtua Tennis 2 (Naomi)] */
VGMSTREAM * init_vgmstream_naomi_spsd(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    size_t data_size;
    int loop_flag, channel_count;


    /* checks */
    if (!check_extensions(streamFile, "spsd"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x53505344) /* "SPSD" */
        goto fail;

    loop_flag = 0;
    channel_count = 2;

    start_offset = 0x40;
    data_size = get_streamfile_size(streamFile) - start_offset;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = (uint16_t)read_16bitLE(0x2A,streamFile);
    vgmstream->num_samples = read_32bitLE(0x0C,streamFile);

    vgmstream->meta_type = meta_NAOMI_SPSD;
    switch (read_8bit(0x08,streamFile)) {
        case 0x01: /* [Virtua Tennis 2 (Naomi)] */
            vgmstream->coding_type = coding_PCM8;
            break;
        case 0x03:
            vgmstream->coding_type = coding_AICA;
            break;
        default:
            goto fail;
    }
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x2000;
    if (vgmstream->interleave_block_size)
        vgmstream->interleave_last_block_size = (data_size % (vgmstream->interleave_block_size*vgmstream->channels)) / vgmstream->channels;


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
