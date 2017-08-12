#include "meta.h"
#include "../coding/coding.h"

typedef enum { XMA2 } gtd_codec;

/* GTD - found in Knights Contract (X360, PS3), Valhalla Knights 3 (PSV) */
VGMSTREAM * init_vgmstream_gtd(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, chunk_offset, stpr_offset, name_offset = 0;
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
        /* 0x34(18): null,  0x54(4): seek table offset, 0x58(4): seek table size, 0x5c(8): null, 0x64: seek table */

        stpr_offset = read_32bitBE(chunk_offset+0x54,streamFile) + read_32bitBE(chunk_offset+0x58,streamFile);;
        if (read_32bitBE(stpr_offset,streamFile) == 0x53545052) { /* "STPR" */
            name_offset = stpr_offset + 0xB8; /* there are offsets fields but seems to work */
        }

        codec = XMA2;
    }
    else {
        /* there are PSV (LE, ATRAC9 data) and PS3 (MSF inside?) variations, somewhat-but-not-quite similar */

        /* for PSV: */
        /* 0x0c: data_size, 0x10: channles, 0x14: sample rate, 0x18-0x2c: fixed and unknown values */
        /* 0x2c: STPR chunk, with name_offset at + 0xE8 */

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
    if (name_offset) //encoding is Shift-Jis in some PSV files
        read_string(vgmstream->stream_name,STREAM_NAME_SIZE, name_offset,streamFile);

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
