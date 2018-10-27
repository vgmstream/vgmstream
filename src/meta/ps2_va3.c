#include "meta.h"
#include "../coding/coding.h"


/* VA3 - Konami / Sony Atrac3 Container [PS2 DDR Supernova 2 Arcade] */
VGMSTREAM * init_vgmstream_va3(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;
    uint32_t data_size, loop_start, loop_end;

    /* check extension, case insensitive */
    if (!check_extensions(streamFile, "va3"))
        goto fail;

    if (read_32bitBE(0x00, streamFile) != 0x21334156)   /* "!3AV" */
        goto fail;

    /* va3 header */
    start_offset = 0x800;
    data_size = read_32bitLE(0x04, streamFile);// get_streamfile_size(streamFile) - start_offset;
    // pretty sure 0x4 LE 32 bit is some sort of filesize...

    loop_flag = 0;
    //0x18 is 1... what is this?
    channel_count = 2;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;
    
    
    vgmstream->sample_rate = read_32bitLE(0x14, streamFile);
    vgmstream->num_samples = read_32bitLE(0x08, streamFile);
    vgmstream->channels = channel_count;
    vgmstream->meta_type = meta_VA3;
    loop_start = 0;
    loop_end = 0;

#ifdef VGM_USE_FFMPEG
    {
        ffmpeg_codec_data *ffmpeg_data = NULL;
        uint8_t buf[200];
        int32_t bytes, samples_size = 1024, block_size, encoder_delay, joint_stereo;
        block_size = 0xC0 * vgmstream->channels;
        //max_samples = (data_size / block_size) * samples_size;
        encoder_delay = 0x0;
        joint_stereo = 0;

        /* make a fake riff so FFmpeg can parse the ATRAC3 */
        bytes = ffmpeg_make_riff_atrac3(buf, 200, vgmstream->num_samples, data_size, vgmstream->channels, vgmstream->sample_rate, block_size, joint_stereo, encoder_delay);
        if (bytes <= 0) goto fail;

        ffmpeg_data = init_ffmpeg_header_offset(streamFile, buf, bytes, start_offset, data_size);
        if (!ffmpeg_data) goto fail;
        vgmstream->codec_data = ffmpeg_data;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;
        //vgmstream->num_samples = max_samples;

        if (loop_flag) {
            vgmstream->loop_start_sample = (loop_start / block_size) * samples_size;
            vgmstream->loop_end_sample = (loop_end / block_size) * samples_size;
        }

    }
#else
    goto fail;
#endif

    /* open the file for reading */
    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
