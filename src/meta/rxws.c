#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"

/* RXWS - from Sony SCEI PS2 games (Okage: Shadow King, Genji, Bokura no Kazoku) */
VGMSTREAM* init_vgmstream_rxws(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sh = NULL;
    off_t start_offset, chunk_offset, name_offset = 0;
    size_t stream_size, chunk_size;
    int loop_flag = 0, channels, is_separate = 0, type, sample_rate;
    int32_t num_samples, loop_start;
    int total_subsongs, target_subsong = sf->stream_index;

    /* checks */
    /* .xws: header and data
     * .xwh+xwb: header + data (.bin+dat are also found in Wild Arms 4/5) */
    if (!check_extensions(sf,"xws,xwb"))
        goto fail;
    is_separate = check_extensions(sf,"xwb");

    /* xwh+xwb: use xwh as header; otherwise use the current file */
    if (is_separate) {
        /* extra check to reject Microsoft's XWB faster */
        if (is_id32be(0x00,sf,"WBND") ||    /* (LE) */
            is_id32be(0x00,sf,"DNBW"))      /* (BE) */
            goto fail;

        sh = open_streamfile_by_ext(sf, "xwh");
        if (!sh) goto fail;
    } else {
        sh = sf;
    }
    if (!is_id32be(0x00,sh,"RXWS"))
        goto fail;

    /* file size (just the .xwh/xws) */
    if (read_u32le(0x04,sh) + 0x10 != get_streamfile_size(sh))
        goto fail;
    /* 0x08: version (0x100/0x200)
     * 0x0C: null */

    /* typical chunks: FORM, FTXT, MARK, BODY (for .xws) */
    if (!is_id32be(0x10,sh,"FORM")) /* main header (always first) */
        goto fail;
    chunk_size = read_u32le(0x10+0x04,sh); /* size - 0x10 */
    /* 0x08 version (0x100), 0x0c: null */
    chunk_offset = 0x20;


    /* check multi-streams */
    total_subsongs = read_s32le(chunk_offset+0x00,sh);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;


    /* read stream header */
    {
        off_t header_offset = chunk_offset + 0x4 + 0x1c * (target_subsong-1); /* position in FORM */
        off_t stream_offset, next_stream_offset, data_offset = 0;

        type = read_u8(header_offset+0x00, sh);
        /* 0x01: unknown (always 0x1c) */
        /* 0x02: flags? (usually 8002/0002, & 0x01 if looped) */
        /* 0x04: vol/pan stuff? (0x00007F7F) */
        /* 0x08: null? */
        channels =    read_u8(header_offset+0x09, sh);
        /* 0x0c: null? */
        sample_rate   = read_u16le(header_offset+0x0a,sh);
        stream_offset = read_u32le(header_offset+0x10,sh);
        num_samples   = read_s32le(header_offset+0x14,sh);
        loop_start    = read_s32le(header_offset+0x18,sh);
        loop_flag = (loop_start >= 0);

        /* find data start and size */
        if (is_separate) {
            data_offset = 0x00;
        }
        else {
            off_t current_chunk = 0x10;
            /* note the extra 0x10 in chunk_size/offsets */
            while (current_chunk < get_streamfile_size(sf)) {
                if (is_id32be(current_chunk,sf, "BODY")) {
                    data_offset = 0x10 + current_chunk;
                    break;
                }
                current_chunk += 0x10 + read_u32le(current_chunk+4,sf);
            }
            if (!data_offset) goto fail;
        }

        if (target_subsong == total_subsongs) {
            next_stream_offset = get_streamfile_size(is_separate ? sf : sh) - data_offset;
        } else {
            off_t next_header_offset = chunk_offset + 0x4 + 0x1c * (target_subsong);
            next_stream_offset = read_u32le(next_header_offset+0x10,sh);
        }

        stream_size = next_stream_offset - stream_offset;
        start_offset = data_offset + stream_offset;
    }

    /* get stream name (always follows FORM) */
    if (is_id32be(0x10+0x10 + chunk_size,sh, "FTXT")) {
        chunk_offset = 0x10+0x10 + chunk_size + 0x10;
        if (read_s32le(chunk_offset+0x00,sh) == total_subsongs) {
            name_offset = chunk_offset + read_u32le(chunk_offset+0x04 + (target_subsong-1)*0x04,sh);
        }
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_RXWS;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;
    if (name_offset)
        read_string(vgmstream->stream_name,STREAM_NAME_SIZE, name_offset,sh);

    switch (type) {
        case 0x00:      /* PS-ADPCM */
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x10;

            vgmstream->num_samples = ps_bytes_to_samples(num_samples, channels);
            vgmstream->loop_start_sample = ps_bytes_to_samples(loop_start, channels);
            vgmstream->loop_end_sample = vgmstream->num_samples;
            break;

        case 0x01:      /* PCM */
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x2;

            vgmstream->num_samples = pcm_bytes_to_samples(num_samples, channels, 16);
            vgmstream->loop_start_sample = pcm_bytes_to_samples(loop_start, channels, 16);
            vgmstream->loop_end_sample = vgmstream->num_samples;
            break;

#ifdef VGM_USE_FFMPEG
        case 0x02: {    /* ATRAC3 */
            int block_align, encoder_delay;

            block_align = 0xc0 * channels;
            encoder_delay = 1024 + 69*2; /* observed default */
            vgmstream->num_samples = num_samples - encoder_delay;

            vgmstream->codec_data = init_ffmpeg_atrac3_raw(sf, start_offset,stream_size, vgmstream->num_samples,vgmstream->channels,vgmstream->sample_rate, block_align, encoder_delay);
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
    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;

    if (is_separate && sh) close_streamfile(sh);
    return vgmstream;

fail:
    if (is_separate && sh) close_streamfile(sh);
    close_vgmstream(vgmstream);
    return NULL;
}


/* .RXW - legacy fake ext/header for poorly split XWH+XWB files generated by old tools (incorrect header/chunk sizes) */
VGMSTREAM* init_vgmstream_rxws_badrip(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    int loop_flag=0, channels;
    off_t start_offset;

    /* check extension, case insensitive */
    if (!check_extensions(sf,"rxw"))
        goto fail;

    /* check RXWS/FORM Header */
    if (!((read_32bitBE(0x00,sf) == 0x52585753) &&
          (read_32bitBE(0x10,sf) == 0x464F524D)))
        goto fail;

    loop_flag = (read_u32le(0x3C,sf)!=0xFFFFFFFF);
    channels=2; /* Always stereo files */
    
    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(0x2E,sf);
    vgmstream->num_samples = (read_32bitLE(0x38,sf)*28/16)/2;

    /* Get loop point values */
    if(vgmstream->loop_flag) {
        vgmstream->loop_start_sample = read_32bitLE(0x3C,sf)/16*14;
        vgmstream->loop_end_sample = read_32bitLE(0x38,sf)/16*14;
    }

    vgmstream->interleave_block_size = read_32bitLE(0x1c,sf)+0x10;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->meta_type = meta_RXWS;
    start_offset = 0x40;

    /* open the file for reading */
    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
