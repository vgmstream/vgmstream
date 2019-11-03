#include "meta.h"
#include "../coding/coding.h"


/* 2PFS - from Konami Games [Mahoromatic: Moetto - KiraKira Maid-San (PS2), GANTZ The Game (PS2)] */
VGMSTREAM * init_vgmstream_ps2_2pfs(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count, version, interleave;
    int loop_start_block, loop_end_block; /* block number */
    int loop_start_adjust, loop_end_adjust; /* loops start/end a few samples into the start/end block */


    /* checks */
    /* .sap: standard
     * .2psf: header id? (Mahoromatic) */
    if (!check_extensions(streamFile, "sap,2psf"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x32504653) /* "2PFS" */
        goto fail;

    version = read_16bitLE(0x04,streamFile);
    if (version != 0x01 && version != 0x02) /* v1: Mahoromatic, v2: Gantz */
        goto fail;


    channel_count = read_8bit(0x40,streamFile);
    loop_flag = read_8bit(0x41,streamFile);
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
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_PS2_2PFS;
    vgmstream->num_samples = read_32bitLE(0x34,streamFile) * 28 / 16 / channel_count;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;

    if (version == 0x01) {
        vgmstream->sample_rate = read_32bitLE(0x44,streamFile);
        loop_start_adjust = read_16bitLE(0x42,streamFile);
        loop_start_block = read_32bitLE(0x48,streamFile);
        loop_end_block = read_32bitLE(0x4c,streamFile);
    }
    else {
        vgmstream->sample_rate = read_32bitLE(0x48,streamFile);
        loop_start_adjust = read_32bitLE(0x44,streamFile);
        loop_start_block = read_32bitLE(0x50,streamFile);
        loop_end_block = read_32bitLE(0x54,streamFile);
    }
    loop_end_adjust = interleave; /* loops end after all samples in the end_block AFAIK */

    if (loop_flag) {
        /* block to offset > offset to sample + adjust (number of frames into the block) */
        vgmstream->loop_start_sample =
                ps_bytes_to_samples(loop_start_block * channel_count * interleave, channel_count)
                + ps_bytes_to_samples(loop_start_adjust * channel_count, channel_count);
        vgmstream->loop_end_sample =
                ps_bytes_to_samples(loop_end_block * channel_count * interleave, channel_count)
                + ps_bytes_to_samples(loop_end_adjust * channel_count, channel_count);
    }

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
