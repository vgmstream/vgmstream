#include "meta.h"
#include "../coding/coding.h"

/* Nihon Falcom FDK NXOpus  [Ys X -NORDICS- (Switch)] */
VGMSTREAM* init_vgmstream_nxof(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels, sample_rate;
    size_t data_size, skip = 0;
    int32_t num_samples, loop_start, loop_end;

    /* checks */
    if (!is_id32le(0x00, sf, "nxof"))
        goto fail;
    if (!check_extensions(sf,"nxopus"))
        goto fail;

    channels        = read_u8(0x05, sf);
    sample_rate     = read_u32le(0x08, sf);
    start_offset    = read_u32le(0x18, sf);
    data_size       = read_u32le(0x1C, sf);
    num_samples     = read_u32le(0x20, sf);
    loop_start      = read_u32le(0x30, sf);
    loop_end        = read_u32le(0x34, sf);

    loop_flag = (loop_end > 0);

    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_NXOF;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

#ifdef VGM_USE_FFMPEG
    skip = switch_opus_get_encoder_delay(start_offset, sf);
    vgmstream->codec_data = init_ffmpeg_switch_opus(sf, start_offset, data_size, vgmstream->channels, skip, vgmstream->sample_rate);
    if (!vgmstream->codec_data) goto fail;
    vgmstream->coding_type = coding_FFmpeg;
    vgmstream->layout_type = layout_none;
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
