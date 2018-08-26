#include "meta.h"
#include "../coding/coding.h"


/* CSMP - Retro Studios sample [Metroid Prime 3 (Wii), Donkey Kong Country Returns (Wii)] */
VGMSTREAM * init_vgmstream_csmp(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, first_offset = 0x08, chunk_offset;
    int loop_flag, channel_count;


    /* checks */
    if (!check_extensions(streamFile, "csmp"))
        goto fail;
    if (read_32bitBE(0x00, streamFile) != 0x43534D50) /* "CSMP" */
        goto fail;
    if (read_32bitBE(0x04, streamFile) != 1)  /* version? */
        goto fail;

    if (!find_chunk(streamFile, 0x44415441,first_offset,0, &chunk_offset,NULL, 1, 0)) /*"DATA"*/
        goto fail;

    /* contains standard DSP header, but somehow some validations (start/loop ps)
     * don't seem to work, so no point to handle as standard DSP */

    channel_count = 1;
    loop_flag = read_16bitBE(chunk_offset+0x0c,streamFile);
    start_offset = chunk_offset + 0x60;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_CSMP;
    vgmstream->sample_rate = read_32bitBE(chunk_offset+0x08,streamFile);
    vgmstream->num_samples = read_32bitBE(chunk_offset+0x00,streamFile);
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(read_32bitBE(chunk_offset+0x10,streamFile));
    vgmstream->loop_end_sample =  dsp_nibbles_to_samples(read_32bitBE(chunk_offset+0x14,streamFile))+1;
    if (vgmstream->loop_end_sample > vgmstream->num_samples) /* ? */
        vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_none;
    dsp_read_coefs_be(vgmstream, streamFile, chunk_offset+0x1c, 0x00);
    dsp_read_hist_be(vgmstream, streamFile, chunk_offset+0x40, 0x00);

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
