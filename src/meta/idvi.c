#include "meta.h"
#include "../coding/coding.h"

/* IDVI - from Capcom's Eldorado Gate Volume 1-7 (DC) */
VGMSTREAM* init_vgmstream_idvi(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    size_t data_size;
    int loop_flag, channels;


    /* checks */
    if (!is_id32be(0x00,sf, "IDVI"))
        return NULL;
    if (!check_extensions(sf,"dvi"))
        return NULL;

    loop_flag = (read_s32le(0x0C,sf) != 0);
    channels = read_s32le(0x04,sf); /* always 2? */
    start_offset = 0x800;
    data_size = get_streamfile_size(sf) - start_offset;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->channels = channels;
    vgmstream->sample_rate = read_s32le(0x08,sf);
    vgmstream->num_samples = ima_bytes_to_samples(data_size, channels);
    vgmstream->loop_start_sample = read_s32le(0x0C,sf);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->meta_type = meta_IDVI;
    vgmstream->coding_type = coding_DVI_IMA_mono;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x400;
    if (vgmstream->interleave_block_size)
        vgmstream->interleave_last_block_size = (data_size % (vgmstream->interleave_block_size*vgmstream->channels)) / vgmstream->channels;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
