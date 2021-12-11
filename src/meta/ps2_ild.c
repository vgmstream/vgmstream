#include "meta.h"
#include "../coding/coding.h"

/* ILD - from Tose(?) games [Battle of Sunrise (PS2), Nightmare Before Christmas: Oogie's Revenge (PS2)] */
VGMSTREAM* init_vgmstream_ild(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    int channels, loop_flag;
    uint32_t data_size;
    off_t start_offset;

    /* check ILD Header */
    if (!is_id32be(0x00,sf, "ILD\0"))
        goto fail;

    if (!check_extensions(sf, "ild"))
        goto fail;

    channels = read_u32le(0x04,sf); /* tracks (seen 2 and 4) */
    start_offset = read_u32le(0x08,sf);
    data_size = read_u32le(0x0C,sf);
    /* 0x10: headers size / 2? */
    /* 0x14: header per channel */
    /* - 0x00: null */
    /* - 0x04: header size? (always 0x20) */
    /* - 0x08: size (may vary a bit between channel pairs) */
    /* - 0x0c: interleave? */
    /* - 0x10: channels per track (1) */
    /* - 0x14: sample rate */
    /* - 0x18: loop start */
    /* - 0x1c: loop end (also varies) */


    loop_flag = (read_s32le(0x2C,sf) > 0);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_ILD;

    vgmstream->num_samples = ps_bytes_to_samples(data_size, channels);
    vgmstream->interleave_block_size = read_u32le(0x14 + 0x0c,sf);
    vgmstream->sample_rate = read_u32le(0x14 + 0x14,sf);
    vgmstream->loop_start_sample = ps_bytes_to_samples(read_u32le(0x14 + 0x18,sf), 1);
    vgmstream->loop_end_sample = ps_bytes_to_samples(read_u32le(0x14 + 0x1c,sf), 1);

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
