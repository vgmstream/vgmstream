#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"

/* RXWS - from Sony SCEI PS2 games (Okage: Shadow King, Genji, Bokura no Kazoku) */
VGMSTREAM * init_vgmstream_ps2_rxws(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamHeader = NULL;
    off_t start_offset, chunk_offset, name_offset = 0;
    size_t stream_size, chunk_size;
    int loop_flag = 0, channel_count, is_separate = 0, type, sample_rate;
    int32_t num_samples, loop_start;
    int total_subsongs, target_subsong = streamFile->stream_index;

    /* checks */
    /* .xws: header and data
     * .xwh+xwb: header + data (.bin+dat are also found in Wild Arms 4/5) */
    if (!check_extensions(streamFile,"xws,xwb"))
        goto fail;
    is_separate = check_extensions(streamFile,"xwb");

    /* xwh+xwb: use xwh as header; otherwise use the current file */
    if (is_separate) {
        /* extra check to reject Microsoft's XWB faster */
        if ((read_32bitBE(0x00,streamFile) == 0x57424E44) ||    /* "WBND" (LE) */
            (read_32bitBE(0x00,streamFile) == 0x444E4257))      /* "DNBW" (BE) */
            goto fail;

        streamHeader = open_streamfile_by_ext(streamFile, "xwh");
        if (!streamHeader) goto fail;
    } else {
        streamHeader = streamFile;
    }
    if (read_32bitBE(0x00,streamHeader) != 0x52585753) /* "RXWS" */
        goto fail;

    /* file size (just the .xwh/xws) */
    if (read_32bitLE(0x04,streamHeader)+0x10 != get_streamfile_size(streamHeader))
        goto fail;
    /* 0x08(4): version (0x100/0x200), 0x0C: null */

    /* typical chunks: FORM, FTXT, MARK, BODY (for .xws) */
    if (read_32bitBE(0x10,streamHeader) != 0x464F524D) /* "FORM", main header (always first) */
        goto fail;
    chunk_size = read_32bitLE(0x10+0x04,streamHeader); /* size - 0x10 */
    /* 0x08 version (0x100), 0x0c: null */
    chunk_offset = 0x20;


    /* check multi-streams */
    total_subsongs = read_32bitLE(chunk_offset+0x00,streamHeader);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;


    /* read stream header */
    {
        off_t header_offset = chunk_offset + 0x4 + 0x1c * (target_subsong-1); /* position in FORM */
        off_t stream_offset, next_stream_offset, data_offset = 0;

        type = read_8bit(header_offset+0x00, streamHeader);
        /* 0x01(1): unknown (always 0x1c), 0x02(2): flags? (usually 8002/0002, & 0x01 if looped) */
        /* 0x04(4): vol/pan stuff? (0x00007F7F), 0x08(1): null?, 0x0c(4): null? */
        channel_count =    read_8bit(header_offset+0x09, streamHeader);
        sample_rate = (uint16_t)read_16bitLE(header_offset+0x0a,streamHeader);
        stream_offset = read_32bitLE(header_offset+0x10,streamHeader);
        num_samples   = read_32bitLE(header_offset+0x14,streamHeader);
        loop_start    = read_32bitLE(header_offset+0x18,streamHeader);
        loop_flag = (loop_start != 0xFFFFFFFF);

        /* find data start and size */
        if (is_separate) {
            data_offset = 0x00;
        }
        else {
            off_t current_chunk = 0x10;
            /* note the extra 0x10 in chunk_size/offsets */
            while (current_chunk < get_streamfile_size(streamFile)) {
                if (read_32bitBE(current_chunk,streamFile) == 0x424F4459) { /* "BODY" chunk_type */
                    data_offset = 0x10 + current_chunk;
                    break;
                }
                current_chunk += 0x10 + read_32bitLE(current_chunk+4,streamFile);
            }
            if (!data_offset) goto fail;
        }

        if (target_subsong == total_subsongs) {
            next_stream_offset = get_streamfile_size(is_separate ? streamFile : streamHeader) - data_offset;
        } else {
            off_t next_header_offset = chunk_offset + 0x4 + 0x1c * (target_subsong);
            next_stream_offset = read_32bitLE(next_header_offset+0x10,streamHeader);
        }

        stream_size = next_stream_offset - stream_offset;
        start_offset = data_offset + stream_offset;
    }

    /* get stream name (always follows FORM) */
    if (read_32bitBE(0x10+0x10 + chunk_size,streamHeader) == 0x46545854) { /* "FTXT" */
        chunk_offset = 0x10+0x10 + chunk_size + 0x10;
        if (read_32bitLE(chunk_offset+0x00,streamHeader) == total_subsongs) {
            name_offset = chunk_offset + read_32bitLE(chunk_offset+0x04 + (target_subsong-1)*0x04,streamHeader);
        }
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;
    vgmstream->meta_type = meta_PS2_RXWS;
    if (name_offset)
        read_string(vgmstream->stream_name,STREAM_NAME_SIZE, name_offset,streamHeader);

    switch (type) {
        case 0x00:      /* PS-ADPCM */
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x10;

            vgmstream->num_samples = ps_bytes_to_samples(num_samples, channel_count);
            vgmstream->loop_start_sample = ps_bytes_to_samples(loop_start, channel_count);
            vgmstream->loop_end_sample = vgmstream->num_samples;
            break;

        case 0x01:      /* PCM */
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x2;

            vgmstream->num_samples = pcm_bytes_to_samples(num_samples, channel_count, 16);
            vgmstream->loop_start_sample = pcm_bytes_to_samples(loop_start, channel_count, 16);
            vgmstream->loop_end_sample = vgmstream->num_samples;
            break;

#ifdef VGM_USE_FFMPEG
        case 0x02: {    /* ATRAC3 */
            int block_align, encoder_delay;

            block_align = 0xc0 * channel_count;
            encoder_delay = 1024 + 69*2; /* observed default */
            vgmstream->num_samples = num_samples - encoder_delay;

            vgmstream->codec_data = init_ffmpeg_atrac3_raw(streamFile, start_offset,stream_size, vgmstream->num_samples,vgmstream->channels,vgmstream->sample_rate, block_align, encoder_delay);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->loop_start_sample = loop_start;
            vgmstream->loop_end_sample   = vgmstream->num_samples;
            break;
        }
#endif
        default:
            goto fail;
    }

    /* open the file for reading */
    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    if (is_separate && streamHeader) close_streamfile(streamHeader);
    return vgmstream;

fail:
    if (is_separate && streamHeader) close_streamfile(streamHeader);
    close_vgmstream(vgmstream);
    return NULL;
}


/* .RXW - legacy fake ext/header for poorly split XWH+XWB files generated by old tools (incorrect header/chunk sizes) */
VGMSTREAM * init_vgmstream_ps2_rxw(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    int loop_flag=0, channel_count;
    off_t start_offset;

    /* check extension, case insensitive */
    if (!check_extensions(streamFile,"rxw")) goto fail;

    /* check RXWS/FORM Header */
    if (!((read_32bitBE(0x00,streamFile) == 0x52585753) && 
          (read_32bitBE(0x10,streamFile) == 0x464F524D)))
        goto fail;

    loop_flag = (read_32bitLE(0x3C,streamFile)!=0xFFFFFFFF);
    channel_count=2; /* Always stereo files */
    
    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(0x2E,streamFile);
    vgmstream->num_samples = (read_32bitLE(0x38,streamFile)*28/16)/2;

    /* Get loop point values */
    if(vgmstream->loop_flag) {
        vgmstream->loop_start_sample = read_32bitLE(0x3C,streamFile)/16*14;
        vgmstream->loop_end_sample = read_32bitLE(0x38,streamFile)/16*14;
    }

    vgmstream->interleave_block_size = read_32bitLE(0x1c,streamFile)+0x10;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->meta_type = meta_PS2_RXWS;
    start_offset = 0x40;

    /* open the file for reading */
    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
