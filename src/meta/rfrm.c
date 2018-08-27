#include "meta.h"
#include "../coding/coding.h"


/* RFTM - Retro Studios format [Donkey Kong Country Tropical Freeze (WiiU)] */
VGMSTREAM * init_vgmstream_rfrm(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, header_offset;
    size_t interleave;
    int loop_flag, channel_count;


    /* checks */
    if (!check_extensions(streamFile, "csmp"))
        goto fail;

    /* format uses weird-sized chunks, quick hack until I get enough files */

    if (read_32bitBE(0x00, streamFile) != 0x5246524D) /* "RFRM" */
        goto fail;
    if (read_32bitBE(0x14, streamFile) != 0x43534D50) /* "CSMP" */
        goto fail;
    if (read_32bitBE(0x3d, streamFile) != 0x44415441) /* "DATA" */
        goto fail;

    channel_count = read_32bitBE(0x35, streamFile);

    header_offset = 0x58;
    loop_flag = read_16bitBE(header_offset+0x0c,streamFile);

    start_offset = header_offset + 0x60;
    interleave = (get_streamfile_size(streamFile) - header_offset) / channel_count;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_RFRM;
    vgmstream->sample_rate = read_32bitBE(header_offset+0x08,streamFile);
    vgmstream->num_samples = read_32bitBE(header_offset+0x00,streamFile);
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(read_32bitBE(header_offset+0x10,streamFile));
    vgmstream->loop_end_sample =  dsp_nibbles_to_samples(read_32bitBE(header_offset+0x14,streamFile))+1;
    if (vgmstream->loop_end_sample > vgmstream->num_samples) /* ? */
        vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;
    dsp_read_coefs_be(vgmstream, streamFile, header_offset+0x1c, interleave);
    dsp_read_hist_be(vgmstream, streamFile, header_offset+0x40, interleave);

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
