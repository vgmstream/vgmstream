#include "meta.h"
#include "../coding/coding.h"


/* "idsp" - from Namco's Wii NUB archives [Soul Calibur Legends (Wii), Sky Crawlers: Innocent Aces (Wii)] */
VGMSTREAM * init_vgmstream_nub_idsp(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;

    /* checks */
    if ( !check_extensions(streamFile,"idsp") )
        goto fail;

    /* actual header starts at "IDSP", while "idsp" is mostly nub bank stuff */
    if (read_32bitBE(0x00,streamFile) != 0x69647370)    /* "idsp" */
        goto fail;
    if (read_32bitBE(0xBC,streamFile) != 0x49445350)    /* "IDSP" */
        goto fail;

    loop_flag = read_32bitBE(0x20,streamFile);
    channel_count = read_32bitBE(0xC4,streamFile);
    if (channel_count > 8) goto fail;

    start_offset = 0x100 + (channel_count * 0x60);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_NUB_IDSP;
    vgmstream->sample_rate = read_32bitBE(0xC8,streamFile);
    vgmstream->num_samples = dsp_bytes_to_samples(read_32bitBE(0x14,streamFile),channel_count);
    if (loop_flag) {
        vgmstream->loop_start_sample = (read_32bitBE(0xD0,streamFile));
        vgmstream->loop_end_sample = (read_32bitBE(0xD4,streamFile));
    }

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitBE(0xD8,streamFile);
    if (vgmstream->interleave_block_size == 0)
        vgmstream->interleave_block_size = (get_streamfile_size(streamFile) - start_offset) / channel_count;

    dsp_read_coefs_be(vgmstream,streamFile,0x118,0x60);

    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
