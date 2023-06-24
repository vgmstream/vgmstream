#include "meta.h"
#include "../coding/coding.h"

/* .SMV - from Cho Aniki Zero (PSP) */
VGMSTREAM* init_vgmstream_smv(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset, channel_size, loop_start;
    int loop_flag, channels;


    /* check extension */
    if (!check_extensions(sf, "smv"))
        goto fail;

    channel_size = read_u32le(0x00,sf);
    /* 0x08: number of full interleave blocks */
    channels = read_u16le(0x0a,sf);
    loop_start = read_u32le(0x18,sf);
    loop_flag = (loop_start != -1);
    start_offset = 0x800;

    if (channel_size * channels + start_offset != get_streamfile_size(sf))
        goto fail;

    channel_size -= 0x10; /* last value has SPU end frame without flag 0x7 as it should */

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_s32le(0x10, sf);
    vgmstream->num_samples = ps_bytes_to_samples(channel_size, 1);
    vgmstream->loop_start_sample = ps_bytes_to_samples(loop_start, 1);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->meta_type = meta_SMV;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_u32le(0x04, sf);
    vgmstream->interleave_last_block_size = read_u32le(0x0c, sf);

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
