#include "meta.h"
#include "../coding/coding.h"

/* .sdt - from High Voltage games? [Baldur's Gate - Dark Alliance (GC)] */
VGMSTREAM * init_vgmstream_sdt(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    size_t data_size;
    int loop_flag, channel_count;


    /* checks */
    if (!check_extensions(streamFile, "sdt"))
        goto fail;

    channel_count = read_32bitBE(0x00,streamFile); /* assumed */
    loop_flag = (read_32bitBE(0x04,streamFile) != 0);
    start_offset = 0xA0;
    data_size = get_streamfile_size(streamFile) - start_offset;

    if (channel_count != 2)
        goto fail; /* only seen this */
    if (read_32bitBE(0x08,streamFile) != read_32bitBE(0x08,streamFile))
        goto fail; /* sample rate agreement between channels */
    if (read_32bitBE(0x98,streamFile) != 0x8000)
        goto fail; /* expected interleave */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SDT;
    vgmstream->sample_rate = read_32bitBE(0x08,streamFile);
    vgmstream->num_samples = dsp_bytes_to_samples(data_size, channel_count); /* maybe at @0x14 - 2? */
    if (loop_flag) {
        vgmstream->loop_start_sample = 0; /* maybe @0x08? */
        vgmstream->loop_end_sample = vgmstream->num_samples; /* maybe @0x10? */
    }
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x8000;
    if (vgmstream->interleave_block_size)
        vgmstream->interleave_last_block_size =
                (data_size % (vgmstream->interleave_block_size*vgmstream->channels)) / vgmstream->channels;

    dsp_read_coefs_be(vgmstream,streamFile, 0x3c,0x2E);
    dsp_read_hist_be(vgmstream, streamFile, 0x60,0x2E);


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
