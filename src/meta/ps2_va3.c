#include "meta.h"
#include "../coding/coding.h"


/* VA3 - Konami / Sony Atrac3 Container [PS2 DDR Supernova 2 Arcade] */
VGMSTREAM * init_vgmstream_va3(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;
    uint32_t data_size;

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

    vgmstream->meta_type = meta_VA3;
    vgmstream->sample_rate = read_32bitLE(0x14, streamFile);
    vgmstream->num_samples = read_32bitLE(0x08, streamFile);
    vgmstream->channels = channel_count;

#ifdef VGM_USE_FFMPEG
    {
        int block_align, encoder_delay;

        block_align = 0xC0 * vgmstream->channels;
        encoder_delay = 0; //todo

        vgmstream->codec_data = init_ffmpeg_atrac3_raw(streamFile, start_offset,data_size, vgmstream->num_samples,vgmstream->channels,vgmstream->sample_rate, block_align, encoder_delay);
        if (!vgmstream->codec_data) goto fail;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;
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
