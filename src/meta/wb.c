#include "meta.h"
#include "../coding/coding.h"


/* .WB - from Shooting Love. ~TRIZEAL~ */
VGMSTREAM* init_vgmstream_wb(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset;
    int channels, loop_flag;

    /* check header */
    if (read_u32le(0x00,sf) != 0x00000000)
        return NULL;
    if (read_u32le(0x0c,sf) + 0x10 != get_streamfile_size(sf))
        return NULL;

    /* .wb: actual extension */
    if (!check_extensions(sf,"wb"))
        goto fail;

    channels = 2;
    start_offset = 0x10;
    loop_flag = read_32bitLE(0x04,sf) > 0; /* loop end may be defined */

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_WB;
    vgmstream->sample_rate = 48000;
    vgmstream->num_samples = pcm16_bytes_to_samples(read_u32le(0x0C,sf), channels);
    vgmstream->loop_start_sample = read_s32le(0x04,sf);
    vgmstream->loop_end_sample = read_s32le(0x08,sf);

    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 2;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
