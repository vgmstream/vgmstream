#include "meta.h"
#include "../coding/coding.h"

/* .DSF - from Ocean game(s?) [Last Rites (PC)] */
VGMSTREAM* init_vgmstream_dsf(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels, sample_rate;
    size_t data_size;


    /* checks */
    if (!is_id32be(0x00,sf, "OCEA") || !is_id64be(0x04,sf, "N DSA\0\0\0"))
        goto fail;

    if (!check_extensions(sf, "dsf"))
        goto fail;

    /* 0x10(2): always 1? */
    /* 0x12(4): total nibbles / 0x10? */
    /* 0x16(4): always 0? */
    start_offset    = read_u32le(0x1a,sf);
    sample_rate     = read_s32le(0x1e,sf);
    channels        = read_s32le(0x22,sf) + 1;
    data_size       = get_streamfile_size(sf) - start_offset;
    loop_flag = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_DSF;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = ((data_size / 0x08 / channels) * 14); /* bytes-to-samples */
    vgmstream->coding_type = coding_DSA;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x08;

    read_string(vgmstream->stream_name,0x20+1, 0x26,sf);

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
