#include "meta.h"
#include "../util.h"

/* VSF (from Musashi: Samurai Legend) */
VGMSTREAM * init_vgmstream_ps2_vsf(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;

    /* check extension, case insensitive */
    if (!check_extensions(streamFile, "vsf"))
        goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x56534600) /* "VSF" */
        goto fail;

    loop_flag = (read_32bitLE(0x1c,streamFile)==0x13);
    if(read_8bit(0x1C,streamFile)==0x0) 
        channel_count = 1;
    else
        channel_count = 2;

   /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

   /* fill in the vital statistics */
    start_offset = 0x800;
   vgmstream->channels = channel_count;
    vgmstream->sample_rate = 44100;
    vgmstream->coding_type = coding_PSX;
    vgmstream->num_samples = read_32bitLE(0x10,streamFile)*28;
    if (loop_flag) {
        vgmstream->loop_start_sample = read_32bitLE(0x18,streamFile)*28;
       vgmstream->loop_end_sample = vgmstream->num_samples;
    }

    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x400;
    vgmstream->meta_type = meta_PS2_VSF;

    /* open the file for reading */
    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
        goto fail;
    return vgmstream;

    /* clean up anything we may have opened */
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
