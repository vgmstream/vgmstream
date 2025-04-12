#include "meta.h"
#include "../coding/coding.h"
#include "../util/endianness.h"

typedef enum { NONE, PCM16LE, MSADPCM, XMA2, ATRAC9, HEVAG } gtd_codec_t;

static void read_name(VGMSTREAM* vgmstream, STREAMFILE* sf, uint32_t offset);


/* GHS - Hexadrive's HexaEngine games [Gunslinger Stratos (AC), Knights Contract (X360), Valhalla Knights 3 (Vita)] */
VGMSTREAM* init_vgmstream_ghs(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t stream_offset, stream_size = 0, stpr_offset = 0, loop_start_offset = 0, loop_end_offset = 0;
    uint32_t chunk_offset = 0, chunk_size = 0, block_size, at9_config_data = 0;
    int loop_flag = 0, channels, sample_rate;
    int32_t num_samples, loop_start_sample, loop_end_sample;
    gtd_codec_t codec = NONE;


    /* checks */
    if (!is_id32be(0x00,sf, "GHS "))
        return NULL;
    if (!check_extensions(sf,"gtd"))
        return NULL;

    int big_endian = guess_endian32(0x04,sf);
    read_u32_t read_u32 = big_endian ? read_u32be : read_u32le;
    read_u16_t read_u16 = big_endian ? read_u16be : read_u16le;

    int target_subsong = sf->stream_index;
    int total_subsongs = read_u32(0x04, sf); /* seen in sfx packs inside .ged */
    if (!check_subsongs(&target_subsong, total_subsongs))
        return NULL;


    // header version is not formally specified, use v1 channels as test (v2 has sample rate in that position)
    uint32_t version_test = read_u32le(0x10, sf);
    bool is_v1 = (version_test < 16);

    if (is_v1) {
        uint32_t offset = 0x08;
        stream_offset = 0x00;
        for (int i = 0; i < total_subsongs; i++) {
            int format    = read_u32(offset + 0x00,sf);
            int temp_size = read_u32(offset + 0x04,sf);

            if (i + 1 == target_subsong) {
                if (format == 0x0000) {
                    codec = PCM16LE; /* VK3 voices */
                    block_size = 0x02;
                }
                else if (format == 0x0001) {
                    codec = HEVAG; /* VK3 voices */
                    block_size = 0x10;
                }
                else if (format == 0x0002) {
                    codec = ATRAC9; /* VK3 bgm */
                    block_size = 0;
                }
                else {
                    VGM_LOG("GHS: unknown v1 format %x\n", format);
                    goto fail;
                }

                stream_size         = read_u32(offset + 0x04,sf);
                channels            = read_u32(offset + 0x08,sf);
                sample_rate         = read_u32(offset + 0x0c,sf);
                // 10: null/bps in PCM?
                loop_start_offset   = read_u32(offset + 0x14,sf);
                loop_end_offset     = read_u32(offset + 0x18,sf);
                // 1c: channel layout in ATRAC9?
                at9_config_data     = read_u32be(offset + 0x20,sf);

                loop_flag = loop_end_offset > loop_start_offset;
            }

            offset += 0x24;
            if (i + 1 < target_subsong) {
                stream_offset += temp_size;
            }
        }

        if (codec == NONE)
            goto fail;

        stream_offset += offset;

        // STPR subheader
        if (codec == ATRAC9) {
            if (target_subsong > 1) //unknown STPR position for other subsongs
                return NULL;
            stpr_offset = stream_offset;
            stream_offset = read_u32(stpr_offset + 0x04,sf) + 0x34;
        } 
    }
    else {
        // 0x08: size of all seek tables (XMA2, all tables go together after headers) / null
        uint32_t offset = 0x0c + (target_subsong - 1) * 0x64; 
        
        int format = read_u16(offset + 0x00,sf);
        if (format == 0x0001) {
            codec = PCM16LE; /* GS bgm */
        }
        else if (format == 0x0002) {
            codec = MSADPCM; /* GS sfx */
        }
        else if (format == 0x0166) {
            codec = XMA2;
            chunk_offset = offset; /* "fmt " */
            chunk_size = 0x34;
        }
        else {
            VGM_LOG("GHS: unknown v2 format %x\n", format);
            goto fail;
        }

        /* 0x0c: standard fmt chunk (depending on format, reserved with padding up to 0x48 if needed) */
        channels            = read_u16(offset + 0x02,sf);
        sample_rate         = read_u32(offset + 0x04,sf);
        block_size          = read_u16(offset + 0x0c,sf);
        /* loops can be found at 0x28/2c in PCM16 (also later) */
        stream_offset       = read_u32(offset + 0x4c,sf); /* always 0x800 */
        stream_size         = read_u32(offset + 0x50,sf);
        /* 0x54: seek table offset (XMA2) / data start */
        /* 0x58: seek table size (XMA2) / null */
        loop_start_sample   = read_u32(offset + 0x5c,sf); /* null in XMA2 */
        loop_end_sample     = read_u32(offset + 0x60,sf) + loop_start_sample; /* +1? */

        if (codec == XMA2) {
            xma2_parse_fmt_chunk_extra(sf, chunk_offset, &loop_flag, &num_samples, &loop_start_sample, &loop_end_sample, 1);
        }
        else {
            loop_flag = loop_end_sample != 0;
        }

        stpr_offset = read_u32(offset + 0x54,sf) + read_u32(offset + 0x58,sf);
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_GHS;
    vgmstream->sample_rate = sample_rate;
    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample = loop_end_sample;

    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;
    read_name(vgmstream, sf, stpr_offset);

    switch(codec) {
        case PCM16LE:
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = block_size / channels;

            vgmstream->num_samples = pcm16_bytes_to_samples(stream_size, channels);

            break;

        case MSADPCM:
            vgmstream->coding_type = coding_MSADPCM;
            vgmstream->layout_type = layout_interleave;
            vgmstream->frame_size = block_size;

            vgmstream->num_samples = msadpcm_bytes_to_samples(stream_size, block_size, channels);

            break;

        case HEVAG:
            vgmstream->coding_type = coding_HEVAG;
            vgmstream->layout_type = layout_interleave;
            vgmstream->frame_size = block_size;

            vgmstream->num_samples = ps_bytes_to_samples(stream_size, channels);

            break;

#ifdef VGM_USE_FFMPEG
        case XMA2:
            vgmstream->codec_data = init_ffmpeg_xma_chunk(sf, stream_offset, stream_size, chunk_offset, chunk_size);
            if ( !vgmstream->codec_data ) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = num_samples;

            xma_fix_raw_samples(vgmstream, sf, stream_offset, stream_size, chunk_offset, 1,1);
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
                vgmstream->loop_start_sample = atrac9_bytes_to_samples(loop_start_offset - stream_offset, vgmstream->codec_data);
                vgmstream->loop_end_sample = atrac9_bytes_to_samples(loop_end_offset - stream_offset, vgmstream->codec_data);
            }
            vgmstream->num_samples = atrac9_bytes_to_samples(stream_size, vgmstream->codec_data);
            break;
        }
#endif

        default:
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream, sf, stream_offset))
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
    uint32_t subfile_offset, subfile_size, stpr_offset;


    /* checks */
    if (!is_id64be(0x00,sf,"S_P_STH\x01"))
        return NULL;
    if (!check_extensions(sf,"gtd"))
        return NULL;

    subfile_offset = read_u32be(0x08, sf);
    subfile_size = get_streamfile_size(sf) - subfile_offset;

    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, "msf");
    if (!temp_sf) goto fail;

    vgmstream = init_vgmstream_msf(temp_sf);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_GHS;

    stpr_offset = 0x00;
    read_name(vgmstream, sf, stpr_offset);

    close_streamfile(temp_sf);
    return vgmstream;
fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}

/* S_PACK - Hexadrive's HexaEngine games [Gunslinger Stratos (PC), Knights Contract (X360)] */
VGMSTREAM* init_vgmstream_s_pack(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;

    /* checks */
    if (!is_id64be(0x00,sf,"S_PACK\x00\x00") && !is_id64be(0x00,sf,"S_PACK\x00\x01")) /* v1: KC */
        return NULL;
    if (!check_extensions(sf,"ged"))
        return NULL;
    /* 0x08: file size */
    /* 0x0c-0x20: null */

    int big_endian = guess_endian32(0x20,sf);
    read_u32_t read_u32 = big_endian ? read_u32be : read_u32le;

    uint32_t offset = read_u32(0x20, sf); /* offset to minitable */
    /* 0x24: number of chunks in S_P_H? */
    /* 0x28: number of entries in minitable */

    /* minitable */
    /* 0x00: offset to "S_P_H", that seems to have cuenames (may have more cues than waves though) */
    uint32_t subfile_offset = read_u32(offset + 0x04, sf); /* may be null or S_CHR_M (no GHS in file) */
    uint32_t schar_offset = read_u32(offset + 0x08, sf); /* S_CHR_M seen in KC, some kind of cues */

    if (schar_offset == 0)
        schar_offset = get_streamfile_size(sf);
    uint32_t subfile_size = schar_offset - subfile_offset;

    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, "gtd");
    if (!temp_sf) goto fail;

    vgmstream = init_vgmstream_ghs(temp_sf);
    if (!vgmstream) goto fail;

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}

static void read_name(VGMSTREAM* vgmstream, STREAMFILE* sf, uint32_t offset) {
    uint32_t name_offset = 0;

    //if (!offset) //may be 0 in PS3
    //    return;

    if (is_id32be(offset,sf, "STPR"))
        offset += 0x08;
    
    if (is_id64be(offset + 0x00,sf, "S_P_STH\0")) { /* stream header v0: GS, VK3 */
        /* 08 subheader size */
        /* 0c version/count? */
        /* 10 version/count? */
        /* 20 offset to header configs */
        /* 24 hash? */
        /* 2c -1? */
        /* 30 1? */
        if (!is_id64be(offset + 0x40,sf, "stream\0\0"))
            return;
        /* 50 bank name */
        /* 70+ header configs (some are repeats from GHS) */
        /* E0 file name .gtd */

        name_offset = offset + 0xE0; /* show file name though actual files are already (bankname)_(filename).gtd */
    }
    else if (is_id64be(offset + 0x00,sf, "S_P_STH\1")) { /* stream header v1: KC */
        /* same as above, except no stream+bank name, so at 0x40 are header configs */
        name_offset = offset + 0xB0;
    }

    /* optional (only found in streams, sfx packs that point to GHS have cue names) */
    if (!name_offset)
        return;

    //TO-DO: Shift-Jis in some Vita files
    read_string(vgmstream->stream_name,STREAM_NAME_SIZE, name_offset,sf);
}
