#include "meta.h"
#include "../coding/coding.h"

/* ASTB - found in Dead Rising (X360) */
VGMSTREAM* init_vgmstream_astb(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, data_size;
    int loop_flag, channels;
    int i, xma_streams;

    /* check */
    if (!is_id32be(0x00,sf, "ASTB"))
        goto fail;
    if (!check_extensions(sf,"ast"))
        goto fail;

    if (read_u32be(0x04,sf) != get_streamfile_size(sf))
        goto fail;
    if (read_u16be(0x30,sf) != 0x165) /* only seen XMA1 */
        goto fail;

    start_offset = read_u32be(0x10,sf);
    data_size = read_u32be(0x20,sf);
    xma_streams = read_u16be(0x38,sf);

    loop_flag = read_u8(0x3a,sf);
    channels = 0; /* sum of all stream channels (though only 1/2ch ever seen) */
    for (i = 0; i < xma_streams; i++) {
        channels += read_u8(0x3c + 0x14 * i + 0x11,sf);
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_s32be(0x40,sf);
    vgmstream->meta_type = meta_ASTB;

    {
        /* manually find sample offsets (XMA1 nonsense again) */
        ms_sample_data msd = {0};

        msd.xma_version = 1;
        msd.channels = channels;
        msd.data_offset = start_offset;
        msd.data_size = data_size;
        msd.loop_flag = loop_flag;
        msd.loop_start_b = read_u32be(0x44,sf);
        msd.loop_end_b   = read_u32be(0x48,sf);
        msd.loop_start_subframe = read_u8(0x4c,sf) & 0xF; /* lower 4b: subframe where the loop starts, 0..4 */
        msd.loop_end_subframe   = read_u8(0x4c,sf) >> 4;  /* upper 4b: subframe where the loop ends, 0..3 */

        xma_get_samples(&msd, sf);
        vgmstream->num_samples = msd.num_samples;
        vgmstream->loop_start_sample = msd.loop_start_sample;
        vgmstream->loop_end_sample = msd.loop_end_sample;
    }

#ifdef VGM_USE_FFMPEG
    {
        off_t fmt_offset = 0x30;
        size_t fmt_size = 0x0c + xma_streams * 0x14;

        /* XMA1 "fmt" chunk @ 0x20 (BE, unlike the usual LE) */
        vgmstream->codec_data = init_ffmpeg_xma_chunk(sf, start_offset, data_size, fmt_offset, fmt_size);
        if (!vgmstream->codec_data) goto fail;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;

        xma_fix_raw_samples(vgmstream, sf, start_offset, data_size, fmt_offset, 1,1);
    }
#else
    goto fail;
#endif

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
