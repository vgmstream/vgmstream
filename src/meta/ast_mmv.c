#include "meta.h"
#include "../coding/coding.h"

/* AST - from Marvelous(?) games [Katekyou Hitman Reborn! Dream Hyper Battle! (PS2), Binchou-tan: Shiawasegoyomi (PS2)] */
VGMSTREAM* init_vgmstream_ast_mmv(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset, data_size;
    int loop_flag, channels, sample_rate, interleave;

    /* checks */
    if (!is_id32be(0x00,sf, "AST\0"))
        goto fail;

    /* .ast: from executables (usually found in bigfiles) */
    if (!check_extensions(sf,"ast"))
        goto fail;

    data_size = read_u32le(0x04, sf);
    if (data_size != get_streamfile_size(sf))
        goto fail;

    sample_rate = read_s32le(0x08,sf);
    channels = read_32bitLE(0x0C,sf);
    interleave = read_u32le(0x10,sf);
    /* 0x14: number of blocks */
    /* 0x18: ? (not fully related to size/time) */
    /* 0x1c: f32 time */
    loop_flag = 0;    
    start_offset = 0x100;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_AST_MMV;

    vgmstream->num_samples = ps_bytes_to_samples(data_size - start_offset, channels);
    vgmstream->sample_rate = sample_rate;
    vgmstream->interleave_block_size = interleave;

    vgmstream->layout_type = layout_interleave;    
    vgmstream->coding_type = coding_PSX;

    read_string(vgmstream->stream_name,0x20, 0x20,sf);

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
