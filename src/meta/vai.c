#include "meta.h"
#include "../coding/coding.h"

/* .VAI - from Asobo Studio games [Ratatouille (GC)] */
VGMSTREAM* init_vgmstream_vai(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    size_t data_size;
    int loop_flag, channels;


    /* checks */
    int sample_rate = read_s32be(0x00,sf);
    if (sample_rate < 8000 || sample_rate > 48000) //arbitrary max
        return NULL;
    if (!check_extensions(sf,"vai"))
        return NULL;

    start_offset = 0x4060;
    data_size = read_s32be(0x04,sf);
    if (data_size != get_streamfile_size(sf) - start_offset)
        return NULL;

    channels = 2;
    loop_flag = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_VAI;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = dsp_bytes_to_samples(data_size,channels);

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x4000;
    dsp_read_coefs_be(vgmstream, sf, 0x0c, 0x20);

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
