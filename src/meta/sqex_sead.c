#include "meta.h"
#include "../coding/coding.h"
#include "sqex_sead_streamfile.h"


typedef struct {
    int big_endian;

    int version;
    int is_sab;
    int is_mab;

    int total_subsongs;
    int target_subsong;

    uint16_t wave_id;
    int loop_flag;
    int channel_count;
    int codec;
    int sample_rate;
    int loop_start;
    int loop_end;
    off_t meta_offset;
    off_t extradata_offset;
    size_t extradata_size;
    size_t stream_size;
    size_t special_size;

    off_t descriptor_offset;
    size_t descriptor_size;
    off_t filename_offset;
    size_t filename_size;
    off_t cuename_offset;
    size_t cuename_size;
    off_t modename_offset;
    size_t modename_size;
    off_t instname_offset;
    size_t instname_size;
    off_t sndname_offset;
    size_t sndname_size;

    off_t sections_offset;
    off_t snd_offset;
    off_t trk_offset;
    off_t musc_offset;
    off_t inst_offset;
    off_t mtrl_offset;

    char readable_name[STREAM_NAME_SIZE];

} sead_header;

static int parse_sead(sead_header *sead, STREAMFILE *sf);


/* SABF/MABF - Square Enix's "sead" audio games [Dragon Quest Builders (PS3), Dissidia Opera Omnia (mobile), FF XV (PS4)] */
VGMSTREAM * init_vgmstream_sqex_sead(STREAMFILE * streamFile) {
    VGMSTREAM * vgmstream = NULL;
    sead_header sead = {0};
    off_t start_offset;
    int target_subsong = streamFile->stream_index;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;


    /* checks */
    /* .sab: sound/bgm
     * .mab: music
     * .sbin: Dissidia Opera Omnia .sab */
    if (!check_extensions(streamFile,"sab,mab,sbin"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) == 0x73616266) { /* "sabf" */
        sead.is_sab = 1;
    } else if (read_32bitBE(0x00,streamFile) == 0x6D616266) { /* "mabf" */
        sead.is_mab = 1;
    } else {
        /* there are other SEAD files with other chunks but similar formats too */
        goto fail;
    }

    sead.big_endian = guess_endianness16bit(0x06,streamFile); /* use some value as no apparent flag */
    if (sead.big_endian) {
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
    } else {
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
    }

    sead.target_subsong = target_subsong;

    if (!parse_sead(&sead, streamFile))
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(sead.channel_count, sead.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = sead.is_sab ? meta_SQEX_SAB : meta_SQEX_MAB;
    vgmstream->sample_rate = sead.sample_rate;
    vgmstream->num_streams = sead.total_subsongs;
    vgmstream->stream_size = sead.stream_size;
    strcpy(vgmstream->stream_name, sead.readable_name);

    switch(sead.codec) {

        case 0x01: { /* PCM [Chrono Trigger sfx (PC)] */
            start_offset = sead.extradata_offset + sead.extradata_size;

            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;

            vgmstream->num_samples = pcm_bytes_to_samples(sead.stream_size, vgmstream->channels, 16);
            vgmstream->loop_start_sample = sead.loop_start;
            vgmstream->loop_end_sample   = sead.loop_end;
            break;
        }

        case 0x02: { /* MSADPCM [Dragon Quest Builders (Vita) sfx] */
            start_offset = sead.extradata_offset + sead.extradata_size;

            /* 0x00 (2): null?, 0x02(2): entry size? */
            vgmstream->coding_type = coding_MSADPCM;
            vgmstream->layout_type = layout_none;
            vgmstream->frame_size = read_16bit(sead.extradata_offset+0x04,streamFile);

            /* much like AKBs, there are slightly different loop values here, probably more accurate
             * (if no loop, loop_end doubles as num_samples) */
            vgmstream->num_samples = msadpcm_bytes_to_samples(sead.stream_size, vgmstream->frame_size, vgmstream->channels);
            vgmstream->loop_start_sample = read_32bit(sead.extradata_offset+0x08, streamFile); //loop_start
            vgmstream->loop_end_sample   = read_32bit(sead.extradata_offset+0x0c, streamFile); //loop_end
            break;
        }

#ifdef VGM_USE_VORBIS
        case 0x03: { /* OGG [Final Fantasy XV Benchmark sfx (PC)] */
            VGMSTREAM *ogg_vgmstream = NULL;
            ogg_vorbis_meta_info_t ovmi = {0};
            off_t subfile_offset = sead.extradata_offset + sead.extradata_size;

            ovmi.meta_type = vgmstream->meta_type;
            ovmi.total_subsongs = sead.total_subsongs;
            ovmi.stream_size = sead.stream_size;
            /* post header has some kind of repeated values, config/table? */

            ogg_vgmstream = init_vgmstream_ogg_vorbis_callbacks(streamFile, NULL, subfile_offset, &ovmi);
            if (ogg_vgmstream) {
                ogg_vgmstream->num_streams = vgmstream->num_streams;
                ogg_vgmstream->stream_size = vgmstream->stream_size;
                strcpy(ogg_vgmstream->stream_name, vgmstream->stream_name);

                close_vgmstream(vgmstream);
                return ogg_vgmstream;
            }
            else {
                goto fail;
            }

            break;
        }
#endif

#ifdef VGM_USE_ATRAC9
        case 0x04: { /* ATRAC9 [Dragon Quest Builders (Vita), Final Fantaxy XV (PS4)] */
            atrac9_config cfg = {0};

            start_offset = sead.extradata_offset + sead.extradata_size;
            /* post header has various typical ATRAC9 values */
            cfg.channels = vgmstream->channels;
            cfg.config_data = read_32bit(sead.extradata_offset+0x0c,streamFile);
            cfg.encoder_delay = read_32bit(sead.extradata_offset+0x18,streamFile);

            vgmstream->codec_data = init_atrac9(&cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_ATRAC9;
            vgmstream->layout_type = layout_none;

            vgmstream->sample_rate = read_32bit(sead.extradata_offset+0x1c,streamFile); /* SAB's sample rate can be different but it's ignored */
            vgmstream->num_samples = read_32bit(sead.extradata_offset+0x10,streamFile); /* loop values above are also weird and ignored */
            vgmstream->loop_start_sample = read_32bit(sead.extradata_offset+0x20, streamFile) - (sead.loop_flag ? cfg.encoder_delay : 0); //loop_start
            vgmstream->loop_end_sample   = read_32bit(sead.extradata_offset+0x24, streamFile) - (sead.loop_flag ? cfg.encoder_delay : 0); //loop_end
            break;
        }
#endif

#ifdef VGM_USE_MPEG
        case 0x06: { /* MSF subfile (MPEG mode) [Dragon Quest Builders (PS3)] */
            mpeg_codec_data *mpeg_data = NULL;
            mpeg_custom_config cfg = {0};

            start_offset = sead.extradata_offset + sead.extradata_size;
            /* post header is a proper MSF, but sample rate/loops are ignored in favor of SAB's */

            mpeg_data = init_mpeg_custom(streamFile, start_offset, &vgmstream->coding_type, vgmstream->channels, MPEG_STANDARD, &cfg);
            if (!mpeg_data) goto fail;
            vgmstream->codec_data = mpeg_data;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = mpeg_bytes_to_samples(sead.stream_size, mpeg_data);
            vgmstream->loop_start_sample = sead.loop_start;
            vgmstream->loop_end_sample = sead.loop_end;
            break;
        }
#endif

        case 0x07: { /* HCA subfile [Dissidia Opera Omnia (Mobile), Final Fantaxy XV (PS4)] */
            //todo there is no easy way to use the HCA decoder; try subfile hack for now
            VGMSTREAM *temp_vgmstream = NULL;
            STREAMFILE *temp_streamFile = NULL;
            off_t subfile_offset = sead.extradata_offset + 0x10;
            size_t subfile_size = sead.stream_size + sead.extradata_size - 0x10;

            /* post header: values from the HCA header, in file endianness + HCA header */
            size_t key_start = sead.special_size & 0xff;
            size_t header_size = read_16bit(sead.extradata_offset+0x02, streamFile);
            int encryption = read_16bit(sead.extradata_offset+0x0c, streamFile); //maybe 8bit?
            /* encryption type 0x01 found in Final Fantasy XII TZA (PS4/PC) */

            temp_streamFile = setup_sqex_sead_streamfile(streamFile, subfile_offset, subfile_size, encryption, header_size, key_start);
            if (!temp_streamFile) goto fail;

            temp_vgmstream = init_vgmstream_hca(temp_streamFile);
            if (temp_vgmstream) {
                /* loops can be slightly different (~1000 samples) but probably HCA's are more accurate */
                temp_vgmstream->num_streams = vgmstream->num_streams;
                temp_vgmstream->stream_size = vgmstream->stream_size;
                temp_vgmstream->meta_type = vgmstream->meta_type;
                strcpy(temp_vgmstream->stream_name, vgmstream->stream_name);

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
            VGM_LOG("SQEX SEAD: unknown codec %x\n", sead.codec);
            goto fail;
    }

    strcpy(vgmstream->stream_name, sead.readable_name);

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


static void build_readable_name(char * buf, size_t buf_size, sead_header *sead, STREAMFILE *sf) {

    if (sead->is_sab) {
        char descriptor[255], name[255];

        if (sead->descriptor_size > 255 || sead->sndname_size > 255) goto fail;

        read_string(descriptor,sead->descriptor_size+1,sead->descriptor_offset, sf);
        read_string(name,sead->sndname_size+1,sead->sndname_offset, sf);

        snprintf(buf,buf_size, "%s/%s", descriptor, name);
    }
    else {
        char descriptor[255], name[255], mode[255];

        if (sead->descriptor_size > 255 || sead->filename_size > 255 || sead->cuename_size > 255 || sead->modename_size > 255) goto fail;

        read_string(descriptor,sead->descriptor_size+1,sead->descriptor_offset, sf);
      //read_string(filename,sead->filename_size+1,sead->filename_offset, sf); /* same as filename, not too interesting */
        if (sead->cuename_offset)
            read_string(name,sead->cuename_size+1,sead->cuename_offset, sf);
        else if (sead->instname_offset)
            read_string(name,sead->instname_size+1,sead->instname_offset, sf);
        else
            strcpy(name, "?");
        read_string(mode,sead->modename_size+1,sead->modename_offset, sf);

        /* default mode in most files, not very interesting */
        if (strcmp(mode, "Mode") == 0 || strcmp(mode, "Mode0") == 0)
            snprintf(buf,buf_size, "%s/%s", descriptor, name);
        else
            snprintf(buf,buf_size, "%s/%s/%s", descriptor, name, mode);
    }

    return;
fail:
    VGM_LOG("SEAD: bad name found\n");
}

static void parse_sead_mab_name(sead_header *sead, STREAMFILE *sf) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = sead->big_endian ? read_32bitBE : read_32bitLE;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = sead->big_endian ? read_16bitBE : read_16bitLE;
    int i, entries, cue, mode, cue_count, mode_count;
    off_t entry_offset, cue_offset, mode_offset, name_offset, table_offset;
    size_t name_size;
    //int wave, wave_count;  off_t wave_offset, subtable_offset;  uint16_t wave_id;
    int name = 0;


    /* find which name corresponds to our song (mabf can have N subsongs
     * and X cues + Y modes and also Z instruments, one of which should reference it) */
    //todo exact name matching unknown, assumes subsong N = name N

    /* parse "musc" (music cue?) */
    entries = read_16bit(sead->musc_offset + 0x04, sf);
    for (i = 0; i < entries; i++) {
        entry_offset = sead->musc_offset + read_32bit(sead->musc_offset + 0x10 + i*0x04, sf);

        /* 0x00: config? */
        sead->filename_offset = entry_offset + read_16bit(entry_offset + 0x02, sf);
        cue_count  = read_8bit(entry_offset + 0x04, sf);
        mode_count = read_8bit(entry_offset + 0x05, sf);
        /* 0x06: some low number? */
        /* 0x07: always 0x80? (apparently not an offset/size) */
        /* 0x08: id? */
        /* 0x0a: 0? */
        /* 0x44: sample rate */
        /* others: unknown/null */
        sead->filename_size   = read_8bit(entry_offset + 0x48, sf);

        /* table points to all cue offsets first then all modes offsets */
        table_offset = align_size_to_block(sead->filename_offset + sead->filename_size + 0x01, 0x10);

        /* cue name (ex. "bgm_007_take2" / "bgm_007s" / etc subsongs) */
        for (cue = 0; cue < cue_count; cue++) {
            cue_offset = sead->musc_offset + 0x20 + read_32bit(table_offset + cue*0x04, sf);

            /* 0x00: id? */
            name_offset = cue_offset + read_16bit(cue_offset + 0x02, sf);
            name_size = read_8bit(cue_offset + 0x04, sf);
          //wave_count = read_8bit(cue_offset + 0x05, sf);
            /* 0x06: ? */
            /* 0x0c: num samples */
            /* 0x10: loop start */
            /* 0x14: loop end */
            /* 0x18: flag? */
            /* others: ? */

            name++;
            if (name == sead->target_subsong || cue_count == 1) {
                sead->cuename_offset = name_offset;
                sead->cuename_size = name_size;
                break;
            }

#if 0       //this works for some games like KH3 but not others like FFXII
            /* subtable: first N wave refs + ? unk refs  (rarely more than 1 each) */
            subtable_offset = align_size_to_block(name_offset + name_size + 1, 0x10);

            for (wave = 0; wave < wave_count; wave++) {
                wave_offset = cue_offset + read_32bit(subtable_offset + wave*0x04, sf);

                /* 0x00: config? */
                /* 0x02: entry size */
                wave_id = read_16bit(wave_offset + 0x04, sf);
                /* 0x06: null? */
                /* 0x08: null? */
                /* 0x0c: some id/config? */

                if (wave_id == sead->wave_id) {
                    sead->cuename_offset = name_offset;
                    sead->cuename_size = name_size;
                    break;
                }
            }

            if (sead->cuename_offset)
                break;
#endif
        }

        /* mode name (ex. almost always "Mode" and only 1 entry, rarely "Water" / "Restaurant" / etc)
         * no idea how modes are referenced (perhaps manually with in-game events)
         * so just a quick hack, only found multiple in FFXV's bgm_gardina */
        if (mode_count == sead->total_subsongs)
            mode = sead->target_subsong - 1;
        else
            mode = 0;

        { //for (mode = 0; mode < mode_count; mode++) {
            mode_offset = sead->musc_offset + 0x20 + read_32bit(table_offset + cue_count*0x04 + mode*0x04, sf);

            /* 0x00: id? */
            name_offset = mode_offset + read_16bit(mode_offset + 0x02, sf);
            /* 0x04: mode id */
            name_size = read_8bit(mode_offset + 0x06, sf);
            /* 0x08: offset? */
            /* others: floats and stuff */

            sead->modename_offset = name_offset;
            sead->modename_size = name_size;
        }
    }


    /* parse "inst" (instruments) */
    entries = read_16bit(sead->inst_offset + 0x04, sf);
    for (i = 0; i < entries; i++) {
        entry_offset = sead->inst_offset + read_32bit(sead->inst_offset + 0x10 + i*0x04, sf);

        /* 0x00: id? */
        /* 0x02: base size? */
        /* 0x05: count? */
      //wave_count = read_8bit(entry_offset + 0x06, sf);
        /* 0x0c: num samples */
        /* 0x10: loop start */
        /* 0x14: loop end */
        /* 0x18: flag? */
        /* others: ? */

        /* no apparent fields and inst is very rare (ex. KH3 tut) */
        name_offset = entry_offset + 0x30;
        name_size = 0x0F;

        name++;
        if (name == sead->target_subsong) {
            sead->instname_offset = name_offset;
            sead->instname_size = name_size;
            break;
        }


#if 0   //not actually tested
        if (wave_count != 1) break; /* ? */

        /* subtable: N wave refs? */
        subtable_offset = align_size_to_block(name_offset + name_size + 1, 0x10);

        for (wave = 0; wave < wave_count; wave++) {
            wave_offset = subtable_offset + read_32bit(subtable_offset + wave*0x04, sf);

            /* 0x00: config? */
            /* 0x02: entry size? */
            wave_id = read_16bit(wave_offset + 0x04, sf);
            /* 0x06: ? */
            /* 0x08: id/crc? */
            /* 0x0c: ? */
            /* 0x10: sample rate */
            /* others: null? */

            if (wave_id == sead->wave_id) {
                sead->instname_offset = name_offset;
                sead->instname_size = name_size;
                break;
            }
        }

        if (sead->instname_offset)
            break;
#endif
    }
}

static void parse_sead_sab_name(sead_header *sead, STREAMFILE *sf) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = sead->big_endian ? read_32bitBE : read_32bitLE;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = sead->big_endian ? read_16bitBE : read_16bitLE;
    int i, snd_entries, trk_entries, snd_id, wave_id, snd_found = 0;
    size_t size;
    off_t entry_offset;


    //todo looks mostly correct for many subsongs but in rare cases wave_ids aren't referenced
    // or maybe id needs another jump (seq?) (ex. DQB se_break_soil, FFXV aircraftzeroone, FFXV 03bt100031pc00)

    snd_entries = read_16bit(sead->snd_offset + 0x04, sf);
    trk_entries = read_16bit(sead->trk_offset + 0x04, sf);

    /* parse "trk" (track info) */
    for (i = 0; i < trk_entries; i++) {
        entry_offset = sead->trk_offset + read_32bit(sead->trk_offset + 0x10 + i*0x04, sf);

        /* 0x00: type? */
        /* 0x01: subtype? */
        size = read_16bit(entry_offset + 0x02, sf); /* bigger if 'type=03' */
        /* 0x04: trk id? */
        /* 0x04: some id? */

        if (size > 0x10) {
            snd_id = read_8bit(entry_offset + 0x10, sf);
            wave_id = read_16bit(entry_offset + 0x11, sf);
        }
        else {
            snd_id = read_16bit(entry_offset + 0x08, sf);
            wave_id = read_16bit(entry_offset + 0x0a, sf);
        }


        if (wave_id == sead->wave_id) {
            snd_found = 1;
            break;
        }
    }

    if (snd_found && snd_id >= snd_entries) {
        VGM_LOG("SEAD: bad snd_id found\n");
        snd_found = 0;
    }

    if (!snd_found) {
        if (sead->total_subsongs == 1 || snd_entries == 1) {
            snd_id = 0; /* meh */
            VGM_LOG("SEAD: snd_id not found, using first\n");
        } else {
            VGM_LOG("SEAD: snd_id not found, subsongs=%i, snd=%i, trk=%i\n", sead->total_subsongs, snd_entries, trk_entries);
            return;
        }
    }

    /* parse "snd " (sound info) */
    {
        off_t entry_offset = sead->snd_offset + read_32bit(sead->snd_offset + 0x10 + snd_id*0x04, sf);

        /* 0x00: config? */
        sead->sndname_offset = entry_offset + read_16bit(entry_offset + 0x02, sf);
        /* 0x04: count of ? */
        /* 0x05: count of ? (0 if no sound exist in file) */
        /* 0x06: some low number? */
        /* 0x07: always 0x80? (apparently not an offset/size) */
        /* 0x08: snd id */
        /* 0x0a: 0? */
        /* 0x0c: 1.0? */
        /* 0x1a: header size? */
        /* 0x1c: 30.0? * */
        /* 0x24: crc/id? */
        /* 0x46: header size? */
        /* 0x4c: header size? */

        if (sead->version == 1) {
            sead->sndname_offset -= 0x10;
            sead->sndname_size = read_8bit(entry_offset + 0x08, sf);
        }
        else {
            sead->sndname_size = read_8bit(entry_offset + 0x23, sf);
        }

        /* 0x24: unique id? (referenced in "seq" section?) */
        /* others: probably sound config like pan/volume (has floats and stuff) */
    }
}

static int parse_sead(sead_header *sead, STREAMFILE *sf) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = sead->big_endian ? read_32bitBE : read_32bitLE;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = sead->big_endian ? read_16bitBE : read_16bitLE;

    /** base header **/
    sead->version = read_8bit(0x04, sf); /* usually 0x02, rarely 0x01 (ex FF XV early songs) */
    /* 0x05(1): 0/1? */
    /* 0x06(2): ? (usually 0x10, rarely 0x20) */
    /* 0x08(1): 3/4? */
    sead->descriptor_size = read_8bit(0x09, sf);
    /*  0x0a(2): ? */
    if (read_32bit(0x0c, sf) != get_streamfile_size(sf))
        goto fail;

    if (sead->descriptor_size == 0) /* not set when version == 1 */
        sead->descriptor_size = 0x0f;
    sead->descriptor_offset = 0x10; /* file descriptor ("BGM", "Music2", "SE", etc, long names are ok) */
    sead->sections_offset = sead->descriptor_offset + (sead->descriptor_size + 0x01); /* string null matters for padding */
    sead->sections_offset = align_size_to_block(sead->sections_offset, 0x10);


    /** offsets to sections **/
    if (sead->is_sab) {
        if (read_32bitBE(sead->sections_offset + 0x00, sf) != 0x736E6420) goto fail; /* "snd " (sonds) */
        if (read_32bitBE(sead->sections_offset + 0x10, sf) != 0x73657120) goto fail; /* "seq " (unknown) */
        if (read_32bitBE(sead->sections_offset + 0x20, sf) != 0x74726B20) goto fail; /* "trk " (unknown) */
        if (read_32bitBE(sead->sections_offset + 0x30, sf) != 0x6D74726C) goto fail; /* "mtrl" (headers/streams) */
        sead->snd_offset  = read_32bit(sead->sections_offset + 0x08, sf);
      //sead->seq_offset  = read_32bit(sead->sections_offset + 0x18, sf);
        sead->trk_offset  = read_32bit(sead->sections_offset + 0x28, sf);
        sead->mtrl_offset = read_32bit(sead->sections_offset + 0x38, sf);
    }
    else if (sead->is_mab) {
        if (read_32bitBE(sead->sections_offset + 0x00, sf) != 0x6D757363) goto fail; /* "musc" (cues) */
        if (read_32bitBE(sead->sections_offset + 0x10, sf) != 0x696E7374) goto fail; /* "inst" (instruments) */
        if (read_32bitBE(sead->sections_offset + 0x20, sf) != 0x6D74726C) goto fail; /* "mtrl" (headers/streams) */
        sead->musc_offset = read_32bit(sead->sections_offset + 0x08, sf);
        sead->inst_offset = read_32bit(sead->sections_offset + 0x18, sf);
        sead->mtrl_offset = read_32bit(sead->sections_offset + 0x28, sf);
    }
    else {
        goto fail;
    }


    /* section format at offset:
     * 0x00(2): 0/1?
     * 0x02(2): header size? (always 0x10)
     * 0x04(2): entries
     * 0x06(+): padded to 0x10
     * 0x10 + 0x04*entry: offset to entry from table start (also padded to 0x10 at the end) */


    /* find meta_offset in "mtrl" and total subsongs */
    {
        int i, entries;

        entries = read_16bit(sead->mtrl_offset+0x04, sf);

        if (sead->target_subsong == 0) sead->target_subsong = 1;
        sead->total_subsongs = 0;
        sead->meta_offset = 0;

        /* manually find subsongs as entries can be dummy (ex. sfx banks in Dissidia Opera Omnia) */
        for (i = 0; i < entries; i++) {
            off_t entry_offset = sead->mtrl_offset + read_32bit(sead->mtrl_offset + 0x10 + i*0x04, sf);

            if (read_8bit(entry_offset + 0x05, sf) == 0) {
                continue; /* codec 0 when dummy (see stream header) */
            }


            sead->total_subsongs++;
            if (!sead->meta_offset && sead->total_subsongs == sead->target_subsong) {
                sead->meta_offset = entry_offset;
            }
        }
        if (sead->meta_offset == 0) goto fail;
        /* SAB can contain 0 entries too */
    }


    /** stream header **/
    /* 0x00(2): 0x00/01? */
    /* 0x02(2): base entry size? (0x20) */
    sead->channel_count   =  read_8bit(sead->meta_offset + 0x04, sf);
    sead->codec           =  read_8bit(sead->meta_offset + 0x05, sf);
    sead->wave_id         = read_16bit(sead->meta_offset + 0x06, sf); /* 0..N */
    sead->sample_rate     = read_32bit(sead->meta_offset + 0x08, sf);
    sead->loop_start      = read_32bit(sead->meta_offset + 0x0c, sf); /* in samples but usually ignored */

    sead->loop_end        = read_32bit(sead->meta_offset + 0x10, sf);
    sead->extradata_size  = read_32bit(sead->meta_offset + 0x14, sf); /* including subfile header, can be 0 */
    sead->stream_size     = read_32bit(sead->meta_offset + 0x18, sf); /* not including subfile header */
    sead->special_size    = read_32bit(sead->meta_offset + 0x1c, sf);

    sead->loop_flag       = (sead->loop_end > 0);
    sead->extradata_offset = sead->meta_offset + 0x20;


    /** info section (get stream name) **/
    if (sead->is_sab) {
        parse_sead_sab_name(sead, sf);
    }
    else if (sead->is_mab) {
        parse_sead_mab_name(sead, sf);
    }

    build_readable_name(sead->readable_name, sizeof(sead->readable_name), sead, sf);

    return 1;
fail:
    return 0;
}
