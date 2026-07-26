#include "meta.h"
#include "../coding/coding.h"

/* XMA from Unreal Engine 5 games [Fortnite (Xone)] */
VGMSTREAM* init_vgmstream_xma_ue5(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag;


    /* checks */
    int format = read_u16le(0x00,sf);
    int channels = read_u16le(0x02,sf);
    int sample_rate = read_s32le(0x04,sf);
    if (format != 0x166 || channels < 1 || channels > 8 || sample_rate < 8000 || sample_rate > 96000)
        return NULL;
    /* .xma: assumed */
    if (!check_extensions(sf, "xma"))
        return NULL;
    if (get_streamfile_size(sf) < 0x800)
        return NULL;

    uint32_t chunk_offset = 0x00;
    uint32_t chunk_size = 0x34;
    start_offset = chunk_size + 0x04;

    /* Apparently UE5 uses this simplified RIFF fmt XMA as payload, without seek table even in music.
     * Add some extra checks just in case (see ffmpeg_make_riff_xma2 and xma2_parse_fmt_chunk_extra) */
    int block_size =    read_u8(0x0c,sf);
    int extra_data  = read_u16le(0x10,sf);
    if (block_size / channels != 0x02 || extra_data != 0x22)
        return NULL;

    loop_flag = false; // doesn't seem to use loop points
    int32_t num_samples = 0;
    xma2_parse_fmt_chunk_extra(sf, chunk_offset, &loop_flag, &num_samples, NULL, NULL, false);

    uint32_t data_size = read_u32le(chunk_size + 0x00, sf);
    if (data_size < 0x800 || start_offset + data_size != get_streamfile_size(sf))
        return NULL;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_XMA_UE5;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;

#ifdef VGM_USE_FFMPEG
    {
        vgmstream->codec_data = init_ffmpeg_xma_chunk(sf, start_offset, data_size, chunk_offset, chunk_size);
        if (!vgmstream->codec_data) goto fail;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;

        xma_fix_raw_samples(vgmstream, sf, start_offset, data_size, chunk_offset, 1,1);
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
