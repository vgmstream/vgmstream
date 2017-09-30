#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"

/* .OPUS - from Lego City Undercover (Switch)  */
VGMSTREAM * init_vgmstream_nsw_opus(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag = 0, channel_count;

    /* check extension, case insensitive */
    if ( !check_extensions(streamFile,"opus")) /* no relation to Ogg Opus */
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x01000080)
        goto fail;

    start_offset = 0x28;
    channel_count = read_8bit(0x09,streamFile); /* assumed */
    /* other values in the header: no idea */

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(0x0c,streamFile);
    vgmstream->meta_type = meta_NSW_OPUS;

#ifdef VGM_USE_FFMPEG
    {
        uint8_t buf[0x100];
        size_t bytes, skip, data_size;
        ffmpeg_custom_config cfg;

        data_size = get_streamfile_size(streamFile) - start_offset;
        skip = 0; //todo

        bytes = ffmpeg_make_opus_header(buf,0x100, vgmstream->channels, skip, vgmstream->sample_rate);
        if (bytes <= 0) goto fail;

        memset(&cfg, 0, sizeof(ffmpeg_custom_config));
        cfg.type = FFMPEG_SWITCH_OPUS;

        vgmstream->codec_data = init_ffmpeg_config(streamFile, buf,bytes, start_offset,data_size, &cfg);
        if (!vgmstream->codec_data) goto fail;

        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;

        vgmstream->num_samples = switch_opus_get_samples(start_offset, data_size, vgmstream->sample_rate, streamFile);
    }
#else
    goto fail;
#endif

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
