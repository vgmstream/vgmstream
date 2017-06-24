#include "meta.h"
#include "../coding/coding.h"

/* XA30 - found in Driver: Parallel Lines (PS2) */
VGMSTREAM * init_vgmstream_ps2_xa30(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;
    size_t file_size, data_size;


    /* check extension, case insensitive */
    /* ".xa30" is just the ID, the real filename inside the file uses .XA */
    if (!check_extensions(streamFile,"xa,xa30"))
        goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x58413330) /* "XA30" */
        goto fail;
    if (read_32bitLE(0x04,streamFile) <= 2) goto fail; /* extra check to avoid PS2/PC XA30 mixup */

    loop_flag = 0;
    channel_count = 1 ; /* 0x08(2): interleave?  0x0a(2): channels? (always 1 in practice) */

    start_offset = read_32bitLE(0x0C,streamFile);

    file_size = get_streamfile_size(streamFile);
    data_size = read_32bitLE(0x14,streamFile); /* always off by 0x800 */
    if (data_size-0x0800 != file_size) goto fail;
    data_size = file_size - start_offset;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(0x04,streamFile);
    vgmstream->num_samples = ps_bytes_to_samples(data_size,channel_count); /* 0x10: some num_samples value (but smaller) */

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_PS2_XA30;
    /* the rest of the header has unknown values (and several repeats) and the filename */


    /* open the file for reading */
    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
