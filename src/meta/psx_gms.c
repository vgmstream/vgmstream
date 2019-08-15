#include "meta.h"


/* .seb - Game Arts games [Grandia (PS1), Grandia II/III/X (PS2)] */
VGMSTREAM * init_vgmstream_seb(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;


    /* checks */
    /* .seb: found in Grandia II (PS2) .idx */
    /* .gms: fake? (.stz+idx bigfile without names, except in Grandia II) */
    if (!check_extensions(streamFile, "seb,gms,"))
        goto fail;

    channel_count = read_32bitLE(0x00,streamFile);
    if (channel_count > 2) goto fail; /* mono or stereo */
    /* 0x08/0c: unknown count, possibly related to looping */

    start_offset = 0x800;

    if (read_32bitLE(0x10,streamFile) > get_streamfile_size(streamFile) ||  /* loop start offset */
        read_32bitLE(0x18,streamFile) > get_streamfile_size(streamFile))    /* loop end offset */
        goto fail;
    /* in Grandia III sometimes there is a value at 0x24/34 */

    loop_flag = (read_32bitLE(0x20,streamFile) == 0);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SEB;
    vgmstream->sample_rate = read_32bitLE(0x04,streamFile);

    vgmstream->num_samples = read_32bitLE(0x1c,streamFile);
    vgmstream->loop_start_sample = read_32bitLE(0x14,streamFile);
    vgmstream->loop_end_sample = read_32bitLE(0x1c,streamFile);

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x800;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
