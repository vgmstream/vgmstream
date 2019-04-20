#include "meta.h"
#include "../coding/coding.h"


/* SXD - Sony/SCE's SNDX lib format (cousin of SGXD) [Gravity Rush, Freedom Wars, Soul Sacrifice PSV] */
VGMSTREAM * init_vgmstream_sxd(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE *streamHeader = NULL, *streamExternal = NULL, *streamData = NULL, *streamHead = NULL, *streamBody = NULL;
    off_t start_offset, chunk_offset, first_offset = 0x60, name_offset = 0;
    size_t chunk_size, stream_size = 0;

    int is_dual, is_external;
    int loop_flag, channels, codec, flags;
    int sample_rate, num_samples, loop_start_sample, loop_end_sample;
    uint32_t at9_config_data = 0;
    int total_subsongs, target_subsong = streamFile->stream_index;


    /* checks */
    /* .sxd: header+data (SXDF)
     * .sxd1: header (SXDF) + .sxd2 = data (SXDS)
     * .sxd3: sxd1 + sxd2 pasted together (found in some PS4 games, ex. Fate Extella)*/
    if (!check_extensions(streamFile,"sxd,sxd2,sxd3"))
        goto fail;

    /* setup head/body variations */
    if (check_extensions(streamFile,"sxd2")) {
        /* sxd1+sxd2: open sxd1 as header */

        streamHead = open_streamfile_by_ext(streamFile, "sxd1");
        if (!streamHead) goto fail;

        streamHeader = streamHead;
        streamExternal = streamFile;
        is_dual = 1;
    }
    else if (check_extensions(streamFile,"sxd3")) {
        /* sxd3: make subfiles for head and body to simplify parsing */
        off_t  sxd1_offset  = 0x00;
        size_t sxd1_size    = read_32bitLE(0x08, streamFile);
        off_t  sxd2_offset  = sxd1_size;
        size_t sxd2_size    = get_streamfile_size(streamFile) - sxd1_size;

        streamHead = setup_subfile_streamfile(streamFile, sxd1_offset, sxd1_size, "sxd1");
        if (!streamHead) goto fail;

        streamBody = setup_subfile_streamfile(streamFile, sxd2_offset, sxd2_size, "sxd2");
        if (!streamBody) goto fail;

        streamHeader = streamHead;
        streamExternal = streamBody;
        is_dual = 1;
    }
    else {
        /* sxd: use the current file as header */
        streamHeader = streamFile;
        streamExternal = NULL;
        is_dual = 0;
    }

    if (streamHeader && read_32bitBE(0x00,streamHeader) != 0x53584446) /* "SXDF" */
        goto fail;
    if (streamExternal && read_32bitBE(0x00,streamExternal) != 0x53584453) /* "SXDS" */
        goto fail;


    /* typical chunks: NAME, WAVE and many control chunks (SXDs don't need to contain any sound data) */
    if (!find_chunk_le(streamHeader, 0x57415645,first_offset,0, &chunk_offset,&chunk_size)) /* "WAVE" */
        goto fail;

    /* check multi-streams (usually only in SFX containers) */
    total_subsongs = read_32bitLE(chunk_offset+0x04,streamHeader);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;
    // todo rarely a WAVE subsong may point to a repeated data offset, with different tags only
    

    /* read stream header */
    {
        off_t table_offset, header_offset, stream_offset;

        /* get target offset using table of relative offsets within WAVE */
        table_offset  = chunk_offset + 0x08 + 4*(target_subsong-1);
        header_offset = table_offset + read_32bitLE(table_offset,streamHeader);

        flags       = read_32bitLE(header_offset+0x00,streamHeader);
        codec       = read_8bit   (header_offset+0x04,streamHeader);
        channels    = read_8bit   (header_offset+0x05,streamHeader);
        /* 0x06(2): unknown, rarely 0xFF */
        sample_rate = read_32bitLE(header_offset+0x08,streamHeader);
        /* 0x0c(4): unknown size? (0x4000/0x3999/0x3333/etc, not related to extra data) */
        /* 0x10(4): ? + volume? + pan? (can be 0 for music) */
        num_samples       = read_32bitLE(header_offset+0x14,streamHeader);
        loop_start_sample = read_32bitLE(header_offset+0x18,streamHeader);
        loop_end_sample   = read_32bitLE(header_offset+0x1c,streamHeader);
        stream_size       = read_32bitLE(header_offset+0x20,streamHeader);
        stream_offset     = read_32bitLE(header_offset+0x24,streamHeader);

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
            off_t extra_offset = header_offset+0x28;
            off_t max_offset = chunk_offset + chunk_size;

            if (!(flags & 1))
                goto fail;

            while (extra_offset < max_offset) {
                uint32_t tag = read_32bitBE(extra_offset, streamHeader);
                if (tag == 0x0A010000 || tag == 0x0A010600) {
                    at9_config_data = read_32bitLE(extra_offset+0x04,streamHeader); /* yes, LE */
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
            start_offset = header_offset+0x24 + stream_offset; /* from current entry offset if internal */
        }
    }

    /* get stream name (NAME is tied to REQD/cues, and SFX cues repeat WAVEs, but should work ok for streams) */
    if (is_dual && find_chunk_le(streamHeader, 0x4E414D45,first_offset,0, &chunk_offset,NULL)) { /* "NAME" */
        /* table: relative offset (32b) + hash? (32b) + cue index (32b) */
        int i;
        int num_entries = read_16bitLE(chunk_offset+0x04,streamHeader); /* can be bigger than streams */
        for (i = 0; i < num_entries; i++) {
            uint32_t index = (uint32_t)read_32bitLE(chunk_offset+0x08 + 0x08 + i*0x0c,streamHeader);
            if (index+1 == target_subsong) {
                name_offset = chunk_offset+0x08 + 0x00 + i*0x0c + read_32bitLE(chunk_offset+0x08 + 0x00 + i*0x0c,streamHeader);
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
        streamData = streamExternal;
    } else {
        streamData = streamHeader;
    }

    if (start_offset > get_streamfile_size(streamData)) {
        VGM_LOG("SXD: wrong location?\n");
        goto fail;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample = loop_end_sample;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;
    vgmstream->meta_type = meta_SXD;
    if (name_offset)
        read_string(vgmstream->stream_name,STREAM_NAME_SIZE, name_offset,streamHeader);

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
    if (!vgmstream_open_stream(vgmstream,streamData,start_offset))
        goto fail;

    if (streamHead) close_streamfile(streamHead);
    if (streamBody) close_streamfile(streamBody);
    return vgmstream;

fail:
    if (streamHead) close_streamfile(streamHead);
    if (streamBody) close_streamfile(streamBody);
    close_vgmstream(vgmstream);
    return NULL;
}
