#include "meta.h"
#include "../coding/coding.h"

typedef enum { XMA2 } gtd_codec;

/* GTD - found in Knights Contract (X360, PS3), Valhalla Knights 3 (PSV) */
VGMSTREAM * init_vgmstream_gtd(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, chunk_offset;
    size_t data_size, chunk_size;
    int loop_flag, channel_count, sample_rate;
    int num_samples, loop_start_sample, loop_end_sample;
    gtd_codec codec;


    /* check extension, case insensitive */
    if ( !check_extensions(streamFile,"gtd"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x47485320)   /* "GHS " */
        goto fail;

    /* header type, not formally specified */
    if (read_32bitBE(0x04,streamFile) == 1 && read_16bitBE(0x0C,streamFile) == 0x0166) { /* XMA2 */
        /* 0x08(4): seek table size */
        chunk_offset = 0x0c; /* custom header with a "fmt " data chunk inside */
        chunk_size = 0x34;

        channel_count = read_16bitBE(chunk_offset+0x02,streamFile);
        sample_rate   = read_32bitBE(chunk_offset+0x04,streamFile);
        xma2_parse_fmt_chunk_extra(streamFile, chunk_offset, &loop_flag, &num_samples, &loop_start_sample, &loop_end_sample, 1);

        start_offset = read_32bitBE(0x58,streamFile); /* always 0x800 */
        data_size = read_32bitBE(0x5c,streamFile);
        /* 0x40(18): null,  0x60(4): header size (0x70), 0x64(4): seek table size again, 0x68(8): null */
        /* 0x70: seek table; then a "STPR" chunk with the file ID and filename */

        codec = XMA2;
    }
    else {
        /* there are PSV (LE, ATRAC9) and PS3 (MSF inside?) variations, somewhat-but-not-quite similar
         * (contain the "STPR" chunk but the rest is mostly different) */
        goto fail;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample   = loop_end_sample;
    vgmstream->meta_type = meta_GTD;

    switch(codec) {
#ifdef VGM_USE_FFMPEG
        case XMA2: {
            uint8_t buf[0x100];
            size_t bytes;

            bytes = ffmpeg_make_riff_xma_from_fmt_chunk(buf,0x100, chunk_offset,chunk_size, data_size, streamFile, 1);
            if (bytes <= 0) goto fail;

            vgmstream->codec_data = init_ffmpeg_header_offset(streamFile, buf,bytes, start_offset,data_size);
            if ( !vgmstream->codec_data ) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif
        default:
            goto fail;
    }


    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
