#include "meta.h"
#include "../coding/coding.h"

/* BNK - Sony's Scream Tool bank format [Puyo Puyo Tetris (PS4), NekoBuro: Cats Block (Vita)] */
VGMSTREAM * init_vgmstream_bnk_sony(STREAMFILE *streamFile) {
#if 1
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, stream_offset, name_offset = 0;
    size_t stream_size;
    off_t sblk_offset, data_offset;
    int channel_count = 0, loop_flag, sample_rate, codec;
    int version;
    uint32_t atrac9_info = 0;
    int loop_start = 0, loop_length = 0;
    int total_subsongs, target_subsong = streamFile->stream_index;


    /* checks */
    if (!check_extensions(streamFile, "bnk"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x03000000)
        goto fail;
    if (read_32bitBE(0x04,streamFile) != 0x02000000)
        goto fail;
    sblk_offset = read_32bitLE(0x08,streamFile);
    /* 0x0c: sblk size */
    data_offset = read_32bitLE(0x10,streamFile);
    /* 0x14: data size */

    /* SE banks, also used for music. Most table fields seems reserved/defaults and
     * don't change much between subsongs or files, so they aren't described in detail */


    /* SBlk part: parse header */
    if (read_32bitBE(sblk_offset+0x00,streamFile) != 0x53426C6B) /* "SBlk" */
        goto fail;
    version = read_32bitLE(sblk_offset+0x04,streamFile);
    /* 0x08: possibly when version=0x0d, 0x03=Vita, 0x06=PS4 */
    //;VGM_LOG("BNK: sblk_offset=%lx, data_offset=%lx, version %x\n", sblk_offset, data_offset, version);

    {
        int i;
        off_t table1_offset, table2_offset, table3_offset, table4_offset;
        size_t section_entries, material_entries, stream_entries;
        size_t table1_entry_size;
        off_t table1_suboffset, table2_suboffset, table3_suboffset;
        off_t table2_entry_offset = 0, table3_entry_offset = 0;
        int table4_entry_id = -1;
        off_t table4_entries_offset, table4_names_offset;


        switch(version) {
            case 0x09: /* Puyo Puyo Tetris (PS4) */
                section_entries  = (uint16_t)read_16bitLE(sblk_offset+0x16,streamFile); /* entry size: ~0x0c */
                material_entries = (uint16_t)read_16bitLE(sblk_offset+0x18,streamFile); /* entry size: ~0x08 */
                stream_entries   = (uint16_t)read_16bitLE(sblk_offset+0x1a,streamFile); /* entry size: ~0x60 */
                table1_offset    = sblk_offset + read_32bitLE(sblk_offset+0x1c,streamFile);
                table2_offset    = sblk_offset + read_32bitLE(sblk_offset+0x20,streamFile);
                /* 0x24: null? */
                /* 0x28: offset to end? */
                /* 0x2c: offset to table3? */
                /* 0x30: null? */
                table3_offset    = sblk_offset + read_32bitLE(sblk_offset+0x34,streamFile);
                table4_offset    = sblk_offset + read_32bitLE(sblk_offset+0x38,streamFile);

                table1_entry_size = 0x0c;
                table1_suboffset = 0x08;
                table2_suboffset = 0x00;
                table3_suboffset = 0x10;
                break;

            case 0x0d: /* Polara (Vita), Crypt of the Necrodancer (Vita) */
                table1_offset    = sblk_offset + read_32bitLE(sblk_offset+0x18,streamFile);
                table2_offset    = sblk_offset + read_32bitLE(sblk_offset+0x1c,streamFile);
                /* 0x20: null? */
                /* 0x24: offset to end? */
                /* 0x28: offset to table4? */
                table3_offset    = sblk_offset + read_32bitLE(sblk_offset+0x2c,streamFile);
                table4_offset    = sblk_offset + read_32bitLE(sblk_offset+0x30,streamFile);
                /* 0x34: null? */
                section_entries  = (uint16_t)read_16bitLE(sblk_offset+0x38,streamFile); /* entry size: ~0x24 */
                material_entries = (uint16_t)read_16bitLE(sblk_offset+0x3a,streamFile); /* entry size: ~0x08 */
                stream_entries   = (uint16_t)read_16bitLE(sblk_offset+0x3c,streamFile); /* entry size: ~0x90 + variable (sometimes) */

                table1_entry_size = 0x24;
                table1_suboffset = 0x0c;
                table2_suboffset = 0x00;
                table3_suboffset = 0x44;
                break;

            default:
                VGM_LOG("BNK: unknown version %x\n", version);
                goto fail;
        }

        //;VGM_LOG("BNK: table offsets=t1=%lx, %lx, %lx, %lx\n", table1_offset,table2_offset,table3_offset,table4_offset);
        //;VGM_LOG("BNK: table entries=%i, %i, %i\n", section_entries,material_entries,stream_entries);


        /* table defs:
         * - table1: sections, point to some materials (may be less than streams/materials)
         * - table2: materials, point to all sounds or others subtypes (may be more than sounds)
         * - table3: sounds, point to streams (multiple sounds can repeat stream)
         * - table4: names define section names (not all sounds may have a name)
         *
         * approximate table parsing
         * - check materials and skip non-sounds to get table3 offsets (since table3 entry size isn't always constant)
         * - get stream offsets
         * - find if one section points to the selected material, and get section name = stream name */

        /* parse materials */
        total_subsongs = 0;
        if (target_subsong == 0) target_subsong = 1;

        for (i = 0; i < material_entries; i++) {
            uint16_t table2_subtype = (uint16_t)read_16bitLE(table2_offset+(i*0x08)+table2_suboffset+0x02,streamFile);
            if (table2_subtype != 0x100)
                continue; /* not sounds */

            total_subsongs++;
            if (total_subsongs == target_subsong) {
                table2_entry_offset = (i*0x08);
                table3_entry_offset = (uint16_t)read_16bitLE(table2_offset+(i*0x08)+table2_suboffset+0x00,streamFile);
                /* continue to count all subsongs*/
            }
        }

        //;VGM_LOG("BNK: subsongs %i, table2_entry=%lx, table3_entry=%lx\n", total_subsongs,table2_entry_offset,table3_entry_offset);

        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;
        /* this means some subsongs repeat streams, that can happen in some sfx banks, whatevs */
        if (total_subsongs != stream_entries) {
            //;VGM_LOG("BNK: subsongs %i vs table3 %i don't match\n", total_subsongs, stream_entries);
            /* find_dupes...? */
        }


        /* parse sounds */
        stream_offset = read_32bitLE(table3_offset+table3_entry_offset+table3_suboffset+0x00,streamFile);
        stream_size   = read_32bitLE(table3_offset+table3_entry_offset+table3_suboffset+0x04,streamFile);


        /* find if this sound has an assigned name in table1 */
        for (i = 0; i < section_entries; i++) {
            off_t entry_offset = (uint16_t)read_16bitLE(table1_offset+(i*table1_entry_size)+table1_suboffset+0x00,streamFile);

            /* rarely (ex. Polara sfx) one name applies to multiple materials,
             * from current entry_offset to next entry_offset (section offsets should be in order) */
            if (entry_offset <= table2_entry_offset ) {
                table4_entry_id = i;
                //break;
            }
        }

        /* table4: */
        /* 0x00: bank name (optional) */
        /* 0x08: header size */
        /* 0x0c: table4 size */
        /* variable: entries */
        /* variable: names (null terminated) */
        table4_entries_offset = table4_offset + read_32bitLE(table4_offset+0x08, streamFile);
        table4_names_offset = table4_entries_offset + (0x10*section_entries);
        //;VGM_LOG("BNK: t4_entries=%lx, t4_names=%lx\n", table4_entries_offset, table4_names_offset);

        /* get assigned name from table4 names */
        for (i = 0; i < section_entries; i++) {
            int entry_id = read_32bitLE(table4_entries_offset+(i*0x10)+0x0c, streamFile);
            if (entry_id == table4_entry_id) {
                name_offset = table4_names_offset + read_32bitLE(table4_entries_offset+(i*0x10)+0x00, streamFile);
                break;
            }
        }

        //;VGM_LOG("BNK: stream_offset=%lx, stream_size=%x, name_offset=%lx\n", stream_offset, stream_size, name_offset);
    }


    /* data part: parse extradata before the codec, very annoying */
    {
        size_t extradata_size = 0;
        start_offset = data_offset + stream_offset;

        switch(version) {
            case 0x09:
                codec = read_16bitLE(start_offset+0x00,streamFile);
                extradata_size = 0x08 + read_32bitLE(start_offset+0x04,streamFile); /* 0x14 for AT9 */

                switch(codec) {
#ifdef VGM_USE_ATRAC9
                    case 0x02:
                    case 0x05:
                        if (read_32bitLE(start_offset+0x08,streamFile) + 0x08 != extradata_size) /* repeat? */
                            goto fail;
                        atrac9_info = (uint32_t)read_32bitBE(start_offset+0x0c,streamFile);
                        /* 0x10: null? */
                        loop_length = read_32bitLE(start_offset+0x14,streamFile);
                        loop_start  = read_32bitLE(start_offset+0x18,streamFile);

                        /* get from AT9 config just in case, but probably: sr=48000 / codec 0x02=1ch, 0x05=2ch */
                        atrac9_parse_config(atrac9_info, &sample_rate, &channel_count, NULL);
                        break;
#endif
                }
                break;

            case 0x0d:
                codec = read_16bitLE(start_offset+0x00,streamFile);
                if (read_32bitLE(start_offset+0x04,streamFile) != 0x01) /* type? */
                    goto fail;
                extradata_size = 0x10 + read_32bitLE(start_offset+0x08,streamFile); /* 0x80 for AT9, 0x10 for PCM */
                /* 0x0c: null? */


                switch(codec) {
#ifdef VGM_USE_ATRAC9
                    case 0x02:
                    case 0x05:
                        if (read_32bitLE(start_offset+0x10,streamFile) + 0x10 != extradata_size) /* repeat? */
                            goto fail;
                        atrac9_info = (uint32_t)read_32bitBE(start_offset+0x14,streamFile);
                        /* 0x18: null? */
                        /* 0x1c: channels? */
                        /* 0x20: null? */
                        loop_length = read_32bitLE(start_offset+0x24,streamFile);
                        loop_start = read_32bitLE(start_offset+0x28,streamFile);

                        /* get from AT9 config just in case, but probably: sr=48000 / codec 0x02=1ch, 0x05=2ch */
                        atrac9_parse_config(atrac9_info, &sample_rate, &channel_count, NULL);
                        break;
#endif
                    case 0x01:
                    case 0x04:
                        sample_rate = 48000; /* seems ok */
                        /* 0x10: null? */
                        channel_count = read_32bitLE(start_offset+0x14,streamFile);
                        loop_start = read_32bitLE(start_offset+0x18,streamFile);
                        loop_length = read_32bitLE(start_offset+0x1c,streamFile);
                        break;
                }
                break;

            default:
                goto fail;
        }

        start_offset += extradata_size;
        stream_size -= extradata_size;
    }

    loop_flag = (loop_start >= 0) && (loop_length > 0); /* loop_start is -1 if not set */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;

    vgmstream->meta_type = meta_BNK_SONY;

    switch(codec) {
#ifdef VGM_USE_ATRAC9
        case 0x02:   /* ATRAC9 mono? */
        case 0x05: { /* ATRAC9 stereo? */
            atrac9_config cfg = {0};

            cfg.channels = vgmstream->channels;
            cfg.config_data = atrac9_info;
            //cfg.encoder_delay = 0x00; //todo

            vgmstream->codec_data = init_atrac9(&cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_ATRAC9;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = atrac9_bytes_to_samples(stream_size, vgmstream->codec_data);
            vgmstream->loop_start_sample = loop_start;
            vgmstream->loop_end_sample = loop_start + loop_length;
            break;
    }
#endif
        case 0x01: /* PCM16LE mono? (NekoBuro/Polara sfx) */
        case 0x04: /* PCM16LE stereo? (NekoBuro/Polara sfx) */
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;

            vgmstream->num_samples = pcm_bytes_to_samples(stream_size, vgmstream->channels, 16);
            vgmstream->loop_start_sample = loop_start;
            vgmstream->loop_end_sample = loop_start + loop_length;

            break;

        default:
            VGM_LOG("BNK: unknown codec %x\n", codec);
            goto fail;
    }

    if (name_offset)
        read_string(vgmstream->stream_name,STREAM_NAME_SIZE, name_offset,streamFile);


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
#endif
    return NULL;
}
