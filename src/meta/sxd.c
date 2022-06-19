#include "meta.h"
#include "../coding/coding.h"


/* SXD - Sony/SCE's SNDX lib format (cousin of SGXD) [Gravity Rush, Freedom Wars, Soul Sacrifice PSV] */
VGMSTREAM* init_vgmstream_sxd(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_sxd1 = NULL, *sf_sxd2 = NULL, *sf_data = NULL, *sf_h = NULL, *sf_b = NULL;
    off_t start_offset, chunk_offset, first_offset = 0x60, name_offset = 0;
    size_t chunk_size, stream_size = 0;

    int is_dual, is_external;
    int loop_flag, channels, codec, sample_rate;
    int32_t num_samples, loop_start_sample, loop_end_sample;
    uint32_t flags, at9_config_data = 0;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    /* .sxd: header+data (SXDF)
     * .sxd1: header (SXDF) + .sxd2 = data (SXDS)
     * .sxd3: sxd1 + sxd2 pasted together (found in some PS4 games, ex. Fate Extella)*/
    if (!check_extensions(sf,"sxd,sxd2,sxd3"))
        goto fail;

    /* setup head/body variations */
    if (check_extensions(sf,"sxd2")) {
        /* sxd1+sxd2: open sxd1 as header */

        sf_h = open_streamfile_by_ext(sf, "sxd1");
        if (!sf_h) goto fail;

        sf_sxd1 = sf_h;
        sf_sxd2 = sf;
        is_dual = 1;
    }
    else if (check_extensions(sf,"sxd3")) {
        /* sxd3: make subfiles for head and body to simplify parsing */
        off_t  sxd1_offset  = 0x00;
        size_t sxd1_size    = read_u32le(0x08, sf);
        off_t  sxd2_offset  = sxd1_size;
        size_t sxd2_size    = get_streamfile_size(sf) - sxd1_size;

        sf_h = setup_subfile_streamfile(sf, sxd1_offset, sxd1_size, "sxd1");
        if (!sf_h) goto fail;

        sf_b = setup_subfile_streamfile(sf, sxd2_offset, sxd2_size, "sxd2");
        if (!sf_b) goto fail;

        sf_sxd1 = sf_h;
        sf_sxd2 = sf_b;
        is_dual = 1;
    }
    else {
        /* sxd: use the current file as header */
        sf_sxd1 = sf;
        sf_sxd2 = NULL;
        is_dual = 0;
    }

    if (sf_sxd1 && !is_id32be(0x00, sf_sxd1, "SXDF"))
        goto fail;
    if (sf_sxd2 && !is_id32be(0x00, sf_sxd2, "SXDS"))
        goto fail;


    /* typical chunks: NAME, WAVE and many control chunks (SXDs don't need to contain any sound data) */
    if (!find_chunk_le(sf_sxd1, get_id32be("WAVE"),first_offset,0, &chunk_offset,&chunk_size))
        goto fail;

    /* check multi-streams (usually only in SFX containers) */
    total_subsongs = read_s32le(chunk_offset + 0x04,sf_sxd1);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;
    // todo rarely a WAVE subsong may point to a repeated data offset, with different tags only
    

    /* read stream header */
    {
        uint32_t table_offset, header_offset, stream_offset;

        /* get target offset using table of relative offsets within WAVE */
        table_offset  = chunk_offset + 0x08 + 4 * (target_subsong - 1);
        header_offset = table_offset + read_32bitLE(table_offset,sf_sxd1);

        flags               = read_u32le(header_offset+0x00,sf_sxd1);
        codec               = read_u8   (header_offset+0x04,sf_sxd1);
        channels            = read_u8   (header_offset+0x05,sf_sxd1);
        /* 0x06(2): unknown, rarely 0xFF */
        sample_rate         = read_s32le(header_offset+0x08,sf_sxd1);
        /* 0x0c(4): unknown size? (0x4000/0x3999/0x3333/etc, not related to extra data) */
        /* 0x10(4): ? + volume? + pan? (can be 0 for music) */
        num_samples         = read_s32le(header_offset+0x14,sf_sxd1);
        loop_start_sample   = read_s32le(header_offset+0x18,sf_sxd1);
        loop_end_sample     = read_s32le(header_offset+0x1c,sf_sxd1);
        stream_size         = read_u32le(header_offset+0x20,sf_sxd1);
        stream_offset       = read_u32le(header_offset+0x24,sf_sxd1);

        loop_flag = loop_start_sample != -1 && loop_end_sample != -1;

        /* known flag combos:
         *  0x00: Chaos Rings 2 sfx (RAM + no tags)
         *  0x01: common (RAM + tags)
         *  0x02: Chaos Rings 3 sfx (stream + no tags)
         *  0x03: common (stream + tags)
         *  0x05: Gravity Rush 2 sfx (RAM + tags) */
        //has_tags = flags & 1;
        is_external = flags & 2;
        //unknown = flags & 4; /* no apparent differences with/without it? */

        /* flag 1 signals TLV-like extra data. Format appears to be 0x00(1)=tag?, 0x01(1)=extra size*32b?, 0x02(2)=config?
         * but not always (ex. size=0x07 but actually=0), perhaps only some bits are used or varies with tag, or are subflags.
         * A tag may appear with or without extra data (even 0x0a), 0x01/03/04/06 are common at the beginnig (imply number of tags?),
         * 0x64/7F are common at the end (but not always), 0x0A=ATRAC9 config, 0x0B/0C appear with RAM preloading data
         * (most variable in Soul Sacrifice; total TLVs size isn't plainly found in the SXD header AFAIK). */

        /* manually try to find ATRAC9 tag */
        if (codec == 0x42) {
            off_t extra_offset = header_offset + 0x28;
            off_t max_offset = chunk_offset + chunk_size;

            if (!(flags & 1))
                goto fail;

            while (extra_offset < max_offset) {
                uint32_t tag = read_u32be(extra_offset, sf_sxd1);
                if (tag == 0x0A010000 || tag == 0x0A010600) {
                    at9_config_data = read_u32le(extra_offset+0x04, sf_sxd1); /* yes, LE */
                    break;
                }

                extra_offset += 0x04;
            }
            if (!at9_config_data)
                goto fail;
        }

        /* usually .sxd=header+data and .sxd1=header + .sxd2=data, but rarely sxd1 may contain data [The Last Guardian (PS4)] */
        if (is_external) {
            start_offset = stream_offset; /* absolute if external */
        } else {
            start_offset = header_offset + 0x24 + stream_offset; /* from current entry offset if internal */
        }
    }

    /* get stream name (NAME is tied to REQD/cues, and SFX cues repeat WAVEs, but should work ok for streams) */
    if (is_dual && find_chunk_le(sf_sxd1, get_id32be("NAME"),first_offset,0, &chunk_offset,NULL)) {
        /* table: relative offset (32b) + hash? (32b) + cue index (32b) */
        int i;
        int num_entries = read_s16le(chunk_offset + 0x04, sf_sxd1); /* can be bigger than streams */
        for (i = 0; i < num_entries; i++) {
            uint32_t index = read_u32le(chunk_offset + 0x08 + 0x08 + i * 0x0c,sf_sxd1);
            if (index+1 == target_subsong) {
                name_offset = chunk_offset + 0x08 + 0x00 + i*0x0c + read_u32le(chunk_offset + 0x08 + 0x00 + i * 0x0c, sf_sxd1);
                break;
            }
        }
    }

    if (is_external && !is_dual) {
        VGM_LOG("SXD: found single sxd with external data\n");
        goto fail;
    }

    /* even dual files may have some non-external streams */
    if (is_external) {
        sf_data = sf_sxd2;
    } else {
        sf_data = sf_sxd1;
    }

    if (start_offset > get_streamfile_size(sf_data)) {
        VGM_LOG("SXD: wrong location?\n");
        goto fail;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SXD;
    vgmstream->sample_rate = sample_rate;

    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample = loop_end_sample;

    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;
    if (name_offset)
        read_string(vgmstream->stream_name,STREAM_NAME_SIZE, name_offset,sf_sxd1);

    switch (codec) {
        case 0x20:      /* PS-ADPCM [Hot Shots Golf: World Invitational (Vita) sfx] */
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x10;
            break;

        case 0x21:      /* HEVAG [Gravity Rush (Vita) sfx] */
            vgmstream->coding_type = coding_HEVAG;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x10;
            break;

#ifdef VGM_USE_ATRAC9
        case 0x42: {    /* ATRAC9 [Soul Sacrifice (Vita), Freedom Wars (Vita), Gravity Rush 2 (PS4)] */
            atrac9_config cfg = {0};

            cfg.channels = vgmstream->channels;
            cfg.config_data = at9_config_data;

            vgmstream->codec_data = init_atrac9(&cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_ATRAC9;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif
      //case 0x28:      /* dummy codec? (found with 0 samples) [Hot Shots Golf: World Invitational (Vita) sfx] */
        default:
            VGM_LOG("SXD: unknown codec 0x%x\n", codec);
            goto fail;
    }


    /* open the file for reading */
    if (!vgmstream_open_stream(vgmstream, sf_data, start_offset))
        goto fail;

    if (sf_h) close_streamfile(sf_h);
    if (sf_b) close_streamfile(sf_b);
    return vgmstream;

fail:
    if (sf_h) close_streamfile(sf_h);
    if (sf_b) close_streamfile(sf_b);
    close_vgmstream(vgmstream);
    return NULL;
}
