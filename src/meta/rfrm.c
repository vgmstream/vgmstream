#include "meta.h"
#include "../coding/coding.h"

/* RFTM - Retro Studios format [Donkey Kong Country Tropical Freeze (WiiU)] */
VGMSTREAM *init_vgmstream_rfrm(STREAMFILE *streamFile)
{
    VGMSTREAM *vgmstream = NULL;
    off_t fmta_offset = 0x20, header_offset, start_offset;
    size_t stream_size, interleave;
    int loop_flag, channel_count;

    /* checks */
    if (!check_extensions(streamFile, "csmp"))
        goto fail;

    /* format uses weird-sized chunks, quick hack until I get enough files */

    if (read_32bitBE(0x00, streamFile) != 0x5246524D) /* "RFRM" */
        goto fail;
    if (read_32bitBE(0x14, streamFile) != 0x43534D50) /* "CSMP" */
        goto fail;

    stream_size = read_32bitBE(0x08, streamFile) - 0x38; /* stream size is counted from FMTA or LABL, the fixed size FMTA part can already be removed */

    if (read_32bitBE(fmta_offset, streamFile) == 0x4C41424C) /* "LABL" */
    {
        size_t labl_size = 0x18 + read_32bitBE(0x28, streamFile); /* the size of the LABL part is variable */
        fmta_offset += labl_size; /* the FMTA part have been moved by the LABL part */
        stream_size -= labl_size; /* remove the LABL part */
    }

    header_offset = fmta_offset + 0x38; /* where the first DSP sub-file starts */
    start_offset = header_offset + 0x60; /* skip DSP header */

    channel_count = read_32bitBE(fmta_offset + 0x15, streamFile);
    loop_flag = read_16bitBE(header_offset + 0x0C, streamFile); /* read loop flag in the first DSP sub-file */

    interleave = stream_size / channel_count; /* each DSP sub-file is a channel */
    
    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream)
        goto fail;

    vgmstream->meta_type = meta_RFRM;

    /* read DSP header */
    vgmstream->sample_rate = read_32bitBE(header_offset + 0x08, streamFile);
    vgmstream->num_samples = read_32bitBE(header_offset + 0x00, streamFile);
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(read_32bitBE(header_offset + 0x10, streamFile));
    vgmstream->loop_end_sample = dsp_nibbles_to_samples(read_32bitBE(header_offset + 0x14, streamFile)) + 1;
    if (vgmstream->loop_end_sample > vgmstream->num_samples) /* ? */
        vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;
    dsp_read_coefs_be(vgmstream, streamFile, header_offset + 0x1C, interleave);
    dsp_read_hist_be(vgmstream, streamFile, header_offset + 0x40, interleave);

    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
