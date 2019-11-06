#include "meta.h"
#include "../util.h"


/* .AFC - from Nintendo games [Super Mario Sunshine (GC), The Legend of Zelda: Wind Waker (GC)] */
VGMSTREAM * init_vgmstream_afc(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;


    /* checks */
    /* .afc: common
     * .stx: Pikmin (GC) */
    if (!check_extensions(streamFile, "afc,stx"))
        goto fail;

    if (read_u32be(0x00, streamFile) > get_streamfile_size(streamFile)) /* size without padding */
        goto fail;

    if (read_u16be(0x0a, streamFile) != 4) /* bps? */
        goto fail;
    if (read_u16be(0x0c, streamFile) != 16) /* samples per frame? */
        goto fail;
    /* 0x0e: always 0x1E? */

    channel_count = 2;
    loop_flag = read_s32be(0x10, streamFile);
    start_offset = 0x20;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_AFC;
    vgmstream->num_samples = read_s32be(0x04, streamFile);
    vgmstream->sample_rate = read_u16be(0x08, streamFile);
    vgmstream->loop_start_sample = read_s32be(0x14, streamFile);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_NGC_AFC;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x09;

    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
