#include "meta.h"
#include "../coding/coding.h"

typedef enum { PSX, PCM16, ATRAC9, HEVAG } bnk_codec;

/* .BNK - Sony's Scream Tool bank format [Puyo Puyo Tetris (PS4), NekoBuro: Cats Block (Vita)] */
VGMSTREAM* init_vgmstream_bnk_sony(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, stream_offset, name_offset = 0;
    size_t stream_size, interleave = 0;
    off_t sblk_offset, data_offset;
    size_t data_size;
    int channel_count = 0, loop_flag, sample_rate, parts, sblk_version, big_endian;
    int loop_start = 0, loop_end = 0;
    uint32_t pitch, flags;
    uint32_t atrac9_info = 0;

    int total_subsongs, target_subsong = sf->stream_index;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;
    float (*read_f32)(off_t,STREAMFILE*) = NULL;
    bnk_codec codec;


    /* checks */
    if (!check_extensions(sf, "bnk"))
        goto fail;

    /* bnk version */
    if (read_32bitBE(0x00,sf) == 0x00000003) { /* PS3 */
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
        read_f32 = read_f32be;
        big_endian = 1;
    }
    else if (read_32bitBE(0x00,sf) == 0x03000000) { /* PS2/Vita/PS4 */
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
        read_f32 = read_f32le;
        big_endian = 0;
    }
    else {
        goto fail;
    }

    parts = read_32bit(0x04,sf);
    if (parts < 2 || parts > 3) goto fail;

    sblk_offset = read_32bit(0x08,sf);
    /* 0x0c: sklb size */
    data_offset = read_32bit(0x10,sf);
    data_size = read_32bit(0x14,sf);
    /* when sblk_offset >= 0x20: */
    /* 0x18: ZLSD small footer, rare [Yakuza 6's Puyo Puyo (PS4)] */
    /* 0x1c: ZLSD size */

    /* SE banks, also used for music. Most table fields seems reserved/defaults and
     * don't change much between subsongs or files, so they aren't described in detail.
     * Entry sizes are variable (usually flag + extra size xN) so table offsets are needed. */


    /* SBlk part: parse header */
    if (read_32bit(sblk_offset+0x00,sf) != 0x6B6C4253) /* "klBS" (SBlk = sample block?) */
        goto fail;
    sblk_version = read_32bit(sblk_offset+0x04,sf);
    /* 0x08: possibly when sblk_version=0x0d, 0x03=Vita, 0x06=PS4 */
    //;VGM_LOG("BNK: sblk_offset=%lx, data_offset=%lx, sblk_version %x\n", sblk_offset, data_offset, sblk_version);

    {
        int i;
        off_t table1_offset, table2_offset, table3_offset, table4_offset;
        size_t section_entries, material_entries, stream_entries;
        size_t table1_entry_size;
        off_t table1_suboffset, table2_suboffset;
        off_t table2_entry_offset = 0, table3_entry_offset = 0;
        int table4_entry_id = -1;
        off_t table4_entries_offset, table4_names_offset;


        switch(sblk_version) {
            case 0x01: /* Ratchet & Clank (PS2) */
                section_entries  = (uint16_t)read_16bit(sblk_offset+0x16,sf); /* entry size: ~0x0c */
                material_entries = (uint16_t)read_16bit(sblk_offset+0x18,sf); /* entry size: ~0x28 */
                stream_entries   = (uint16_t)read_16bit(sblk_offset+0x1a,sf); /* entry size: none (count) */
                table1_offset    = sblk_offset + read_32bit(sblk_offset+0x1c,sf);
                table2_offset    = sblk_offset + read_32bit(sblk_offset+0x20,sf);
                table3_offset    = table2_offset; /* mixed table in this version */
                table4_offset    = 0; /* not included */

                table1_entry_size = 0; /* not used */
                table1_suboffset = 0;
                table2_suboffset = 0;
                break;

            case 0x03: /* Yu-Gi-Oh! GX - The Beginning of Destiny (PS2) */
            case 0x04: /* Test banks */
            case 0x05: /* Ratchet & Clank (PS3) */
            case 0x09: /* Puyo Puyo Tetris (PS4) */
                section_entries  = (uint16_t)read_16bit(sblk_offset+0x16,sf); /* entry size: ~0x0c */
                material_entries = (uint16_t)read_16bit(sblk_offset+0x18,sf); /* entry size: ~0x08 */
                stream_entries   = (uint16_t)read_16bit(sblk_offset+0x1a,sf); /* entry size: ~0x18 + variable */
                table1_offset    = sblk_offset + read_32bit(sblk_offset+0x1c,sf);
                table2_offset    = sblk_offset + read_32bit(sblk_offset+0x20,sf);
                table3_offset    = sblk_offset + read_32bit(sblk_offset+0x34,sf);
                table4_offset    = sblk_offset + read_32bit(sblk_offset+0x38,sf);

                table1_entry_size = 0x0c;
                table1_suboffset = 0x08;
                table2_suboffset = 0x00;
                break;

            case 0x0d: /* Polara (Vita), Crypt of the Necrodancer (Vita) */
            case 0x0e: /* Yakuza 6's Puyo Puyo (PS4) */
                table1_offset    = sblk_offset + read_32bit(sblk_offset+0x18,sf);
                table2_offset    = sblk_offset + read_32bit(sblk_offset+0x1c,sf);
                table3_offset    = sblk_offset + read_32bit(sblk_offset+0x2c,sf);
                table4_offset    = sblk_offset + read_32bit(sblk_offset+0x30,sf);
                section_entries  = (uint16_t)read_16bit(sblk_offset+0x38,sf); /* entry size: ~0x24 */
                material_entries = (uint16_t)read_16bit(sblk_offset+0x3a,sf); /* entry size: ~0x08 */
                stream_entries   = (uint16_t)read_16bit(sblk_offset+0x3c,sf); /* entry size: ~0x5c + variable */

                table1_entry_size = 0x24;
                table1_suboffset = 0x0c;
                table2_suboffset = 0x00;
                break;

            default:
                VGM_LOG("BNK: unknown version %x\n", sblk_version);
                goto fail;
        }

        //;VGM_LOG("BNK: table offsets=%lx, %lx, %lx, %lx\n", table1_offset,table2_offset,table3_offset,table4_offset);
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

        switch(sblk_version) {
            case 0x01:
                /* table2/3 has size 0x28 entries, seemingly:
                 * 0x00: subtype(01=sound)
                 * 0x08: same as other versions (pitch, flags, offset...)
                 * rest: padding
                 * 0x18: stream offset
                 * there is no stream size like in v0x03
                 */

                for (i = 0; i < material_entries; i++) {
                    uint8_t table2_type = read_32bit(table2_offset + (i*0x28) + 0x00, sf);

                    if (table2_type != 0x01)
                        continue;

                    total_subsongs++;
                    if (total_subsongs == target_subsong) {
                        table2_entry_offset = 0;
                        table3_entry_offset = (i*0x28) + 0x08;
                        /* continue to count all subsongs*/
                    }

                }

                break;

            default:
                for (i = 0; i < material_entries; i++) {
                    uint32_t table2_value, table2_subinfo, table2_subtype;

                    table2_value = (uint32_t)read_32bit(table2_offset+(i*0x08)+table2_suboffset+0x00,sf);
                    table2_subinfo = (table2_value >>  0) & 0xFFFF;
                    table2_subtype = (table2_value >> 16) & 0xFFFF;
                    if (table2_subtype != 0x100)
                        continue; /* not sounds */

                    total_subsongs++;
                    if (total_subsongs == target_subsong) {
                        table2_entry_offset = (i*0x08);
                        table3_entry_offset = table2_subinfo;
                        /* continue to count all subsongs*/
                    }
                }

                break;
        }


        //;VGM_LOG("BNK: subsongs %i, table2_entry=%lx, table3_entry=%lx\n", total_subsongs,table2_entry_offset,table3_entry_offset);

        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;
        /* this means some subsongs repeat streams, that can happen in some sfx banks, whatevs */
        if (total_subsongs != stream_entries) {
            VGM_LOG("BNK: subsongs %i vs table3 %i don't match\n", total_subsongs, stream_entries);
            /* find_dupes...? */
        }

        //;VGM_LOG("BNK: header entry at %lx\n", table3_offset+table3_entry_offset);

        /* parse sounds */
        switch(sblk_version) {
            case 0x01:
            case 0x03:
            case 0x04:
            case 0x05:
            case 0x09:
                pitch   = (uint8_t)read_8bit(table3_offset+table3_entry_offset+0x02,sf);
                flags   = (uint8_t)read_8bit(table3_offset+table3_entry_offset+0x0f,sf);
                stream_offset   = read_32bit(table3_offset+table3_entry_offset+0x10,sf);
                stream_size     = read_32bit(table3_offset+table3_entry_offset+0x14,sf);

                /* must use some log/formula but whatevs */
                switch(pitch) {
                    case 0xC6: sample_rate = 50000; break; //?
                    case 0xC4: sample_rate = 48000; break;
                    case 0xC3: sample_rate = 46000; break; //?
                    case 0xC2: sample_rate = 44100; break;
                    case 0xBC: sample_rate = 36000; break; //?
                    case 0xBA: sample_rate = 32000; break; //?
                    case 0xB9: sample_rate = 30000; break; //?
                    case 0xB8: sample_rate = 28000; break; //?
                    case 0xB6: sample_rate = 22050; break;
                    case 0xB4: sample_rate = 18000; break; //?
                    case 0xB2: sample_rate = 16000; break; //?
                    case 0xB0: sample_rate = 15000; break; //?
                    case 0xAF: sample_rate = 14000; break; //?
                    case 0xAE: sample_rate = 13000; break; //?
                    case 0xAD: sample_rate = 12500; break; //?
                    case 0xAC: sample_rate = 12000; break; //?
                    case 0xAB: sample_rate = 11050; break; //?
                    case 0xAA: sample_rate = 11025; break;
                    case 0xA9: sample_rate = 10000; break; //?
                    case 0xA8: sample_rate = 9000; break; //?
                    case 0xA7: sample_rate = 8000; break; //?
                    case 0xA6: sample_rate = 7000; break; //?
                    case 0xA5: sample_rate = 6500; break; //?
                    case 0xA4: sample_rate = 6000; break; //?
                    case 0xA3: sample_rate = 5800; break; //?
                    case 0xA2: sample_rate = 5400; break; //?
                    case 0xA1: sample_rate = 5000; break; //?
                    case 0x9d: sample_rate = 4000; break; //?
                    case 0x9c: sample_rate = 3500; break; //?
                    default:
                        VGM_LOG("BNK: unknown pitch %x\n", pitch);
                        goto fail;
                }
                break;

            case 0x0d:
            case 0x0e:
                flags   = (uint8_t)read_8bit(table3_offset+table3_entry_offset+0x12,sf);
                stream_offset   = read_32bit(table3_offset+table3_entry_offset+0x44,sf);
                stream_size     = read_32bit(table3_offset+table3_entry_offset+0x48,sf);
                sample_rate  = (int)read_f32(table3_offset+table3_entry_offset+0x4c,sf);
                break;

            default:
                goto fail;
        }

        //;VGM_LOG("BNK: stream at %lx + %x\n", stream_offset, stream_size);

        /* parse names */
        switch(sblk_version) {
          //case 0x03: /* different format? */
          //case 0x04: /* different format? */
            case 0x09:
            case 0x0d:
            case 0x0e:
                /* find if this sound has an assigned name in table1 */
                for (i = 0; i < section_entries; i++) {
                    off_t entry_offset = (uint16_t)read_16bit(table1_offset+(i*table1_entry_size)+table1_suboffset+0x00,sf);

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
                table4_entries_offset = table4_offset + read_32bit(table4_offset+0x08, sf);
                table4_names_offset = table4_entries_offset + (0x10*section_entries);
                //;VGM_LOG("BNK: t4_entries=%lx, t4_names=%lx\n", table4_entries_offset, table4_names_offset);

                /* get assigned name from table4 names */
                for (i = 0; i < section_entries; i++) {
                    int entry_id = read_32bit(table4_entries_offset+(i*0x10)+0x0c, sf);
                    if (entry_id == table4_entry_id) {
                        name_offset = table4_names_offset + read_32bit(table4_entries_offset+(i*0x10)+0x00, sf);
                        break;
                    }
                }

                break;
            default:
                break;
        }

        //;VGM_LOG("BNK: stream_offset=%lx, stream_size=%x, name_offset=%lx\n", stream_offset, stream_size, name_offset);
    }


    /* data part: parse extradata before the codec, if needed */
    {
        int type, loop_length;
        size_t extradata_size = 0, postdata_size = 0;
        start_offset = data_offset + stream_offset;

        switch(sblk_version) {
            case 0x01:
            case 0x03:
            case 0x04:
            case 0x05:
                channel_count = 1;

                /* early versions don't have PS-ADPCM size, could check next offset but it's all kind of loopy */
                if (sblk_version <= 0x03 && stream_size == 0 && (flags & 0x80) == 0) {
                    off_t offset;
                    off_t max_offset = get_streamfile_size(sf);

                    stream_size += 0x10;
                    for (offset = data_offset + stream_offset + 0x10; offset < max_offset; offset += 0x10) {

                        /* beginning frame (if file loops won't have end frame) */
                        if (read_32bitBE(offset + 0x00, sf) == 0x00000000 && read_32bitBE(offset + 0x04, sf) == 0x00000000)
                            break;

                        stream_size += 0x10;

                        /* end frame */
                        if (read_32bitBE(offset + 0x00, sf) == 0x00077777 && read_32bitBE(offset + 0x04, sf) == 0x77777777)
                            break;
                    }

                    //;VGM_LOG("BNK: stream offset=%lx + %lx, new size=%x\n", data_offset, stream_offset, stream_size);
                }


                /* hack for PS3 files that use dual subsongs as stereo */
                if (total_subsongs == 2 && stream_size * 2 == data_size) {
                    channel_count = 2;
                    stream_size = stream_size*channel_count;
                    total_subsongs = 1;
                }
                interleave = stream_size / channel_count;

                if (flags & 0x80) {
                    codec = PCM16; /* rare [Wipeout HD (PS3)] */
                }
                else {
                    loop_flag = ps_find_loop_offsets(sf, start_offset, stream_size, channel_count, interleave, &loop_start, &loop_end);
                    loop_flag = (flags & 0x40); /* no loops values in sight so may only apply to PS-ADPCM flags */

                    codec = PSX;
                }

                //postdata_size = 0x10; /* last frame may be garbage */
                break;

            case 0x09:
                type = read_16bit(start_offset+0x00,sf);
                extradata_size = 0x08 + read_32bit(start_offset+0x04,sf); /* 0x14 for AT9 */

                switch(type) {
                    case 0x02: /* ATRAC9 mono */
                    case 0x05: /* ATRAC9 stereo */
                        if (read_32bit(start_offset+0x08,sf) + 0x08 != extradata_size) /* repeat? */
                            goto fail;
                        channel_count = (type == 0x02) ? 1 : 2;

                        atrac9_info = (uint32_t)read_32bitBE(start_offset+0x0c,sf);
                        /* 0x10: null? */
                        loop_length = read_32bit(start_offset+0x14,sf);
                        loop_start  = read_32bit(start_offset+0x18,sf);
                        loop_end = loop_start + loop_length; /* loop_start is -1 if not set */

                        codec = ATRAC9;
                        break;

                    default:
                        VGM_LOG("BNK: unknown type %x\n", type);
                        goto fail;
                }
                break;

            case 0x0d:
            case 0x0e:
                type = read_16bit(start_offset+0x00,sf);
                if (read_32bit(start_offset+0x04,sf) != 0x01) /* type? */
                    goto fail;
                extradata_size = 0x10 + read_32bit(start_offset+0x08,sf); /* 0x80 for AT9, 0x10 for PCM/PS-ADPCM */
                /* 0x0c: null? */

                switch(type) {
                    case 0x02: /* ATRAC9 mono */
                    case 0x05: /* ATRAC9 stereo */
                        if (read_32bit(start_offset+0x10,sf) + 0x10 != extradata_size) /* repeat? */
                            goto fail;
                        channel_count = (type == 0x02) ? 1 : 2;

                        atrac9_info = (uint32_t)read_32bitBE(start_offset+0x14,sf);
                        /* 0x18: null? */
                        /* 0x1c: channels? */
                        /* 0x20: null? */

                        loop_length = read_32bit(start_offset+0x24,sf);
                        loop_start = read_32bit(start_offset+0x28,sf);
                        loop_end = loop_start + loop_length; /* loop_start is -1 if not set */

                        codec = ATRAC9;
                        break;

                    case 0x01: /* PCM16LE mono? (NekoBuro/Polara sfx) */
                    case 0x04: /* PCM16LE stereo? (NekoBuro/Polara sfx) */
                        /* 0x10: null? */
                        channel_count = read_32bit(start_offset+0x14,sf);
                        interleave = 0x02;

                        loop_start = read_32bit(start_offset+0x18,sf);
                        loop_length = read_32bit(start_offset+0x1c,sf);
                        loop_end = loop_start + loop_length; /* loop_start is -1 if not set */

                        codec = PCM16;
                        break;

                    case 0x00: /* PS-ADPCM (test banks) */
                        /* 0x10: null? */
                        channel_count = read_32bit(start_offset+0x14,sf);
                        interleave = 0x02;

                        loop_start = read_32bit(start_offset+0x18,sf);
                        loop_length = read_32bit(start_offset+0x1c,sf);
                        loop_end = loop_start + loop_length; /* loop_start is -1 if not set */

                        codec = HEVAG;
                        break;

                    default:
                        VGM_LOG("BNK: unknown type %x\n", type);
                        goto fail;
                }
                break;

            default:
                VGM_LOG("BNK: unknown data version %x\n", sblk_version);
                goto fail;
        }

        start_offset += extradata_size;
        stream_size -= extradata_size;
        stream_size -= postdata_size;
        //;VGM_LOG("BNK: offset=%lx, size=%x\n", start_offset, stream_size);
    }

    loop_flag = (loop_start >= 0) && (loop_end > 0);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;

    vgmstream->meta_type = meta_BNK_SONY;

    switch(codec) {
#ifdef VGM_USE_ATRAC9
        case ATRAC9: {
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
            vgmstream->loop_end_sample = loop_end;
            break;
    }
#endif
        case PCM16:
            vgmstream->coding_type = big_endian ? coding_PCM16BE : coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;

            vgmstream->num_samples = pcm_bytes_to_samples(stream_size, vgmstream->channels, 16);
            vgmstream->loop_start_sample = loop_start;
            vgmstream->loop_end_sample = loop_end;
            break;

        case PSX:
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;

            vgmstream->num_samples = ps_bytes_to_samples(stream_size,channel_count);
            vgmstream->loop_start_sample = loop_start;
            vgmstream->loop_end_sample = loop_end;
            break;

        case HEVAG:
            vgmstream->coding_type = coding_HEVAG;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;

            vgmstream->num_samples = ps_bytes_to_samples(stream_size,channel_count);
            vgmstream->loop_start_sample = loop_start;
            vgmstream->loop_end_sample = loop_end;
            break;

        default:
            goto fail;
    }

    if (name_offset)
        read_string(vgmstream->stream_name,STREAM_NAME_SIZE, name_offset,sf);


    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}


#if 0
/* .BNK - Sony's bank, earlier version [Jak and Daxter (PS2), NCAA Gamebreaker 2001 (PS2)] */
VGMSTREAM * init_vgmstream_bnk_sony_v2(STREAMFILE *sf) {
    /* 0x00: 0x00000001
     * 0x04: sections (2 or 3)
     * 0x08+: similar as above (start, size) but with "SBv2"
     *
     * 0x2C/2E=entries?
     * 0x34: table offsets (from sbv2 start)
     * - table1: 0x1c entries
     * - table2: 0x08 entries, possibly point to table3
     * - table3: 0x18 entries, same format as early SBlk (pitch/flags/offset, no size)
     */

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
#endif
