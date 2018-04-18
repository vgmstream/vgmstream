#include "meta.h"
#include "../coding/coding.h"

/* ISH+ISD - from various games [Chaos Field (GC), Pokemon XD - Gale of Darkness (GC)] */
VGMSTREAM * init_vgmstream_ish_isd(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamHeader = NULL;
    off_t start_offset;
    int channel_count, loop_flag;


    /* checks */
    if (!check_extensions(streamFile, "isd"))
        goto fail;

    streamHeader = open_streamfile_by_ext(streamFile,"ish");
    if (!streamHeader) goto fail;

    if (read_32bitBE(0x00,streamHeader) != 0x495F5346) /* "I_SF" */
        goto fail;

    channel_count = read_32bitBE(0x14,streamHeader);
    loop_flag = (read_32bitBE(0x1C,streamHeader) != 0);
    start_offset = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitBE(0x08,streamHeader);
    vgmstream->num_samples = read_32bitBE(0x0C,streamHeader);
    if (loop_flag) {
        vgmstream->loop_start_sample = read_32bitBE(0x20,streamHeader)*14 / 0x08 /channel_count;
        vgmstream->loop_end_sample = read_32bitBE(0x24,streamHeader)*14 / 0x08 / channel_count;
    }

    vgmstream->meta_type = meta_ISH_ISD;
    vgmstream->coding_type = coding_NGC_DSP;
    if (channel_count == 1) {
        vgmstream->layout_type = layout_none;
    } else {
        vgmstream->layout_type = layout_interleave;
        vgmstream->interleave_block_size = read_32bitBE(0x18,streamHeader);
    }

    dsp_read_coefs_be(vgmstream,streamHeader,0x40,0x40);


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    close_streamfile(streamHeader);
    return vgmstream;

fail:
    close_streamfile(streamHeader);
    close_vgmstream(vgmstream);
    return NULL;
}
