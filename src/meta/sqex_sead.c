#include "meta.h"
#include "../coding/coding.h"


static STREAMFILE* setup_sead_hca_streamfile(STREAMFILE *streamFile, off_t subfile_offset, size_t subfile_size);

/* SABF/MABF - Square Enix's "Sead" audio games [Dragon Quest Builders (PS3), Dissidia Opera Omnia (mobile), FF XV (PS4)] */
VGMSTREAM * init_vgmstream_sqex_sead(STREAMFILE * streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, mtrl_offset, meta_offset, post_meta_offset; //, info_offset, name_offset = 0;
    size_t stream_size, subheader_size; //, name_size = 0;

    int loop_flag = 0, channel_count, codec, sample_rate, loop_start, loop_end;
    int is_sab = 0, is_mab = 0;
    int total_subsongs, target_subsong = streamFile->stream_index;

    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;


    /* check extensions (.sab: sound/bgm, .mab: music, .sbin: Dissidia Opera Omnia .sab) */
    if ( !check_extensions(streamFile,"sab,mab,sbin"))
        goto fail;


    /** main header **/
    if (read_32bitBE(0x00,streamFile) == 0x73616266) { /* "sabf" */
        is_sab = 1;
    } else if (read_32bitBE(0x00,streamFile) == 0x6D616266) { /* "mabf" */
        is_mab = 1;
    } else {
        goto fail;
    }

    //if (read_8bit(0x04,streamFile) != 0x02)  /* version? */
    //    goto fail;
    /* 0x04(1): version? (usually 0x02, rarely 0x01, ex FF XV title) */
    /* 0x05(1): 0x00/01? */
    /* 0x06(2): chunk size? (usually 0x10, rarely 0x20) */
    if (read_16bitBE(0x06,streamFile) < 0x100) { /* use size as no apparent flag */
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
    } else {
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
    }
    /* 0x08(1): ?, 0x09(1): ?, 0x0a(2): ?  */
    if (read_32bit(0x0c,streamFile) != get_streamfile_size(streamFile))
        goto fail;
    /* 0x10(10): file descriptor ("BGM", "Music", "SE", etc) */


    /** offset tables **/
    if (is_sab) {
        if (read_32bitBE(0x20,streamFile) != 0x736E6420) goto fail; /* "snd " (info) */
        if (read_32bitBE(0x30,streamFile) != 0x73657120) goto fail; /* "seq " (unknown) */
        if (read_32bitBE(0x40,streamFile) != 0x74726B20) goto fail; /* "trk " (unknown) */
        if (read_32bitBE(0x50,streamFile) != 0x6D74726C) goto fail; /* "mtrl" (headers/streams) */
      //info_offset = read_32bit(0x28,streamFile);
      //seq_offset  = read_32bit(0x38,streamFile);
      //trk_offset  = read_32bit(0x48,streamFile);
        mtrl_offset = read_32bit(0x58,streamFile);
    }
    else if (is_mab) {
        if (read_32bitBE(0x20,streamFile) != 0x6D757363) goto fail; /* "musc" (info) */
        if (read_32bitBE(0x30,streamFile) != 0x696E7374) goto fail; /* "inst" (unknown) */
        if (read_32bitBE(0x40,streamFile) != 0x6D74726C) goto fail; /* "mtrl" (headers/streams) */
      //info_offset = read_32bit(0x28,streamFile);
      //inst_offset = read_32bit(0x38,streamFile);
        mtrl_offset = read_32bit(0x48,streamFile);
    }
    else {
        goto fail;
    }
    /* each section starts with:
     * 0x00(2): 0x00/01?, 0x02: size? (0x10), 0x04(2): entries, 0x06+: padded to 0x10
     * 0x10+0x04*entry: offset from section start, also padded to 0x10 at the end */

    /* find meta_offset in mtrl and total subsongs */
    {
        int i;
        int entries = read_16bit(mtrl_offset+0x04,streamFile);
        off_t entries_offset = mtrl_offset + 0x10;

        if (target_subsong == 0) target_subsong = 1;
        total_subsongs = 0;
        meta_offset = 0;

        /* manually find subsongs as entries can be dummy (ex. sfx banks in Dissidia Opera Omnia) */
        for (i = 0; i < entries; i++) {
            off_t entry_offset = mtrl_offset + read_32bit(entries_offset + i*0x04,streamFile);

            if (read_8bit(entry_offset+0x05,streamFile) == 0)
                continue; /* codec 0 when dummy */

            total_subsongs++;
            if (!meta_offset && total_subsongs == target_subsong)
                meta_offset = entry_offset;
        }
        if (meta_offset == 0) goto fail;
        /* SAB can contain 0 entries too */
    }


    /** stream header **/
    /* 0x00(2): 0x00/01? */
    /* 0x02(2): base entry size? (0x20) */
    channel_count   =  read_8bit(meta_offset+0x04,streamFile);
    codec           =  read_8bit(meta_offset+0x05,streamFile);
  //entry_id        = read_16bit(meta_offset+0x06,streamFile);
    sample_rate     = read_32bit(meta_offset+0x08,streamFile);
    loop_start      = read_32bit(meta_offset+0x0c,streamFile); /* in samples but usually ignored */

    loop_end        = read_32bit(meta_offset+0x10,streamFile);
    subheader_size  = read_32bit(meta_offset+0x14,streamFile); /* including subfile header */
    stream_size     = read_32bit(meta_offset+0x18,streamFile); /* not including subfile header */
    /* 0x1c: null? */

    loop_flag       = (loop_end > 0);
    post_meta_offset = meta_offset + 0x20;


    /** info section (get stream name) **/
    //if (is_sab) { //todo load name based on entry id
        /* "snd ": unknown flags/sizes and name */
        /* 0x08(2): file number within descriptor */
        /* 0x1a(2): base_entry size (-0x10?) */
        //name_size = read_32bit(snd_offset+0x20,streamFile);
        //name_offset = snd_offset+0x70;
        /* 0x24(4): unique id? (referenced in "seq" section) */
    //}
    //else if (is_mab) {
        /* "musc": unknown flags sizes and names, another format */
    //}


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;
    vgmstream->meta_type = is_sab ? meta_SQEX_SAB : meta_SQEX_MAB;

    switch(codec) {

        case 0x02: { /* MSADPCM [Dragon Quest Builders (Vita) sfx] */
            start_offset = post_meta_offset + subheader_size;

            /* 0x00 (2): null?, 0x02(2): entry size? */
            vgmstream->coding_type = coding_MSADPCM;
            vgmstream->layout_type = layout_none;
            vgmstream->interleave_block_size = read_16bit(post_meta_offset+0x04,streamFile);

            /* much like AKBs, there are slightly different loop values here, probably more accurate
             * (if no loop, loop_end doubles as num_samples) */
            vgmstream->num_samples = msadpcm_bytes_to_samples(stream_size, vgmstream->interleave_block_size, vgmstream->channels);
            vgmstream->loop_start_sample = read_32bit(post_meta_offset+0x08, streamFile); //loop_start
            vgmstream->loop_end_sample   = read_32bit(post_meta_offset+0x0c, streamFile); //loop_end
            break;
        }

#ifdef VGM_USE_ATRAC9
        case 0x04: { /* ATRAC9 [Dragon Quest Builders (Vita), Final Fantaxy XV (PS4)] */
            atrac9_config cfg = {0};

            start_offset = post_meta_offset + subheader_size;
            /* post header has various typical ATRAC9 values */
            cfg.channels = vgmstream->channels;
            cfg.config_data = read_32bit(post_meta_offset+0x0c,streamFile);
            cfg.encoder_delay = read_32bit(post_meta_offset+0x18,streamFile);
VGM_LOG("1\n");
            vgmstream->codec_data = init_atrac9(&cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_ATRAC9;
            vgmstream->layout_type = layout_none;
VGM_LOG("2\n");
            vgmstream->sample_rate = read_32bit(post_meta_offset+0x1c,streamFile); /* SAB's sample rate can be different but it's ignored */
            vgmstream->num_samples = read_32bit(post_meta_offset+0x10,streamFile); /* loop values above are also weird and ignored */
            vgmstream->loop_start_sample = read_32bit(post_meta_offset+0x20, streamFile) - (loop_flag ? cfg.encoder_delay : 0); //loop_start
            vgmstream->loop_end_sample   = read_32bit(post_meta_offset+0x24, streamFile) - (loop_flag ? cfg.encoder_delay : 0); //loop_end
            break;
        }
#endif

#ifdef VGM_USE_MPEG
        case 0x06: {  /* MSF subfile (MPEG mode) [Dragon Quest Builders (PS3)] */
            mpeg_codec_data *mpeg_data = NULL;
            mpeg_custom_config cfg = {0};

            start_offset = post_meta_offset + subheader_size;
            /* post header is a proper MSF, but sample rate/loops are ignored in favor of SAB's */

            mpeg_data = init_mpeg_custom(streamFile, start_offset, &vgmstream->coding_type, vgmstream->channels, MPEG_STANDARD, &cfg);
            if (!mpeg_data) goto fail;
            vgmstream->codec_data = mpeg_data;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = mpeg_bytes_to_samples(stream_size, mpeg_data);
            vgmstream->loop_start_sample = loop_start;
            vgmstream->loop_end_sample = loop_end;
            break;
        }
#endif

        case 0x07: { /* HCA subfile [Dissidia Opera Omnia (Mobile), Final Fantaxy XV (PS4)] */
            //todo there is no easy way to use the HCA decoder; try subfile hack for now
            VGMSTREAM *temp_vgmstream = NULL;
            STREAMFILE *temp_streamFile = NULL;
            off_t subfile_offset = post_meta_offset + 0x10;
            size_t subfile_size = stream_size + subheader_size - 0x10;
            /* post header has 0x10 unknown + HCA header */


            temp_streamFile = setup_sead_hca_streamfile(streamFile, subfile_offset, subfile_size);
            if (!temp_streamFile) goto fail;

            temp_vgmstream = init_vgmstream_hca(temp_streamFile);
            if (temp_vgmstream) {
                /* loops can be slightly different (~1000 samples) but probably HCA's are more accurate */
                temp_vgmstream->num_streams = vgmstream->num_streams;
                temp_vgmstream->stream_size = vgmstream->stream_size;
                temp_vgmstream->meta_type = vgmstream->meta_type;

                close_streamfile(temp_streamFile);
                close_vgmstream(vgmstream);
                return temp_vgmstream;
            }
            else {
                close_streamfile(temp_streamFile);
                goto fail;
            }
        }

        case 0x00: /* dummy entry */
        default:
            VGM_LOG("SQEX SEAD: unknown codec %x\n", codec);
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

static STREAMFILE* setup_sead_hca_streamfile(STREAMFILE *streamFile, off_t subfile_offset, size_t subfile_size) {
    STREAMFILE *temp_streamFile = NULL, *new_streamFile = NULL;

    /* setup subfile */
    new_streamFile = open_wrap_streamfile(streamFile);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_clamp_streamfile(temp_streamFile, subfile_offset,subfile_size);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_fakename_streamfile(temp_streamFile, NULL,"hca");
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    return temp_streamFile;

fail:
    close_streamfile(temp_streamFile);
    return NULL;
}
