#include "meta.h"
#include "../coding/coding.h"

/* RSTM - from Rockstar games [Midnight Club 3, Bully - Canis Canim Edit (PS2)] */
VGMSTREAM* init_vgmstream_rstm_rockstar(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset;
    int channels, loop_flag;


    /* checks */
    if (!is_id32be(0x00, sf, "RSTM"))
        return NULL;

    /* .rsm: in filelist
     * .rstm: header id */
    if (!check_extensions(sf,"rsm,rstm"))
        return NULL;

    loop_flag = (read_s32le(0x24,sf) > 0);
    channels = read_s32le(0x0C,sf);
    start_offset = 0x800;

	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_RSTM_ROCKSTAR;

    vgmstream->sample_rate = read_s32le(0x08,sf);
    vgmstream->num_samples = ps_bytes_to_samples(read_u32le(0x20,sf),channels);
    vgmstream->loop_start_sample = ps_bytes_to_samples(read_u32le(0x24,sf),channels);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x10;

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, sf, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
