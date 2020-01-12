#include "meta.h"
#include "../coding/coding.h"

/* PASX - from Premium Agency games [SoulCalibur II HD (X360), Death By Cube (X360)] */
VGMSTREAM * init_vgmstream_x360_pasx(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, chunk_offset;
    size_t data_size, chunk_size;
    int loop_flag, channel_count, sample_rate;
    int num_samples, loop_start_sample, loop_end_sample;


    /* checks */
    /* .past: Soul Calibur II HD
     * .sgb: Death By Cube */
    if ( !check_extensions(streamFile,"past,sgb"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x50415358)   /* "PASX" */
        goto fail;


    /* custom header with a "fmt " data chunk inside */
    chunk_size   = read_32bitBE(0x08,streamFile);
    data_size    = read_32bitBE(0x0c,streamFile);
    chunk_offset = read_32bitBE(0x10,streamFile); /* 0x14: fmt offset end */
    start_offset = read_32bitBE(0x18,streamFile);

    channel_count = read_16bitBE(chunk_offset+0x02,streamFile);
    sample_rate   = read_32bitBE(chunk_offset+0x04,streamFile);
    xma2_parse_fmt_chunk_extra(streamFile, chunk_offset, &loop_flag, &num_samples, &loop_start_sample, &loop_end_sample, 1);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample   = loop_end_sample;
    vgmstream->meta_type = meta_X360_PASX;

#ifdef VGM_USE_FFMPEG
    {
        uint8_t buf[0x100];
        size_t bytes;

        bytes = ffmpeg_make_riff_xma_from_fmt_chunk(buf,0x100, chunk_offset,chunk_size, data_size, streamFile, 1);
        if (bytes <= 0) goto fail;

        vgmstream->codec_data = init_ffmpeg_header_offset(streamFile, buf,bytes, start_offset,data_size);
        if ( !vgmstream->codec_data ) goto fail;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;

        xma_fix_raw_samples(vgmstream, streamFile, start_offset, data_size, chunk_offset, 1,1);
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
