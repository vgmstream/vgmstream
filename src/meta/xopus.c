#include "meta.h"
#include "../coding/coding.h"


/* XOPUS - from Rovio games [Angry Birds: Transformers (Android), Angry Birds: Go (Android)] */
VGMSTREAM * init_vgmstream_xopus(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag = 0, channel_count, sample_rate, num_samples, skip;
    size_t data_size;
    int entries;


    /* checks*/
    if (!check_extensions(streamFile, "xopus"))
        goto fail;
    if (read_32bitBE(0x00, streamFile) != 0x584F7075)    /* "XOpu" */
        goto fail;

    /* 0x04: always 1? 0x30? */
    channel_count = read_8bit(0x05, streamFile);
    /* 0x06: always 0x30? */
    /* 0x08: always 0xc8? encoder delay? */
    num_samples = read_32bitLE(0x0c, streamFile);
    /* 0x10: always 0x138? max packet size? */
    entries = read_32bitLE(0x14, streamFile); /* packet sizes */
    /* 0x18: data size */
    /* 0x1c: unused */
    /* 0x20+: packet sizes table */


    sample_rate = 48000;
    loop_flag = 0;

    start_offset = 0x20 + 0x02*entries;
    data_size = get_streamfile_size(streamFile) - start_offset;
    skip = x_opus_get_encoder_delay(start_offset, streamFile);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_XOPUS;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples /*- skip*/;

#ifdef VGM_USE_FFMPEG
    {
        vgmstream->codec_data = init_ffmpeg_x_opus(streamFile, start_offset,data_size, vgmstream->channels, skip, vgmstream->sample_rate);
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
