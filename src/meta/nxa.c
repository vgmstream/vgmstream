#include "meta.h"
#include "../coding/coding.h"


/* Entergram NXA Opus [Higurashi no Naku Koro ni Hou (Switch), Gensou Rougoku no Kaleidoscope (Switch)] */
VGMSTREAM* init_vgmstream_opus_nxa(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels, type, sample_rate;
    int32_t num_samples, loop_start, loop_end, skip;
    size_t data_size, frame_size;

    /* checks */
    if (!check_extensions(sf, "nxa"))
        goto fail;
    if (!is_id32be(0x00, sf, "NXA1"))
        goto fail;

    start_offset = 0x30;
    type        = read_u32le(0x04, sf);
    data_size   = read_u32le(0x08, sf) - start_offset;
    sample_rate = read_u32le(0x0C, sf);
    channels    = read_s16le(0x10, sf);
    frame_size  = read_u16le(0x12, sf);
    /* 0x14: frame samples */
    skip        = read_s16le(0x16, sf);
    num_samples = read_s32le(0x18, sf);
    loop_start  = read_s32le(0x1c, sf);
    loop_end    = read_s32le(0x20, sf);
    /* rest: null */

    loop_flag = (loop_start > 0);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_NXA;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

    switch(type) {
#ifdef VGM_USE_FFMPEG
        case 1: { /* Higurashi no Naku Koro ni Hou (Switch) */
            vgmstream->codec_data = init_ffmpeg_switch_opus(sf, start_offset, data_size, vgmstream->channels, skip, vgmstream->sample_rate);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }

        case 2: { /* Gensou Rougoku no Kaleidoscope (Switch) */
            opus_config cfg = {0};

            cfg.channels = channels;
            cfg.skip = skip;
            cfg.frame_size = frame_size;

            vgmstream->codec_data = init_ffmpeg_fixed_opus(sf, start_offset, data_size, &cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif
        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
