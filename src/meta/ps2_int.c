#include "meta.h"
#include "../coding/coding.h"

/* raw PCM file assumed by extension [PaRappa The Rapper 2 (PS2)? , Amplitude (PS2)?] */
VGMSTREAM * init_vgmstream_ps2_int(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int channel_count;

    /* checks */
    if (!check_extensions(streamFile, "int,wp2"))
        goto fail;

    if (check_extensions(streamFile, "int"))
        channel_count = 2;
    else
        channel_count = 4;

    /* try to skip known .int (a horrible idea this parser exists) */
    {
        /* ignore A2M .int */
        if (read_32bitBE(0x00,streamFile) == 0x41324D00) /* "A2M\0" */
            goto fail;
        /* ignore EXST .int */
        if (read_32bitBE(0x10,streamFile) == 0x0C020000 &&
            read_32bitBE(0x20,streamFile) == 0x0C020000 &&
            read_32bitBE(0x30,streamFile) == 0x0C020000 &&
            read_32bitBE(0x40,streamFile) == 0x0C020000) /* check a few empty PS-ADPCM frames */
            goto fail;
    }

    start_offset = 0x00;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,0);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = 48000;
    vgmstream->meta_type = meta_PS2_RAW;
    vgmstream->num_samples = pcm_bytes_to_samples(get_streamfile_size(streamFile), vgmstream->channels, 16);
    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x200;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
