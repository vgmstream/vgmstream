#include "meta.h"


/* .seb - Game Arts games [Grandia (PS1), Grandia II/III/X (PS2)] */
VGMSTREAM* init_vgmstream_seb(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;


    /* checks */
    channels = read_32bitLE(0x00,sf);
    if (channels > 2)
        return NULL;
    // 0x08/0c: unknown count, possibly related to looping

    /* .seb: found in Grandia II (PS2) .idx */
    /* .gms: fake? (.stz+idx bigfile without names, except in Grandia II) */
    if (!check_extensions(sf, "seb,gms,"))
        return NULL;

    start_offset = 0x800;

    if (read_u32le(0x10,sf) > get_streamfile_size(sf) ||  // loop start offset
        read_u32le(0x18,sf) > get_streamfile_size(sf))    // loop end offset
        return NULL;
    /* in Grandia III sometimes there is a value at 0x24/34 */

    loop_flag = (read_s32le(0x20,sf) == 0);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SEB;
    vgmstream->sample_rate = read_s32le(0x04,sf);

    vgmstream->num_samples = read_s32le(0x1c,sf);
    vgmstream->loop_start_sample = read_s32le(0x14,sf);
    vgmstream->loop_end_sample = read_s32le(0x1c,sf);

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x800;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
