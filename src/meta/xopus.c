#include "meta.h"
#include "../coding/coding.h"


/* XOPUS - from Exient games [Angry Birds: Transformers (Android), Angry Birds: Go (Android)] */
VGMSTREAM* init_vgmstream_xopus(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, table_offset;
    int loop_flag = 0, channels, sample_rate, num_samples, skip;
    size_t data_size;
    int entries;


    /* checks*/
    if (!check_extensions(sf, "xopus"))
        goto fail;
    if (read_u32be(0x00, sf) != 0x584F7075) /* "XOpu" */
        goto fail;

    /* 0x04: always 0x01? */
    channels = read_u8(0x05, sf);
    /* 0x06: always 0x30? */
    /* 0x08: always 0xc8? max allowed packet size? */
    num_samples = read_s32le(0x0c, sf);
    skip = read_s32le(0x10, sf);
    entries = read_u32le(0x14, sf);
    data_size = read_u32le(0x18, sf);
    /* 0x1c: unused */
    /* 0x20+: packet sizes table */

    sample_rate = 48000;
    loop_flag = 0;

    table_offset = 0x20;
    start_offset = table_offset + 0x02*entries;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_XOPUS;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;

#ifdef VGM_USE_FFMPEG
    {
        vgmstream->codec_data = init_ffmpeg_x_opus(sf, table_offset, entries, start_offset, data_size, vgmstream->channels, skip);
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
