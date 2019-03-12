#include "meta.h"
#include "../coding/coding.h"


/* .STRM - from Abylight 3DS games [Cursed Castilla (3DS)] */
VGMSTREAM * init_vgmstream_strm_abylight(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count, sample_rate;
    size_t data_size;


    /* check extension */
    if ( !check_extensions(streamFile,"strm") )
        goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x5354524D) /* "STRM" */
        goto fail;
    if (read_32bitLE(0x04,streamFile) != 0x03E8) /* version 1000? */
        goto fail;

    loop_flag = 0;
    channel_count = 2; /* there are various possible fields but all files are stereo */
    sample_rate = read_32bitLE(0x08,streamFile);

    start_offset = 0x1e;
    data_size = read_32bitLE(0x10,streamFile);
    if (data_size != get_streamfile_size(streamFile) - start_offset)
        goto fail;
    if (data_size != read_32bitLE(0x18,streamFile))
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = aac_get_samples(streamFile, start_offset, data_size);

    vgmstream->meta_type = meta_STRM_ABYLIGHT;

#ifdef VGM_USE_FFMPEG
    {
        ffmpeg_codec_data *ffmpeg_data = NULL;

        ffmpeg_data = init_ffmpeg_offset(streamFile, start_offset,data_size);
        if (!ffmpeg_data) goto fail;
        vgmstream->codec_data = ffmpeg_data;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;

        /* apparently none, or maybe ~600 */
        //if (!ffmpeg_data->skipSamples)
        //    ffmpeg_set_skip_samples(ffmpeg_data, 1024);
        //vgmstream->num_samples -= 1024;
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
