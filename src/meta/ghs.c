#include "meta.h"
#include "../coding/coding.h"

typedef enum { XMA2, ATRAC9 } gtd_codec;
//TODO rename gtd to ghs
/* GHS - Hexadrive's HexaEngine games [Knights Contract (X360), Valhalla Knights 3 (Vita)] */
VGMSTREAM* init_vgmstream_ghs(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, chunk_offset, stpr_offset, name_offset = 0, loop_start_offset, loop_end_offset;
    size_t data_size, chunk_size;
    int loop_flag, channels, sample_rate;
    int num_samples, loop_start_sample, loop_end_sample;
    uint32_t at9_config_data;
    gtd_codec codec;


    /* checks */
    if (!is_id32be(0x00,sf, "GHS "))
        goto fail;
    if ( !check_extensions(sf,"gtd"))
        goto fail;


    /* header type, not formally specified */
    if (read_32bitBE(0x04,sf) == 1 && read_16bitBE(0x0C,sf) == 0x0166) { /* XMA2 */
        /* 0x08(4): seek table size */
        chunk_offset = 0x0c; /* custom header with a "fmt " data chunk inside */
        chunk_size = 0x34;

        channels = read_16bitBE(chunk_offset+0x02,sf);
        sample_rate   = read_32bitBE(chunk_offset+0x04,sf);
        xma2_parse_fmt_chunk_extra(sf, chunk_offset, &loop_flag, &num_samples, &loop_start_sample, &loop_end_sample, 1);

        start_offset = read_32bitBE(0x58,sf); /* always 0x800 */
        data_size = read_32bitBE(0x5c,sf);
        /* 0x34(18): null,  0x54(4): seek table offset, 0x58(4): seek table size, 0x5c(8): null, 0x64: seek table */

        stpr_offset = read_32bitBE(chunk_offset+0x54,sf) + read_32bitBE(chunk_offset+0x58,sf);
        if (is_id32be(stpr_offset,sf, "STPR")) {
            /* SRPR encases the original "S_P_STH" header (no data) */
            name_offset = stpr_offset + 0xB8; /* there are offsets fields but seems to work */
        }

        codec = XMA2;
    }
    else if (0x34 + read_32bitLE(0x30,sf) + read_32bitLE(0x0c,sf) == get_streamfile_size(sf)) { /* ATRAC9 */

        data_size = read_32bitLE(0x0c,sf);
        start_offset = 0x34 + read_32bitLE(0x30,sf);
        channels   = read_32bitLE(0x10,sf);
        sample_rate     = read_32bitLE(0x14,sf);
        loop_start_offset = read_32bitLE(0x1c, sf);
        loop_end_offset = read_32bitLE(0x20, sf);
        loop_flag = loop_end_offset > loop_start_offset;
        at9_config_data = read_32bitBE(0x28,sf);
        /* 0x18-0x28: fixed/unknown values */

        stpr_offset = 0x2c;
        if (is_id32be(stpr_offset,sf, "STPR")) {
            /* STPR encases the original "S_P_STH" header (no data) */
            name_offset = stpr_offset + 0xE8; /* there are offsets fields but seems to work */
        }

        codec = ATRAC9;
    }
    else {
        goto fail;
    }



    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample   = loop_end_sample;
    vgmstream->meta_type = meta_GHS;
    if (name_offset) //encoding is Shift-Jis in some PSV files
        read_string(vgmstream->stream_name,STREAM_NAME_SIZE, name_offset,sf);

    switch(codec) {
#ifdef VGM_USE_FFMPEG
        case XMA2:
            vgmstream->codec_data = init_ffmpeg_xma_chunk(sf, start_offset, data_size, chunk_offset, chunk_size);
            if ( !vgmstream->codec_data ) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = num_samples;

            xma_fix_raw_samples(vgmstream, sf, start_offset, data_size, chunk_offset, 1,1);
            break;
#endif
#ifdef VGM_USE_ATRAC9
        case ATRAC9: {
            atrac9_config cfg = {0};

            cfg.channels = vgmstream->channels;
            cfg.config_data = at9_config_data;

            vgmstream->codec_data = init_atrac9(&cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_ATRAC9;
            vgmstream->layout_type = layout_none;
            if (loop_flag) {
                vgmstream->loop_start_sample = atrac9_bytes_to_samples(loop_start_offset - start_offset, vgmstream->codec_data);
                vgmstream->loop_end_sample = atrac9_bytes_to_samples(loop_end_offset - start_offset, vgmstream->codec_data);
            }
            vgmstream->num_samples = atrac9_bytes_to_samples(data_size, vgmstream->codec_data);
            break;
        }

#endif

        default:
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* S_P_STH - Hexadrive's HexaEngine games [Knights Contract (PS3)] */
VGMSTREAM* init_vgmstream_s_p_sth(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    uint32_t subfile_offset, subfile_size, name_offset;


    /* checks */
    if (!is_id64be(0x00,sf,"S_P_STH\x01"))
        goto fail;
    if (!check_extensions(sf,"gtd"))
        goto fail;

    subfile_offset = read_u32be(0x08, sf);
    subfile_size = get_streamfile_size(sf) - subfile_offset;

    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, "msf");
    if (!temp_sf) goto fail;

    vgmstream = init_vgmstream_msf(temp_sf);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_GHS;
    name_offset = 0xB0; /* there are offsets fields but seems to work */
    read_string(vgmstream->stream_name, STREAM_NAME_SIZE, name_offset, sf);

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
