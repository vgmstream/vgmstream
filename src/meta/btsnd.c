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
        return NULL;

    uint32_t type = read_u32be(0x00,sf);
    if (type == 0x00) {
        loop_flag = 0;
    }
    else if (type == 0x02) {
        loop_flag = 1;
    }
    else {
        return NULL;
    }

    loop_start = read_s32be(0x04, sf); /* non-looping: 0 or some number lower than samples */
    start_offset = 0x08;
    channels = 2;

    /* extra checks since format is so simple */
    data_size = get_streamfile_size(sf);
    num_samples = pcm16_bytes_to_samples(data_size - start_offset, channels);
    if (loop_start >= num_samples)
        return NULL;
    if (num_samples > 960000) /* known max reached by various games, encoder/Wii U limit? */
        return NULL;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_BTSND;
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
