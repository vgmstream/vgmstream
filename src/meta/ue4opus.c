#include "meta.h"
#include "../coding/coding.h"


/* UE4OPUS - from Unreal Engine 4 games [ARK: Survival Evolved (PC), Fortnite (PC)] */
VGMSTREAM * init_vgmstream_ue4opus(STREAMFILE *sf) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag = 0, channels, sample_rate, num_samples, skip;
    size_t data_size;


    /* checks*/
    /* .opus/lopus: possible real extension
     * .ue4opus: header id */
    if (!check_extensions(sf, "opus,lopus,ue4opus"))
        goto fail;
    if (!is_id64be(0x00, sf, "UE4OPUS\0"))
        goto fail;


    sample_rate = read_u16le(0x08, sf);
    num_samples = read_s32le(0x0a, sf); /* may be less or equal to file num_samples */
    channels = read_u8(0x0e, sf);
    /* 0x0f(2): frame count */
    loop_flag = 0;

    start_offset = 0x11;
    data_size = get_streamfile_size(sf) - start_offset;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_UE4OPUS;
    vgmstream->sample_rate = sample_rate;

#ifdef VGM_USE_FFMPEG
    {
        /* usually uses 60ms for music (delay of 360 samples) */
        skip = ue4_opus_get_encoder_delay(start_offset, sf);
        vgmstream->num_samples = num_samples - skip;

        vgmstream->codec_data = init_ffmpeg_ue4_opus(sf, start_offset,data_size, vgmstream->channels, skip, vgmstream->sample_rate);
        if (!vgmstream->codec_data) goto fail;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;
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
