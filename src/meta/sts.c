#include "meta.h"
#include "../coding/coding.h"

/* .STS - from Alfa System games [Shikigami no Shiro 3 (Wii)] */
VGMSTREAM* init_vgmstream_sts(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset, data_size, channel_size;
    int loop_flag, channels, sample_rate;


    /* checks */
    if (!check_extensions(sf, "sts"))
        goto fail;

    data_size = read_u32be(0x00,sf);
    if (data_size + 0x04 != get_streamfile_size(sf))
        goto fail;

    channels = read_u8(0x08,sf) + 1;
	sample_rate = read_u16be(0x0c,sf);
	/* 0x10: dsp related? */
	/* 0x16: usable size */
	channel_size = read_u32be(0x1a,sf);

    loop_flag = 0; //(read_s32be(0x4C,sf) != -1); /* not seen */

    start_offset = (channels == 1) ? 0x70 : 0x50;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_STS;
    vgmstream->sample_rate = sample_rate;

	vgmstream->num_samples = dsp_bytes_to_samples(channel_size, 1);
    vgmstream->loop_start_sample = 0;
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
	vgmstream->interleave_block_size = channel_size + 0x2e;

	dsp_read_coefs_be(vgmstream, sf, 0x1e, start_offset - 0x1e + channel_size);
	dsp_read_hist_be(vgmstream, sf, 0x1e + 0x24, start_offset - 0x1e + channel_size);

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
