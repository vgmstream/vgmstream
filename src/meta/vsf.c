#include "meta.h"
#include "../coding/coding.h"


/* VSF - from Square Enix PS2 games between 2004-2006 [Musashi: Samurai Legend (PS2), Front Mission 5 (PS2)] */
VGMSTREAM * init_vgmstream_vsf(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count, flags, pitch;
    size_t channel_size, loop_start;

    /* checks */
    /* .vsf: header id and actual extension [Code Age Commanders (PS2)] */
    if (!check_extensions(streamFile, "vsf"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x56534600) /* "VSF\0" */
        goto fail;

    /* 0x04: data size */
    /* 0x08: file number? */
    /* 0x0c: version? (always 0x00010000) */
    channel_size = read_32bitLE(0x10,streamFile) * 0x10;
    /* 0x14: frame size */
    loop_start = read_32bitLE(0x18,streamFile) * 0x10; /* also in channel size */
    flags = read_32bitLE(0x1c,streamFile);
    pitch = read_32bitLE(0x20,streamFile);
    /* 0x24: volume? */
    /* 0x28: ? (may be 0) */
    /* rest is 0xFF */

    channel_count = (flags & (1<<0)) ? 2 : 1;
    loop_flag = (flags & (1<<1));
    start_offset = (flags & (1<<8)) ? 0x80 : 0x800;
    /* flag (1<<4) is common but no apparent differences, no other flags known */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = round10((48000 * pitch) / 4096);
    vgmstream->num_samples = ps_bytes_to_samples(channel_size, 1);
    vgmstream->loop_start_sample = ps_bytes_to_samples(loop_start, 1);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x400;
    vgmstream->meta_type = meta_VSF;

    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
