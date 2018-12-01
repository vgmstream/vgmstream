#include "meta.h"
#include "../coding/coding.h"


/* UE4OPUS - from Unreal Engine 4 games [ARK: Survival Evolved (PC), Fortnite (PC)] */
VGMSTREAM * init_vgmstream_ue4opus(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag = 0, channel_count, sample_rate, num_samples, skip;
    size_t data_size;


    /* checks*/
    /* .opus/lopus: possible real extension
     * .ue4opus: header id */
    if (!check_extensions(streamFile, "opus,lopus,ue4opus"))
        goto fail;
    if (read_32bitBE(0x00, streamFile) != 0x5545344F &&  /* "UE4O" */
        read_32bitBE(0x00, streamFile) != 0x50555300)    /* "PUS\0" */
        goto fail;


    sample_rate = (uint16_t)read_16bitLE(0x08, streamFile);
    num_samples = read_32bitLE(0x0a, streamFile); /* may be less or equal to file num_samples */
    channel_count = read_8bit(0x0e, streamFile);
    /* 0x0f(2): frame count */
    loop_flag = 0;

    start_offset = 0x11;
    data_size = get_streamfile_size(streamFile) - start_offset;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_UE4OPUS;
    vgmstream->sample_rate = sample_rate;

#ifdef VGM_USE_FFMPEG
    {
        /* usually uses 60ms for music (delay of 360 samples) */
        skip = ue4_opus_get_encoder_delay(start_offset, streamFile);
        vgmstream->num_samples = num_samples - skip;

        vgmstream->codec_data = init_ffmpeg_ue4_opus(streamFile, start_offset,data_size, vgmstream->channels, skip, vgmstream->sample_rate);
        if (!vgmstream->codec_data) goto fail;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;
    }
#else
    goto fail;
#endif

    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
