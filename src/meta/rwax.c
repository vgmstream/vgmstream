#include "meta.h"
#include "../util.h"


/* RWAX - from AirForce Delta Storm (Xbox) */
VGMSTREAM* init_vgmstream_rwax(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset;
    int channels, loop_flag = 0;

    /* checks */
    if (!is_id32be(0x00,sf, "RAWX"))
        return NULL;
    if (!check_extensions(sf,"rwx"))
        return NULL;

    start_offset = read_u32le(0x04,sf);
    loop_flag = read_s32le(0x0C,sf);
    channels = 2;
    
    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_RWAX;
    vgmstream->sample_rate = read_s32le(0x08,sf);
    vgmstream->num_samples = read_s32le(0x10,sf);
    vgmstream->loop_start_sample = read_s32le(0x0C,sf);
    vgmstream->loop_end_sample = read_s32le(0x10,sf);

    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x2;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
