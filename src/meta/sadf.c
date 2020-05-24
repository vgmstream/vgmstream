#include "meta.h"

/* sadf - from Procyon Studio audio driver games [Xenoblade Chronicles 2 (Switch)] (sfx) */
VGMSTREAM* init_vgmstream_sadf(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    int channel_count, loop_flag;
    off_t start_offset;


    /* checks */
    if (!check_extensions(sf, "sad"))
        goto fail;

    if (read_32bitBE(0x00, sf) != 0x73616466) /* "sadf" */
        goto fail;

    channel_count = read_8bit(0x18, sf);
    loop_flag = read_8bit(0x19, sf);
    start_offset = read_32bitLE(0x1C, sf);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->num_samples = read_32bitLE(0x28, sf);
    vgmstream->sample_rate = read_32bitLE(0x24, sf);
    if (loop_flag) {
        vgmstream->loop_start_sample = read_32bitLE(0x2c, sf);
        vgmstream->loop_end_sample = read_32bitLE(0x30, sf);
     }
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = channel_count == 1 ? 0x8 : read_32bitLE(0x20, sf) / channel_count;
    vgmstream->meta_type = meta_SADF;

    dsp_read_coefs_le(vgmstream, sf, 0x80, 0x80);

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
