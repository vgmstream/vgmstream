#include "meta.h"
#include "../coding/coding.h"


/* XSH+XSD/XSS - from Treyarch games */
VGMSTREAM* init_vgmstream_xsh_xsd_xss(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_body = NULL;
    uint32_t offset;
    int loop_flag, channels, codec, sample_rate;


    /* checks */
    uint32_t version = read_u32le(0x00, sf);
    if (version  < 0x009D || version > 0x101)
        return NULL;
    if (read_u32le(0x04, sf) != 0)
        return NULL;

    if (!check_extensions(sf, "xsh"))
        return NULL;

    int total_subsongs = read_u32le(0x08, sf);
    int target_subsong = sf->stream_index;
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    uint32_t stream_type, stream_offset, stream_size, flags;
    uint32_t name_offset, name_size;
    int32_t num_samples;
    switch(version) {
        case 0x009D: /* Spider-Man 2002 (Xbox) */
            offset = 0x0c + (target_subsong-1) * 0x60;

            name_offset = offset + 0x00;
            name_size = 0x20;
            offset += 0x20;

            stream_type = read_u32le(offset + 0x00,sf);
            stream_offset = read_u32le(offset + 0x04,sf);
            stream_size = read_u32le(offset + 0x08,sf);
            flags = read_u32le(offset + 0x14,sf);
            /* 0x18: flags? */
            num_samples = 0;

            offset += 0x1c;
            break;

        case 0x0100: /* Kelly Slater's Pro Surfer (Xbox) */
        case 0x0101: /* Minority Report: Everybody Runs (Xbox), NHL 2K3 (Xbox) */
            /* NHL has stream IDs instead of names */
            if (read_u32le(0x0c,sf) > 0x1000) {
                offset = 0x0c + (target_subsong-1) * 0x64;
                name_offset = offset + 0x00;
                name_size = 0x20;
                offset += 0x20;
            }
            else {
                offset = 0x0c + (target_subsong-1) * 0x48;
                name_offset = offset + 0x00;
                name_size = 0x00;
                offset += 0x04;
            }

            stream_type = read_u32le(offset + 0x00,sf);
            stream_offset = read_u32le(offset + 0x04,sf);
            stream_size = read_u32le(offset + 0x08,sf);
            flags = read_u32le(offset + 0x14,sf);
            num_samples = read_u32le(offset + 0x18,sf);
            /* 0x1c: flags? */

            offset += 0x20;
            break;

        default:
            return NULL;
    }

    // full loop flag (ex. Spider-Man city_e.XSH#7,22)
    loop_flag = (flags & 0x01) != 0;

    if (stream_type > 2)
        return NULL;

    // 0x00: floats x4 (volume/pan/etc? usually 1.0, 1.0, 10.0, 10.0)
    codec = read_u16le(offset + 0x10,sf);
    channels = read_u16le(offset + 0x12,sf);
    sample_rate = read_u32le(offset + 0x14,sf);
    // 0x18: avg bitrate
    // 0x1c: block size
    // 0x1e: bps
    // 0x20: 2?

    if (stream_type == 0) {
        vgmstream = init_vgmstream_silence_container(total_subsongs);
        if (!vgmstream) goto fail;

        close_streamfile(sf_body);
        return vgmstream;
    }

    if (flags & 0x04) {
        char filename[255];
        switch (version) {
            case 0x009D:
                /* stream is a named .xss, with stream_offset/size = 0 */
                read_string(filename, name_size, name_offset,sf);
                strcat(filename, ".xss");

                sf_body = open_streamfile_by_filename(sf,filename);
                if (!sf_body) {
                    vgm_logi("XSH: external file '%s' not found (put together)\n", filename);
                    goto fail;
                }

                /* xss is playable externally, so this is mostly for show */                
                vgmstream = init_vgmstream_riff(sf_body);
                if (!vgmstream) goto fail;

                vgmstream->num_streams = total_subsongs;
                read_string(vgmstream->stream_name, name_size, name_offset,sf);

                // external .xss shouldn't have loop info though
                if (loop_flag && !vgmstream->loop_flag) {
                    vgmstream_force_loop(vgmstream, loop_flag, 0, vgmstream->num_samples);
                }

                close_streamfile(sf_body);
                return vgmstream;
                //break;

            case 0x0100:
            case 0x0101:
                /* bigfile with all streams */
                snprintf(filename, sizeof(filename), "%s", "STREAMS.XSS");
                sf_body = open_streamfile_by_filename(sf,filename);
                if (!sf_body) {
                    vgm_logi("XSH: external file '%s' not found (put together)\n", filename);
                    goto fail;
                }
                break;

            default:
                goto fail;
        }
    }
    else {
        sf_body = open_streamfile_by_ext(sf,"xsd");
        if (!sf_body) {
            VGM_LOG("XSH: can't find XSD");
            goto fail;
        }
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_XSH_XSD_XSS;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;

    switch(codec) {
        case 0x0069:
            vgmstream->coding_type = coding_XBOX_IMA;
            vgmstream->layout_type = layout_none;

            if (!num_samples)
                num_samples = xbox_ima_bytes_to_samples(stream_size, channels);

            vgmstream->num_samples = num_samples;
            vgmstream->loop_start_sample = 0;
            vgmstream->loop_end_sample = num_samples;
            break;
    }

    read_string(vgmstream->stream_name, name_size, name_offset,sf);

    if (!vgmstream_open_stream(vgmstream, sf_body, stream_offset))
        goto fail;
    close_streamfile(sf_body);
    return vgmstream;

fail:
    close_streamfile(sf_body);
    close_vgmstream(vgmstream);
    return NULL;
}
