#include "meta.h"
#include "../coding/coding.h"


//todo this was extracted from a .pak bigfile. Inside are headers then data (no extensions),
// but headers are VAGp in the PS2 version, so it would make more sense to extract pasting
// header+data together, or support as-is (.SCK is a fake extension).

/* SCK+DSP - Scorpion King (GC) */
VGMSTREAM * init_vgmstream_ngc_sck_dsp(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamHeader = NULL;
    int channel_count, loop_flag;
    size_t data_size;

    /* check extension, case insensitive */
    if (!check_extensions(streamFile, "dsp"))
        goto fail;

    streamHeader = open_streamfile_by_ext(streamFile, "sck");
    if (!streamHeader) goto fail;

    if (read_32bitBE(0x5C,streamHeader) != 0x60A94000)
        goto fail;

    channel_count = 2;
    loop_flag = 0;
    data_size = read_32bitBE(0x14,streamHeader);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitBE(0x18,streamHeader);
    vgmstream->num_samples = dsp_bytes_to_samples(data_size, channel_count);
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitBE(0xC,streamHeader);
    if (vgmstream->interleave_block_size > 0)
        vgmstream->interleave_last_block_size = (data_size % (vgmstream->interleave_block_size * channel_count)) / channel_count;

    vgmstream->meta_type = meta_NGC_SCK_DSP;

    dsp_read_coefs_be(vgmstream,streamHeader, 0x2c, 0x00);

    if (!vgmstream_open_stream(vgmstream,streamFile,0x00))
        goto fail;
    close_streamfile(streamHeader);
    return vgmstream;

fail:
    close_streamfile(streamHeader);
    close_vgmstream(vgmstream);
    return NULL;
}
