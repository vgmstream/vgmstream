#include "meta.h"
#include "../coding/coding.h"

/* AST - from MicroVision lib games [P.T.O. IV (PS2), Naval Ops: Warship Gunner (PS2)] */
VGMSTREAM* init_vgmstream_ast_mv(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset, data_size, check;
    int loop_flag, channels, interleave, sample_rate;

    /* checks */
    if (!is_id32be(0x00,sf, "AST\0"))
        goto fail;

    if (!check_extensions(sf,"ast"))
        goto fail;

    channels = 2;
    sample_rate = read_s32le(0x04, sf);
    interleave = read_u32le(0x08,sf);
    data_size = read_u32le(0x0c,sf); /* may have garbage */
    check = read_u32be(0x10, sf);
    /* rest: null/garbage */

    loop_flag = 0;    
    start_offset = 0x800;

    /* there is a variation in .ikm (Zwei), with loops and different start offset */
    if (check != 0x20002000 &&  /* NO:WG (garbage up to start) */
        check != 0x00000000)    /* PTO4 */
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = ps_bytes_to_samples(data_size - start_offset, channels);
    vgmstream->interleave_block_size = interleave;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;    
    vgmstream->meta_type = meta_AST_MV;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
