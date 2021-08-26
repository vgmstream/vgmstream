#include "meta.h"
#include "../coding/coding.h"

/* .btsnd - Wii U boot sound file for each game/app */
VGMSTREAM* init_vgmstream_btsnd(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    int channels, loop_flag;
    off_t start_offset, data_size;
    int32_t num_samples, loop_start;


    /* checks */
    if (!check_extensions(sf, "btsnd"))
        goto fail;

    if (read_u32be(0x00,sf) != 0x02)
        goto fail;

    loop_start = read_s32be(0x04, sf);
    start_offset = 0x08;

    channels = 2;
    loop_flag = loop_start > 0;

    /* extra check since format is so simple */
    data_size = get_streamfile_size(sf);
    num_samples = pcm16_bytes_to_samples(data_size - start_offset, channels);
    if (loop_start >= num_samples)
        goto fail;



    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_WIIU_BTSND;
    vgmstream->sample_rate = 48000;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_PCM16BE;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x02;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
