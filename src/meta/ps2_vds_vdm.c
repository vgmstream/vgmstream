#include "meta.h"
#include "../coding/coding.h"

/* VDS/VDM - from Procyon Studio games [Grafitti Kingdom / Rakugaki Oukoku 2 (PS2), Tsukiyo ni Saraba (PS2)] */
VGMSTREAM * init_vgmstream_ps2_vds_vdm(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;


    /* checks */
    if ( !check_extensions(streamFile,"vds,vdm"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x56445320 && /* "VDS " (music)*/
        read_32bitBE(0x00,streamFile) != 0x56444D20)   /* "VDM " (voices) */
        goto fail;

    loop_flag = read_8bit(0x20,streamFile);
    channel_count = read_32bitLE(0x10,streamFile);
    start_offset = 0x800;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* 0x08: unknown, always 0x10 */
    vgmstream->sample_rate = read_32bitLE(0x0c,streamFile);

    /* when looping (or maybe when stereo) data_size at 0x04 is actually smaller than file_size,
     * sometimes cutting outros with loop disabled; doesn't affect looping though */
    if (!loop_flag)
        vgmstream->num_samples = ps_bytes_to_samples(read_32bitLE(0x04,streamFile), channel_count);
    else
        vgmstream->num_samples = ps_bytes_to_samples(get_streamfile_size(streamFile) - start_offset, channel_count);
    vgmstream->loop_start_sample = ps_bytes_to_samples(read_32bitLE(0x18,streamFile) - start_offset, channel_count);
    vgmstream->loop_end_sample = ps_bytes_to_samples(read_32bitLE(0x1c,streamFile) - start_offset, channel_count);
    /* 0x21: volume?, 0x22: pan?, 0x23: 02=VDS 04=VDM? 02/05=VDM in Tsukiyo ni Saraba? */

    vgmstream->meta_type = meta_PS2_VDS_VDM;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = (channel_count == 1) ? layout_none : layout_interleave;
    vgmstream->interleave_block_size = read_32bitLE(0x14,streamFile);

    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
