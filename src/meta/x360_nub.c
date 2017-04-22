#include "meta.h"
#include "../coding/coding.h"

/* Namco NUB xma - from Tekken 6, Galaga Legions DX */
VGMSTREAM * init_vgmstream_x360_nub(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, chunk_offset;
    size_t data_size, chunk_size;
    int loop_flag, channel_count, sample_rate, chunk_type;
    int num_samples, loop_start_sample, loop_end_sample;


    /* check extension, case insensitive */
    if ( !check_extensions(streamFile,"xma")) /* (probably meant to be .nub) */
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x786D6100)   /* "xma\0" */
        goto fail;


    /* custom header with a "XMA2" or "fmt " chunk inside; most other values are unknown */
    chunk_type   = read_32bitBE(0xC,streamFile);
    start_offset = 0x100;
    data_size    = read_32bitBE(0x14,streamFile);
    chunk_offset = 0xBC;
    chunk_size   = read_32bitBE(0x24,streamFile);
    if (chunk_type == 0x4) { /* "XMA2" */
        xma2_parse_xma2_chunk(streamFile, chunk_offset, &channel_count,&sample_rate, &loop_flag, &num_samples, &loop_start_sample, &loop_end_sample);
    } else if (chunk_type == 0x8) { /* "fmt " */
        channel_count = read_16bitBE(chunk_offset+0x02,streamFile);
        sample_rate   = read_32bitBE(chunk_offset+0x04,streamFile);
        xma2_parse_fmt_chunk_extra(streamFile, chunk_offset, &loop_flag, &num_samples, &loop_start_sample, &loop_end_sample, 1);
    } else {
        goto fail;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample   = loop_end_sample;
    vgmstream->meta_type = meta_NUB_XMA;

#ifdef VGM_USE_FFMPEG
    {
        uint8_t buf[0x100];
        size_t bytes;

        if (chunk_type == 0x4) { /* "XMA2" */
            bytes = ffmpeg_make_riff_xma2_from_xma2_chunk(buf,0x100, chunk_offset,chunk_size, data_size, streamFile);
        } else { /* "fmt " */
            bytes = ffmpeg_make_riff_xma_from_fmt_chunk(buf,0x100, chunk_offset,chunk_size, data_size, streamFile, 1);
        }
        if (bytes <= 0) goto fail;

        vgmstream->codec_data = init_ffmpeg_header_offset(streamFile, buf,bytes, start_offset,data_size);;
        if ( !vgmstream->codec_data ) goto fail;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;
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
