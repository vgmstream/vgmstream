#include "meta.h"
#include "../coding/coding.h"


/* .wsi - blocked dsp [Alone in the Dark (Wii)] */
VGMSTREAM * init_vgmstream_wsi(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, header_offset;
    size_t header_spacing;
    int loop_flag, channel_count;


    /* checks */
    if (!check_extensions(streamFile, "wsi"))
        goto fail;

    channel_count = read_32bitBE(0x04,streamFile);
    if (channel_count != 2) goto fail; /* assumed */

    /* check for consistent block headers */
    {
        off_t block_offset;
        off_t block_size_has_been;
        int i;

        block_offset = read_32bitBE(0x00,streamFile);
        if (block_offset < 0x08) goto fail;

        block_size_has_been = block_offset;

        /* check 4 blocks, to get an idea */
        for (i = 0; i < 4*channel_count; i++) {
            off_t block_size = read_32bitBE(block_offset,streamFile);

            if (block_size < 0x10)
                goto fail; /* expect at least the block header */
            if (i%channel_count+1 != read_32bitBE(block_offset+0x08,streamFile))
                goto fail; /* expect the channel numbers to alternate */

            if (i%channel_count==0)
                block_size_has_been = block_size;
            else if (block_size != block_size_has_been)
                goto fail; /* expect every block in a set of channels to have the same size */

            block_offset += block_size;
        }
    }

    start_offset = read_32bitBE(0x00, streamFile);
    header_offset = start_offset + 0x10;
    header_spacing = read_32bitBE(start_offset,streamFile);

    /* contains standard DSP header, but since it's blocked validations (start/loop ps, etc)
     * will fail, so no point to handle as standard DSP */
    loop_flag = read_16bitBE(header_offset+0x0c,streamFile);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_DSP_WSI;
    vgmstream->sample_rate = read_32bitBE(header_offset+0x08,streamFile);

    vgmstream->num_samples = read_32bitBE(header_offset+0x00,streamFile) / 14 * 14; /* remove incomplete last frame */
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(read_32bitBE(header_offset+0x10,streamFile));
    vgmstream->loop_end_sample =  dsp_nibbles_to_samples(read_32bitBE(header_offset+0x14,streamFile))+1;
    if (vgmstream->loop_end_sample > vgmstream->num_samples) /* ? */
        vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_blocked_wsi;
    dsp_read_coefs_be(vgmstream, streamFile, header_offset+0x1c, header_spacing);
    dsp_read_hist_be(vgmstream, streamFile, header_offset+0x40, header_spacing);

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
