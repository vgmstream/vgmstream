#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"

/* .OPUS - from Switch games (Lego City Undercover, Ultra SF II, Disgaea 5) */
VGMSTREAM * init_vgmstream_nsw_opus(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag = 0, channel_count;
    int num_samples = 0, loop_start = 0, loop_end = 0;
    off_t offset = 0;

    /* check extension, case insensitive */
    if ( !check_extensions(streamFile,"opus")) /* no relation to Ogg Opus */
        goto fail;

    /* variations, maybe custom */
    if (read_32bitBE(0x00,streamFile) == 0x01000080) { /* Lego City Undercover */
        offset = 0x00;
    }
    else if ((read_32bitBE(0x04,streamFile) == 0x00000000 && read_32bitBE(0x0c,streamFile) == 0x00000000) ||
             (read_32bitBE(0x04,streamFile) == 0xFFFFFFFF && read_32bitBE(0x0c,streamFile) == 0xFFFFFFFF)) { /* Disgaea 5 */
        offset = 0x10;

        loop_start = read_32bitLE(0x00,streamFile);
        loop_end = read_32bitLE(0x08,streamFile);
    }
    else if (read_32bitLE(0x04,streamFile) == 0x02) { /* Ultra Street Fighter II */
        offset = read_32bitLE(0x1c,streamFile);

        num_samples = read_32bitLE(0x00,streamFile);
        loop_start = read_32bitLE(0x08,streamFile);
        loop_end = read_32bitLE(0x0c,streamFile);
    }
    else {
        offset = 0x00;
    }

    if (read_32bitBE(offset + 0x00,streamFile) != 0x01000080)
        goto fail;

    start_offset = offset + 0x28;
    channel_count = read_8bit(offset + 0x09,streamFile); /* assumed */
    /* 0x0a: packet size if CBR?, other values: no idea */

    loop_flag = (loop_end > 0); /* -1 when not set */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(offset + 0x0c,streamFile);
    vgmstream->meta_type = meta_NSW_OPUS;

    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

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

        if (vgmstream->num_samples == 0)
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
