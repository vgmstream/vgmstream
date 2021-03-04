#include "meta.h"
#include "../coding/coding.h"


/* 2PFS - from Konami Games [Mahoromatic: Moetto - KiraKira Maid-San (PS2), GANTZ The Game (PS2)] */
VGMSTREAM* init_vgmstream_ps2_2pfs(STREAMFILE *sf) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels, version, interleave;
    int loop_start_block, loop_end_block; /* block number */
    int loop_start_adjust, loop_end_adjust; /* loops start/end a few samples into the start/end block */


    /* checks */
    /* .sap: standard
     * .2pfs: header id? (Mahoromatic) */
    if (!check_extensions(sf, "sap,2pfs"))
        goto fail;

    if (read_u32be(0x00,sf) != 0x32504653) /* "2PFS" */
        goto fail;

    version = read_u16le(0x04,sf);
    if (version != 0x01 && version != 0x02) /* v1: Mahoromatic, v2: Gantz */
        goto fail;


    channels = read_u8(0x40,sf);
    loop_flag = read_u8(0x41,sf);
    start_offset = 0x800;
    interleave = 0x1000;

    /* other header values
     *  0x06: unknown, v1=0x0004 v2=0x0001
     *  0x08: unique file id
     *  0x0c: base header size (v1=0x50, v2=0x60) + datasize (without the 0x800 full header size)
     *  0x10-0x30: unknown (v1 differs from v2)
     *  0x38-0x40: unknown (v1 same as v2)
     *  0x4c: unknown, some kind of total samples? (v2 only)
     */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_PS2_2PFS;
    vgmstream->num_samples = read_u32le(0x34,sf) * 28 / 16 / channels;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;

    if (version == 0x01) {
        vgmstream->sample_rate = read_u32le(0x44,sf);
        loop_start_adjust = read_u16le(0x42,sf);
        loop_start_block = read_u32le(0x48,sf);
        loop_end_block = read_u32le(0x4c,sf);
    }
    else {
        vgmstream->sample_rate = read_u32le(0x48,sf);
        loop_start_adjust = read_u32le(0x44,sf);
        loop_start_block = read_u32le(0x50,sf);
        loop_end_block = read_u32le(0x54,sf);
    }
    loop_end_adjust = interleave; /* loops end after all samples in the end_block AFAIK */

    if (loop_flag) {
        /* block to offset > offset to sample + adjust (number of frames into the block) */
        vgmstream->loop_start_sample =
                ps_bytes_to_samples(loop_start_block * channels * interleave, channels)
                + ps_bytes_to_samples(loop_start_adjust * channels, channels);
        vgmstream->loop_end_sample =
                ps_bytes_to_samples(loop_end_block * channels * interleave, channels)
                + ps_bytes_to_samples(loop_end_adjust * channels, channels);
    }

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
