#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"

/* .OPUS - from Switch games (Lego City Undercover, Ultra SF II, Disgaea 5) */
VGMSTREAM * init_vgmstream_nsw_opus(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag = 0, channel_count;
    int num_samples = 0, loop_start = 0, loop_end = 0;
    off_t offset = 0, data_offset;
    size_t data_size, skip = 0;

    /* check extension, case insensitive */
    if ( !check_extensions(streamFile,"opus,lopus,nop")) /* no relation to Ogg Opus */
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
    else if (read_32bitBE(0x00, streamFile) == 0x73616466 && read_32bitBE(0x08, streamFile) == 0x6f707573) { /* Xenoblade Chronicles 2 */
        offset = read_32bitLE(0x1c, streamFile);

        num_samples = read_32bitLE(0x28, streamFile);
        loop_flag = read_8bit(0x19, streamFile);
        if (loop_flag) {
            loop_start = read_32bitLE(0x2c, streamFile);
            loop_end = read_32bitLE(0x30, streamFile);
            }
    }
    else {
        offset = 0x00;
    }

    if ((uint32_t)read_32bitLE(offset + 0x00,streamFile) != 0x80000001)
        goto fail;
    
    channel_count = read_8bit(offset + 0x09, streamFile);
    /* 0x0a: packet size if CBR, 0 if VBR */
    data_offset = offset + read_32bitLE(offset + 0x10, streamFile);
    skip = read_32bitLE(offset + 0x1c, streamFile);

    if ((uint32_t)read_32bitLE(data_offset, streamFile) != 0x80000004)
        goto fail;
    
    data_size = read_32bitLE(data_offset + 0x04, streamFile);

    start_offset = data_offset + 0x08;
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
        size_t bytes;
        ffmpeg_custom_config cfg;
        ffmpeg_codec_data *ffmpeg_data;

        bytes = ffmpeg_make_opus_header(buf,0x100, vgmstream->channels, skip, vgmstream->sample_rate);
        if (bytes <= 0) goto fail;

        memset(&cfg, 0, sizeof(ffmpeg_custom_config));
        cfg.type = FFMPEG_SWITCH_OPUS;

        ffmpeg_data = init_ffmpeg_config(streamFile, buf,bytes, start_offset,data_size, &cfg);
        if (!ffmpeg_data) goto fail;

        vgmstream->codec_data = ffmpeg_data;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;
        
		if (ffmpeg_data->skipSamples <= 0) {
			ffmpeg_set_skip_samples(ffmpeg_data, skip);
		}

		if (vgmstream->num_samples == 0) {
            vgmstream->num_samples = switch_opus_get_samples(start_offset, data_size,
                vgmstream->sample_rate, streamFile) - skip;
		}
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
