#include "meta.h"
#include "../util.h"

/* AUS - Atomic Planet games [Jackie Chan Adventures (PS2), Mega Man Anniversary Collection (PS2/Xbox)] */
VGMSTREAM* init_vgmstream_aus(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels, codec;


    /* checks */
    if (!is_id32be(0x00, sf, "AUS "))
        goto fail;
    if (!check_extensions(sf, "aus"))
        goto fail;

    channels = read_u32le(0x0c,sf);
    start_offset = 0x800;
    codec = read_u16le(0x06,sf);
    loop_flag = (read_u32le(0x1c,sf) == 1); /* games seem to just do full loops, even when makes no sense (jingles/megaman stages) */

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_AUS;
    vgmstream->sample_rate = read_s32le(0x10,sf); /* uses pretty odd values */
    vgmstream->num_samples = read_s32le(0x08,sf);
    vgmstream->loop_start_sample = read_s32le(0x14,sf); /* always 0? */
    vgmstream->loop_end_sample = read_s32le(0x18,sf); /* always samples? */

    if (codec == 0x02) {
        vgmstream->coding_type = coding_XBOX_IMA;
        vgmstream->layout_type = layout_none;
    }
    else {
        vgmstream->coding_type = coding_PSX;
        vgmstream->layout_type = layout_interleave;
        vgmstream->interleave_block_size = 0x800;
    }

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
