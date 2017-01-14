#include "meta.h"

/* VDS/VDM - from Grafitti Kingdom / Rakugaki Oukoku 2 */
VGMSTREAM * init_vgmstream_ps2_vds_vdm(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;

    /* check extension, case insensitive */
    if ( !check_extensions(streamFile,"vds,vdm"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x56445320 && /* "VDS " (music)*/
        read_32bitBE(0x00,streamFile) != 0x56444D20)   /* "VDM " (voices) */
        goto fail;

    loop_flag = read_8bit(0x20,streamFile);
    channel_count = read_32bitLE(0x10,streamFile);

   /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = channel_count > 1 ? layout_interleave : layout_none;
    vgmstream->meta_type = meta_PS2_VDS_VDM;

    start_offset = 0x800;
    vgmstream->num_samples = read_32bitLE(0x04,streamFile) * 28 / 16 / channel_count;
    /* 0x08: unknown, always 10 */
    vgmstream->sample_rate = read_32bitLE(0x0c,streamFile);
    vgmstream->channels = channel_count; /*0x10*/
    vgmstream->interleave_block_size = read_32bitLE(0x14,streamFile);
    vgmstream->loop_start_sample = (read_32bitLE(0x18,streamFile) - start_offset) * 28 / 16 / channel_count;
    vgmstream->loop_end_sample = (read_32bitLE(0x1c,streamFile) - start_offset) * 28 / 16 / channel_count;
    vgmstream->loop_flag = loop_flag; /*0x20*/
    /*0x21: volume? */
    /*0x22: pan? */
    /*0x23: 02=VDS 04=VDM? */

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
