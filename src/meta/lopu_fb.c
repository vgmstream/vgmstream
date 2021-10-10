#include "meta.h"
#include "../coding/coding.h"


/* LOPU - French-Bread's Opus [Melty Blood: Type Lumina (Switch)] */
VGMSTREAM* init_vgmstream_lopu_fb(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset, data_size;
    int loop_flag, channels, sample_rate;
    int32_t num_samples, loop_start, loop_end, skip;

    /* checks */
    if (!is_id32be(0x00, sf, "LOPU"))
        goto fail;

    /* .lopus: real extension (honest) */
    if (!check_extensions(sf, "lopus"))
        goto fail;

    start_offset    = read_u32le(0x04, sf);
    sample_rate     = read_s32le(0x08, sf);
    channels        = read_s16le(0x0c, sf);
    /* 0x10: ? (1984) */
    num_samples     = read_s32le(0x14, sf);
    loop_start      = read_s32le(0x18, sf);
    loop_end        = read_s32le(0x1c, sf) + 1;
    /* 0x20: frame size */
    skip            = read_s16le(0x24, sf);
    data_size       = read_u32le(0x28, sf);
    /* rest: null */

    loop_flag = (loop_end > 0); /* -1 if no loop */

    /* Must remove skip or some files decode past limit. loop_end equals to PC (.ogg) version's max
     * samples, but in some case (stage_park) goes slightly past max but is still valid.
     * (loops shouldn't remove skip as they wouldn't match PC/bgm.txt loop times) */
    num_samples -= skip;
    if (num_samples < loop_end)
        num_samples = loop_end;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_LOPU_FB;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

#ifdef VGM_USE_FFMPEG
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
