#include "meta.h"
#include "../coding/coding.h"

/* VDS/VDM - from Procyon Studio games [Grafitti Kingdom / Rakugaki Oukoku 2 (PS2), Tsukiyo ni Saraba (PS2)] */
VGMSTREAM* init_vgmstream_vds_vdm(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;


    /* checks */
    if (!is_id32be(0x00,sf, "VDS ") && !is_id32be(0x00,sf, "VDM "))
        return NULL;
    if (!check_extensions(sf,"vds,vdm"))
        return NULL;

    channels = read_s32le(0x10,sf); // VDM = mono, VDS = stereo
    loop_flag = read_u8(0x20,sf);
    start_offset = 0x800;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    /* 0x08: unknown, always 0x10 */
    vgmstream->sample_rate = read_s32le(0x0c,sf);

    /* when looping (or maybe when stereo) data_size at 0x04 is actually smaller than file_size,
     * sometimes cutting outros with loop disabled; doesn't affect looping though */
    if (!loop_flag)
        vgmstream->num_samples = ps_bytes_to_samples(read_u32le(0x04,sf), channels);
    else
        vgmstream->num_samples = ps_bytes_to_samples(get_streamfile_size(sf) - start_offset, channels);
    vgmstream->loop_start_sample = ps_bytes_to_samples(read_u32le(0x18,sf) - start_offset, channels);
    vgmstream->loop_end_sample = ps_bytes_to_samples(read_u32le(0x1c,sf) - start_offset, channels);
    /* 0x21: volume?, 0x22: pan?, 0x23: 02=VDS 04=VDM? 02/05=VDM in Tsukiyo ni Saraba? */

    vgmstream->meta_type = meta_VDS_VDM;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = (channels == 1) ? layout_none : layout_interleave;
    vgmstream->interleave_block_size = read_u32le(0x14,sf);

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
