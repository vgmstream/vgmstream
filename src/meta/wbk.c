#include "meta.h"
#include "../coding/coding.h"
//#include <ctype.h>

/* .WBK - seen in some Treyarch games [Spider-Man 2, Ultimate Spider-Man, Call of Duty 2: Big Red One] */
VGMSTREAM* init_vgmstream_wbk(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t table_offset, entry_offset, data_offset, strings_offset, coefsec_offset,
        name_offset, codec, flags, channels, sound_offset, sound_size, num_samples, sample_rate;
    int target_subsong = sf->stream_index, total_subsongs, loop_flag, has_names, i;

    /* checks */
    if (!is_id32be(0x00, sf, "WAVE") ||
        !is_id32be(0x04, sf, "BK11"))
        goto fail;

    if (!check_extensions(sf, "wbk"))
        goto fail;

    /* always little endian, even on GC */
    data_offset = read_u32le(0x10, sf);
    //data_size = read_u32le(0x14, sf);
    //streams_offset = read_u32le(0x18, sf);
    //streams_size = read_u32le(0x1c, sf);

    total_subsongs = read_u32le(0x40, sf);
    table_offset = read_u32le(0x44, sf);

    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1)
        goto fail;

    //paramsec_size = read_u32le(0x50, sf);
    //paramsec_offset = read_u32le(0x54, sf);
    //coefsec_size = read_u32le(0x58, sf);
    coefsec_offset = read_u32le(0x5c, sf);
    //strings_size = read_u32le(0x60, sf);
    strings_offset = read_u32le(0x64, sf);

    /* Ultimate Spider-Man has no names, only name hashes */
    {
        size_t len;

        /* check if the first sound points at the first or the second string */
        len = read_string(NULL, STREAM_NAME_SIZE, strings_offset, sf);
        name_offset = read_u32le(table_offset + 0x00, sf);
        has_names = (name_offset == 0x00 || name_offset == len + 0x01);
    }

    /* 0x00: name offset/name hash
     * 0x04: codec
     * 0x05: flags
     * 0x06: channel mask (can actually only be mono or stereo)
     * 0x07: padding
     * 0x08: sound size
     * 0x0c: number of samples
     * 0x10: group name offset
     * 0x14: parameters offset
     * 0x18: DSP coefs offset, usually not set (-1)
     * 0x1c: sound offset
     * 0x20: sample rate
     * 0x24: always 0?
     *
     * struct slightly changed in Call of Duty 2 but still compatible
     */
    entry_offset = table_offset + (target_subsong - 1) * 0x28;
    name_offset = read_u32le(entry_offset + 0x00, sf);
    codec = read_u8(entry_offset + 0x04, sf);
    flags = read_u8(entry_offset + 0x05, sf);
    channels = read_u8(entry_offset + 0x06, sf) == 0x03 ? 2 : 1;
    sound_size = read_u32le(entry_offset + 0x08, sf);
    num_samples = read_u32le(entry_offset + 0x0c, sf);
    sound_offset = read_u32le(entry_offset + 0x1c, sf);
    sample_rate = read_u16le(entry_offset + 0x20, sf);

    if (!(flags & 0x02)) /* streamed sounds have absolute offset */
        sound_offset += data_offset;

    loop_flag = (flags & 0x08);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->meta_type = meta_WBK;
    vgmstream->sample_rate = sample_rate;
    vgmstream->stream_size = sound_size;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = 0;
    vgmstream->loop_end_sample = num_samples; /* full loops only */
    vgmstream->num_streams = total_subsongs;

    switch (codec) {
        case 0x03: { /* DSP */
            uint32_t coef_offset;
            static const int16_t coef_table[16] = {
                0x0216,0xfc9f,0x026c,0x04b4,0x065e,0xfdec,0x0a11,0xfd1e,
                0x0588,0xfc38,0x05ad,0x01da,0x083b,0xfdbc,0x08c3,0xff18
            };

            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x8000;

            coef_offset = read_u32le(entry_offset + 0x18, sf);
            if (coef_offset == UINT32_MAX || coefsec_offset == 0x00) {
                /* hardcoded coef table */
                for (i = 0; i < vgmstream->channels; i++)
                    memcpy(vgmstream->ch[i].adpcm_coef, coef_table, sizeof(coef_table));
            } else {
                if (coefsec_offset == UINT32_MAX)
                    goto fail;

                dsp_read_coefs_be(vgmstream, sf, coefsec_offset + coef_offset, 0x28);
            }
            break;
        }

        case 0x04: /* PSX */
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x800;
            break;

        case 0x05: /* XBOX */
            vgmstream->coding_type = coding_XBOX_IMA;
            vgmstream->layout_type = layout_none;
            break;

        case 0x07: /* IMA */
            vgmstream->coding_type = coding_IMA;
            vgmstream->layout_type = layout_none;

            /* for some reason, number of samples is off for IMA */
            vgmstream->num_samples = ima_bytes_to_samples(sound_size, channels);
            vgmstream->loop_end_sample = vgmstream->num_samples;
            break;

        default:
            goto fail;
    }

    if (has_names)
        read_string(vgmstream->stream_name, STREAM_NAME_SIZE, strings_offset + name_offset, sf);
    else
        snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%08x", name_offset);

    if (!vgmstream_open_stream(vgmstream, sf, sound_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* Ultimate Spider-Man string hashing algorithm, for reference */
#if 0
static uint32_t wbk_hasher(const char* input) {
    uint32_t hash = 0;

    for (const char* ch = input; *ch; ch++) {
        hash += hash*32 + tolower(*ch);
    }

    return hash;
}
#endif

/* .WBK - evolution of the above Treyarch bank format [Call of Duty 3] */
VGMSTREAM* init_vgmstream_wbk_nslb(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t table_offset, name_table_offset, strings_offset, info_offset, data_offset, streams_offset,
        name_offset, entry_offset, info_entry_offset,
        codec, flags, channels, sound_offset, sound_size, num_samples, sample_rate;
    int target_subsong = sf->stream_index, total_subsongs, loop_flag;

    /* checks */
    if (!is_id32be(0x00, sf, "NSLB"))
        goto fail;

    if (!check_extensions(sf, "wbk"))
        goto fail;

    /* always little endian, even on PS3/X360 */
    if (read_u16le(0x04, sf) != 0x01)
        goto fail;

    total_subsongs = read_u32le(0x10, sf);

    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1)
        goto fail;

    name_table_offset = read_u32le(0x18, sf);
    table_offset = read_u32le(0x1c, sf);
    //info_size = read_u32le(0x20, sf);
    info_offset = read_u32le(0x24, sf);
    //strings_size = read_u32le(0x28, sf);
    strings_offset = read_u32le(0x2c, sf);
    //data_size = read_u32le(0x30, sf);
    data_offset = read_u32le(0x34, sf);
    //streams_size = read_u32le(0x38, sf);
    streams_offset = read_u32le(0x3c, sf);

    name_offset = read_u32le(name_table_offset + 0x04 * (target_subsong - 1), sf);
    entry_offset = table_offset + 0x10 * (target_subsong - 1);

    info_entry_offset = read_u32le(entry_offset + 0x00, sf) + info_offset;
    sound_offset = read_u32le(entry_offset + 0x04, sf);
    sound_size = read_u32le(entry_offset + 0x08, sf);
    num_samples = read_u32le(entry_offset + 0x0c, sf);

    sample_rate = read_u16le(info_entry_offset + 0x00, sf);
    codec = read_u8(info_entry_offset + 0x04, sf);
    flags = read_u8(info_entry_offset + 0x05, sf);
    channels = read_u8(info_entry_offset + 0x06, sf) == 0x03 ? 2 : 1;

    if (flags & 0x01)
        sound_offset += streams_offset;
    else
        sound_offset += data_offset;

    loop_flag = (flags & 0x02);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->meta_type = meta_WBK_NSLB;
    vgmstream->sample_rate = sample_rate;
    vgmstream->stream_size = sound_size;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = 0;
    vgmstream->loop_end_sample = num_samples; /* full loops only */
    vgmstream->num_streams = total_subsongs;

    switch (codec) {
        case 0x20: /* XBOX */
            vgmstream->coding_type = coding_XBOX_IMA;
            vgmstream->layout_type = layout_none;
            break;

        case 0x21: /* PSX */
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = (flags & 0x01) ? 0x800 : sound_size / channels;
            break;

        case 0x22: { /* DSP */
            int i, j;
            off_t coef_table_offset;

            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = (flags & 0x01) ? 0x8000 : sound_size / channels;

            /* check info entry variation */
            if (read_u32le(info_entry_offset + 0x44 + 0x30*channels, sf) != 0x00)
                coef_table_offset = 0x4c; /* Call of Duty 3 */
            else
                coef_table_offset = 0x44; /* Kung Fu Panda */

            /* It looks like the table is re-interpreted as 8 32-bit integers and stored with little endian byte order
             * like the rest of the data. Fun times. */
            for (i = 0; i < vgmstream->channels; i++) {
                off_t coef_offset = info_entry_offset + coef_table_offset + 0x30*i;
                for (j = 0; j < 8; j++) {
                    vgmstream->ch[i].adpcm_coef[j*2]   = read_s16le(coef_offset + j*0x04 + 0x02, sf);
                    vgmstream->ch[i].adpcm_coef[j*2+1] = read_s16le(coef_offset + j*0x04 + 0x00, sf);
                }
            }
            break;
        }

        case 0x25: { /* FSB IMA */
            VGMSTREAM* fsb_vgmstream;
            STREAMFILE* temp_sf;

            /* skip "fsb3adpc" */
            sound_offset += 0x08;
            sound_size -= 0x08;
            temp_sf = setup_subfile_streamfile(sf, sound_offset, sound_size, "fsb");
            if (!temp_sf) goto fail;
            temp_sf->stream_index = 0;

            fsb_vgmstream = init_vgmstream_fsb(temp_sf);
            close_streamfile(temp_sf);
            if (!fsb_vgmstream) goto fail;

            fsb_vgmstream->meta_type = vgmstream->meta_type;
            fsb_vgmstream->num_streams = vgmstream->num_streams;
            vgmstream_force_loop(fsb_vgmstream, loop_flag, 0, fsb_vgmstream->num_samples);
            read_string(fsb_vgmstream->stream_name, STREAM_NAME_SIZE, strings_offset + name_offset, sf);

            close_vgmstream(vgmstream);
            return fsb_vgmstream;
        }

#ifdef VGM_USE_FFMPEG
        case 0x30: { /* RIFF XMA */
            off_t riff_fmt_offset, riff_data_offset;
            size_t riff_fmt_size, riff_data_size;

            sound_offset += 0x0c;
            sound_size -= 0x0c;

            /* find "fmt" chunk */
            if (!find_chunk_riff_le(sf, 0x666d7420, sound_offset, sound_size, &riff_fmt_offset, &riff_fmt_size))
                goto fail;

            /* find "data" chunk */
            if (!find_chunk_riff_le(sf, 0x64617461, sound_offset, sound_size, &riff_data_offset, &riff_data_size))
                goto fail;

            sound_offset = riff_data_offset;

            vgmstream->codec_data = init_ffmpeg_xma_chunk(sf, riff_data_offset, riff_data_size, riff_fmt_offset, riff_fmt_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            xma_fix_raw_samples(vgmstream, sf, riff_data_offset, riff_data_size, riff_fmt_offset, 0, 0);
            break;
        }
#endif

#ifdef VGM_USE_MPEG
        case 0x32: { /* MP3 */
            coding_t mpeg_coding;

            vgmstream->codec_data = init_mpeg(sf, sound_offset, &mpeg_coding, channels);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = mpeg_coding;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif

        default:
            goto fail;
    }

    if (codec != 0x21) /* name table is filled with garbage on PS2 for some reason */
        read_string(vgmstream->stream_name, STREAM_NAME_SIZE, strings_offset + name_offset, sf);

    if (!vgmstream_open_stream(vgmstream, sf, sound_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
