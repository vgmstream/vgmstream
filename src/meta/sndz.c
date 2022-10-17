#include "meta.h"
#include "../coding/coding.h"


/* SNDZ - Sony/SCE's lib? (cousin of SXD) [Gran Turismo 7 (PS4), Astro's Playroom (PS5)] */
VGMSTREAM* init_vgmstream_sndz(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_b = NULL;
    uint32_t stream_offset, stream_size, name_offset, data_size;
    int channels, loop_flag, sample_rate, codec, streamed;
    int32_t num_samples, loop_start, loop_end;
    uint32_t at9_config;
    uint32_t offset;
    int total_subsongs, target_subsong = sf->stream_index;


    if (!is_id32be(0x00, sf, "SNDZ"))
        goto fail;
  //head_size = read_u32le(0x04, sf);
    data_size = read_u32le(0x08, sf);
    /* 0x0c: version? (0x00010001) */
    /* 0x10: size size? */
    /* 0x14: null */
    /* 0x18/1c: some kind of ID? (shared by some files) */
    /* 0x20: bank name */


    /* .szd1: header + .szd2 = data
     * .szd/szd3: szd1 + szd2 */
    if (!check_extensions(sf, "szd1,szd,szd3"))
        goto fail;

    /* parse chunk table and WAVS with offset to offset to WAVD */
    {
        uint32_t wavs_offset;
        int i, entries;

        offset = 0x70;
        offset += read_u32le(offset, sf);

        entries = read_u32le(offset, sf);
        offset += 0x04;

        wavs_offset = 0;
        for (i = 0; i < entries; i ++) {
            if (is_id32be(offset + 0x00, sf, "WAVS")) {
                /* 0x04: size? */
                wavs_offset = read_u32le(offset + 0x08, sf);
                offset += 0x0c;
                break;
            }

            offset += 0x0c;
        }

        if (!wavs_offset)
            goto fail;
        offset += wavs_offset;

        offset += read_u32le(offset, sf);
    }

    /* parse WAVD header */
    {
        uint32_t entry_size;

        if (!is_id32be(offset + 0x00, sf, "WAVD"))
            goto fail;
        entry_size = read_u32le(offset + 0x04, sf);
        total_subsongs = read_u32le(offset + 0x08, sf);

        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

        offset += 0x0c;
        offset += entry_size * (target_subsong - 1);

        /* main header */
        streamed = read_u32le(offset + 0x00, sf);
        name_offset = read_u32le(offset + 0x04, sf) + offset + 0x04; /* from here */
        /* 08: null */
        /* 0c: size/offset? */
        codec = read_u8(offset + 0x10, sf);
        channels = read_u8(offset + 0x11, sf);
        /* 12: null */
        sample_rate = read_u32le(offset + 0x14, sf);
        num_samples = read_s32le(offset + 0x18, sf);
        at9_config = read_u32le(offset + 0x1c, sf); /* null for other codecs */
        loop_start = read_s32le(offset + 0x20, sf);
        loop_end = read_s32le(offset + 0x24, sf);
        stream_size = read_u32le(offset + 0x28, sf); /* from data start in szd2 or absolute in szd3 */
        stream_offset = read_u32le(offset + 0x2c, sf);
        /* rest: null */

        loop_flag = loop_end > 0;
    }

    /* szd3 is streamed but has header+data together, with padding between (data_size is the same as file size)*/
    if (streamed && get_streamfile_size(sf) < data_size) {
        sf_b = open_streamfile_by_ext(sf, "szd2");
        if (!sf_b) {
            vgm_logi("SNDZ: can't find companion .szd2 file\n");
            goto fail;
        }

        if (data_size > get_streamfile_size(sf_b))
            goto fail;
    }
    else {
        /* should have enough data */
        if (stream_offset + stream_size > get_streamfile_size(sf))
            goto fail;
        sf_b = sf;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SNDZ;
    vgmstream->sample_rate = sample_rate;

    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;
    if (name_offset)
        read_string(vgmstream->stream_name,STREAM_NAME_SIZE, name_offset, sf);

    switch (codec) {
        case 0x02:
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;
            break;

        case 0x04:
            vgmstream->coding_type = coding_PCM24LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;
            break;

        case 0x08:
            vgmstream->coding_type = coding_PCMFLOAT;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x04;
            break;

        case 0x20:
            vgmstream->coding_type = coding_HEVAG;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x10;
            break;

#ifdef VGM_USE_ATRAC9
        case 0x21: {
            atrac9_config cfg = {0};

            cfg.channels = channels;
            cfg.config_data = at9_config;

            vgmstream->codec_data = init_atrac9(&cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_ATRAC9;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif
        default:
            vgm_logi("SNDZ: unknown codec 0x%x\n", codec);
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sf_b, stream_offset))
        goto fail;

    if (sf_b != sf) close_streamfile(sf_b);
    return vgmstream;

fail:
    if (sf_b != sf) close_streamfile(sf_b);
    close_vgmstream(vgmstream);
    return NULL;
}
