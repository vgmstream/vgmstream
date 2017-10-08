#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"
#include "../layout/layout.h"

/* MUSC - from Krome's PS2 games (The Legend of Spyro, Ty the Tasmanian Tiger) */
VGMSTREAM * init_vgmstream_musc(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    int loop_flag, channel_count;
    off_t start_offset;
    size_t data_size;

    /* .mus is the real extension, .musc is the header ID */
    if (!check_extensions(streamFile,"mus,musc"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x4D555343) /* "MUSC" */
        goto fail;

    start_offset = read_32bitLE(0x10,streamFile);
    data_size    = read_32bitLE(0x14,streamFile);
    if (start_offset + data_size != get_streamfile_size(streamFile))
        goto fail;
    /* always does full loops unless it ends in silence */
    loop_flag = read_32bitBE(get_streamfile_size(streamFile) - 0x10,streamFile) != 0x0C000000;
    channel_count = 2;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = (uint16_t)read_16bitLE(0x06,streamFile);
    vgmstream->num_samples = ps_bytes_to_samples(data_size, channel_count);
    vgmstream->loop_start_sample = 0;
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->meta_type = meta_MUSC;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitLE(0x18,streamFile) / 2;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
