#include "meta.h"
#include "../coding/coding.h"

/* Unreal Engine 5 Opus */
VGMSTREAM* init_vgmstream_ueopus(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag = 0, channels, sample_rate;
    int32_t num_samples, skip_samples, skip_start, skip_size;

    if (!is_id64be(0x00, sf, "UEOPUS\0\0"))
        return NULL;

    if (!check_extensions(sf, "opus"))
        return NULL;

    channels = read_u8(0x09, sf);
    sample_rate = read_s32le(0x0A, sf);
    num_samples = read_s32le(0x12, sf);
    skip_start = read_s32le(0x1E, sf);
    skip_size = read_s32le(0x22, sf);

    skip_samples = skip_start + skip_size;

    start_offset = 0x2A;

    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) return NULL;

    vgmstream->meta_type = meta_UEOPUS;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;

#ifdef VGM_USE_FFMPEG
    vgmstream->codec_data = init_ffmpeg_ue_opus(sf, start_offset, get_streamfile_size(sf) - start_offset, channels, skip_samples, sample_rate);
    if (!vgmstream->codec_data) goto fail;

    vgmstream->coding_type = coding_FFmpeg;
    vgmstream->layout_type = layout_none;
    vgmstream->channel_layout = ffmpeg_get_channel_layout(vgmstream->codec_data);
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