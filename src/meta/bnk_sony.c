#include <math.h>
#include "meta.h"
#include "../coding/coding.h"
#include "../util/endianness.h"

typedef enum { PSX, PCM16, ATRAC9, HEVAG } bnk_codec;

/* .BNK - Sony's SCREAM bank format [The Sly Collection (PS3), Puyo Puyo Tetris (PS4), NekoBuro: Cats Block (Vita)] */
VGMSTREAM* init_vgmstream_bnk_sony(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset, stream_offset, name_offset = 0;
    uint32_t sblk_offset, data_offset;
    uint32_t stream_size, data_size, interleave = 0;
    int channels = 0, loop_flag, sample_rate, parts, sblk_version, big_endian;
    int loop_start = 0, loop_end = 0;
    uint32_t center_note, center_fine, flags;
    uint32_t atrac9_info = 0;

    int total_subsongs, target_subsong = sf->stream_index;
    read_u16_t read_u16;
    read_u32_t read_u32;
    read_f32_t read_f32;
    bnk_codec codec;


    /* bnk/SCREAM tool version */
    if (read_u32be(0x00,sf) == 0x03) { /* PS3 */
        read_u32 = read_u32be;
        read_u16 = read_u16be;
        read_f32 = read_f32be;
        big_endian = 1;
    }
    else if (read_u32le(0x00,sf) == 0x03) { /* PS2/Vita/PS4 */
        read_u32 = read_u32le;
        read_u16 = read_u16le;
        read_f32 = read_f32le;
        big_endian = 0;
    }
    else {
        goto fail;
    }

    /* checks */
    if (!check_extensions(sf, "bnk"))
        goto fail;


    parts = read_u32(0x04,sf);
    if (parts < 2 || parts > 3) goto fail;
    /* in theory a bank can contain multiple blocks */

    sblk_offset = read_u32(0x08,sf);
    /* 0x0c: sblk size */
    data_offset = read_u32(0x10,sf);
    data_size = read_u32(0x14,sf);
    /* when sblk_offset >= 0x20: */
    /* 0x18: ZLSD small footer, rare in earlier versions [Yakuza 6's Puyo Puyo (PS4)] */
    /* 0x1c: ZLSD size */

    /* SE banks, also used for music. Most table fields seems reserved/defaults and
     * don't change much between subsongs or files, so they aren't described in detail.
     * Entry sizes are variable (usually flag + extra size xN) so table offsets are needed. */


    /* SBlk part: parse header */
    if (read_u32(sblk_offset+0x00,sf) != get_id32be("klBS")) /* SBlk = SFX block */
        goto fail;
    sblk_version = read_u32(sblk_offset+0x04,sf);
    /* 0x08: flags? (sblk_version>=0x0d?, 0x03=Vita, 0x06=PS4, 0x05=PS5)
     * - 04: non-fixed bank?
     * - 100: has names
     * - 200: has user data */
    /* version < v0x1a: 
     * - 0x0c: block id 
     * - 0x10: block number 
     * - 0x11: padding
     * version >= v0x1a: 
     * - 0x0c: hash (0x10)
     * - 0x1c: filename (0x100?) */   
    //;VGM_LOG("BNK: sblk_offset=%lx, data_offset=%lx, sblk_version %x\n", sblk_offset, data_offset, sblk_version);

    {
        int i;
        uint32_t table1_offset, table2_offset, table3_offset, table4_offset;
        uint32_t section_entries, material_entries, stream_entries;
        uint32_t table1_entry_size;
        uint32_t table1_suboffset, table2_suboffset;
        uint32_t table2_entry_offset = 0, table3_entry_offset = 0;
        int table4_entry_id = -1;
        uint32_t table4_entries_offset, table4_names_offset;


        switch(sblk_version) {
            case 0x01: /* Ratchet & Clank (PS2) */
                section_entries  = read_u16(sblk_offset+0x16,sf); /* entry size: ~0x0c */
                material_entries = read_u16(sblk_offset+0x18,sf); /* entry size: ~0x28 */
                stream_entries   = read_u16(sblk_offset+0x1a,sf); /* entry size: none (count) */
                table1_offset    = sblk_offset + read_u32(sblk_offset+0x1c,sf);
                table2_offset    = sblk_offset + read_u32(sblk_offset+0x20,sf);
                table3_offset    = table2_offset; /* mixed table in this version */
                table4_offset    = 0; /* not included */

                table1_entry_size = 0; /* not used */
                table1_suboffset = 0;
                table2_suboffset = 0;
                break;

            case 0x03: /* Yu-Gi-Oh! GX - The Beginning of Destiny (PS2) */
            case 0x04: /* Test banks */
            case 0x05: /* Ratchet & Clank (PS3) */
            case 0x08: /* Playstation Home Arcade (Vita) */
            case 0x09: /* Puyo Puyo Tetris (PS4) */
                section_entries  = read_u16(sblk_offset+0x16,sf); /* entry size: ~0x0c (NumSounds)*/
                material_entries = read_u16(sblk_offset+0x18,sf); /* entry size: ~0x08 (NumGrains) */
                stream_entries   = read_u16(sblk_offset+0x1a,sf); /* entry size: ~0x18 + variable (NumWaveforms) */
                table1_offset    = sblk_offset + read_u32(sblk_offset+0x1c,sf); /* sound offset */
                table2_offset    = sblk_offset + read_u32(sblk_offset+0x20,sf); /* grain offset */
                /* 0x24: VAG address? */
                /* 0x28: data size */
                /* 0x2c: RAM size */
                /* 0x30: next block offset */
                table3_offset    = sblk_offset + read_u32(sblk_offset+0x34,sf); /* grain data? */
                table4_offset    = sblk_offset + read_u32(sblk_offset+0x38,sf); /* block names */
                /*0x3c: SFXUD? */

                table1_entry_size = 0x0c;
                table1_suboffset = 0x08;
                table2_suboffset = 0x00;
                break;

            case 0x0d: /* Polara (Vita), Crypt of the Necrodancer (Vita) */
            case 0x0e: /* Yakuza 6's Puyo Puyo (PS4) */
            case 0x0f: /* Ikaruga (PS4) */
                table1_offset    = sblk_offset + read_u32(sblk_offset+0x18,sf);
                table2_offset    = sblk_offset + read_u32(sblk_offset+0x1c,sf);
                table3_offset    = sblk_offset + read_u32(sblk_offset+0x2c,sf);
                table4_offset    = sblk_offset + read_u32(sblk_offset+0x30,sf);
                section_entries  = read_u16(sblk_offset+0x38,sf); /* entry size: ~0x24 */
                material_entries = read_u16(sblk_offset+0x3a,sf); /* entry size: ~0x08 */
                stream_entries   = read_u16(sblk_offset+0x3c,sf); /* entry size: ~0x5c + variable */

                table1_entry_size = 0x24;
                table1_suboffset = 0x0c;
                table2_suboffset = 0x00;
                break;

            case 0x1a: /* Demon's Souls (PS5) */

            default:
                vgm_logi("BNK: unknown version %x (report)\n", sblk_version);
                goto fail;
        }

        //;VGM_LOG("BNK: table offsets=%x, %x, %x, %x\n", table1_offset,table2_offset,table3_offset,table4_offset);
        //;VGM_LOG("BNK: table entries=%i, %i, %i\n", section_entries,material_entries,stream_entries);


        /* table defs:
         * - table1: sections, point to some materials (may be less than streams/materials)
         *           (a "sound" that has N grains, and is triggered by games like a cue) 
         * - table2: materials, point to all sounds or others subtypes (may be more than sounds)
         *           (a "grain" that does actions like play or changes volume)
         * - table3: sounds, point to streams (multiple sounds can repeat stream)
         *           (a "waveform" being the actual stream)
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
                    uint32_t table2_type = read_u32(table2_offset + (i*0x28) + 0x00, sf);

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

                    table2_value = read_u32(table2_offset+(i*0x08)+table2_suboffset+0x00,sf);
                    table2_subinfo = (table2_value >>  0) & 0xFFFF;
                    table2_subtype = (table2_value >> 16) & 0xFFFF;
                    if (table2_subtype != 0x0100)
                        continue; /* not sounds (ex. 1: waveform, 42: silence, 25: random, etc) */

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
            VGM_LOG("BNK: subsongs %i vs table3 %i don't match (repeated streams?)\n", total_subsongs, stream_entries);
            /* TODO: find dupes?  */
        }

        //;VGM_LOG("BNK: header entry at %lx\n", table3_offset+table3_entry_offset);

        /* parse sounds */
        switch(sblk_version) {
            case 0x01:
            case 0x03:
            case 0x04:
            case 0x05:
            case 0x08:
            case 0x09:
                /* "tone" */
                /* 0x00: priority */
                /* 0x01: volume */
                center_note     = read_u8 (table3_offset+table3_entry_offset+0x02,sf);
                center_fine     = read_u8 (table3_offset+table3_entry_offset+0x03,sf);
                /* 0x04: pan */
                /* 0x06: map low */
                /* 0x07: map high */
                /* 0x08: pitch bend low */
                /* 0x09: pitch bend high */
                /* 0x0a: ADSR1 */
                /* 0x0c: ADSR2 */
                flags           = read_u16(table3_offset+table3_entry_offset+0x0e,sf);
                stream_offset   = read_u32(table3_offset+table3_entry_offset+0x10,sf);
                stream_size     = read_u32(table3_offset+table3_entry_offset+0x14,sf);

                /* "base" sample rates, allowed by the tool (for other rates must use base + semitones, but aren't exact) */
                if (center_note == 0xC4 && center_fine == 0x00)
                    sample_rate = 48000;
                else if (center_note == 0xC2 && center_fine == 0x42)
                    sample_rate = 44100;
                else if (center_note == 0xb6 && center_fine == 0x42)
                    sample_rate = 22050;
                else if (center_note == 0xaa && center_fine == 0x42)
                    sample_rate = 11025;
                else if (center_note == 0xa4 && center_fine == 0x7c)
                    sample_rate = 8000;
                else {
                    /* rough ("center") sample rates using semitone-to-hz formula: (rate) * 2 ^ ((pitch - base) / 12) */
                    double curr_rate = 48000 * pow(2.0, (double)((int)center_note - 0xc4) / 12.0);
                    double prev_rate = 48000 * pow(2.0, (double)(((int)center_note - 1) - 0xc4) / 12.0);
                    /* partial semitone, from 0x00 = 0.0 to 0x7f = 1.0 of a semitone for current rate */
                    float fine_pct = center_fine / 127.0f;

                    //TODO improve (fine seems approximate and not sure how to calc current semitone hz value, so needs prev_rate)
                    sample_rate =  curr_rate + (curr_rate - prev_rate) * fine_pct;

                    /* some odd "beep" sfx in Sly 2/3 seems to go slightly higher after applying fine_pct, probably should resample */
                    if (sample_rate > VGMSTREAM_MAX_SAMPLE_RATE)
                        sample_rate = VGMSTREAM_MAX_SAMPLE_RATE;

                    /* waves can set base sample rate (48/44/22/11/8khz) + pitch in semitones, then converted to center+fine
                     * 48000 + pitch 0.0  > center=0xc4, fine=0x00
                     * 48000 + pitch 0.10 > center=0xc4, fine=0x0c
                     * 48000 + pitch 0.50 > center=0xc4, fine=0x3f
                     * 48000 + pitch 0.99 > center=0xc4, fine=0x7d
                     * 48000 + pitch 1.00 > center=0xc5, fine=0x00
                     * 48000 + pitch 12.0 > center=0xd0, fine=0x00
                     * 48000 + pitch 24.0 > center=0xdc, fine=0x00
                     * 48000 + pitch 56.0 > center=0xfc, fine=0x00
                     * 48000 + pitch 68.0 > center=0x08, fine=0x00 > ?
                     * 48000 + pitch -12.0 > center=0xb8, fine=0x00
                     * 48000 + pitch -0.10 > center=0xc3, fine=0x72
                     * 48000 + pitch -0.001 > not allowed
                     * 8000  + pitch 1.00  > center=0xa4, fine=0x7c
                     * 8000  + pitch -12.00 > center=0x98, fine=0x7c
                     * 8000  + pitch -48.00 > center=0x74, fine=0x7c
                     */
                }
                break;

            case 0x0d:
            case 0x0e:
            case 0x0f:
                flags           = read_u8   (table3_offset+table3_entry_offset+0x12,sf);
                stream_offset   = read_u32(table3_offset+table3_entry_offset+0x44,sf);
                stream_size     = read_u32(table3_offset+table3_entry_offset+0x48,sf);
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
            case 0x08:
            case 0x09:
            case 0x0d:
            case 0x0e:
                /* find if this sound has an assigned name in table1 */
                for (i = 0; i < section_entries; i++) {
                    uint32_t entry_offset = read_u16(table1_offset+(i*table1_entry_size)+table1_suboffset+0x00,sf);

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
                table4_entries_offset = table4_offset + read_u32(table4_offset+0x08, sf);
                table4_names_offset = table4_entries_offset + (0x10*section_entries);
                //;VGM_LOG("BNK: t4_entries=%lx, t4_names=%lx\n", table4_entries_offset, table4_names_offset);

                /* get assigned name from table4 names */
                for (i = 0; i < section_entries; i++) {
                    int entry_id = read_u32(table4_entries_offset+(i*0x10)+0x0c, sf);
                    if (entry_id == table4_entry_id) {
                        name_offset = table4_names_offset + read_u32(table4_entries_offset+(i*0x10)+0x00, sf);
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
                channels = 1;

                /* early versions don't have PS-ADPCM size, could check next offset but it's all kind of loopy */
                if (sblk_version <= 0x03 && stream_size == 0 && (flags & 0x80) == 0) {
                    uint32_t offset;
                    uint32_t max_offset = get_streamfile_size(sf);

                    stream_size += 0x10;
                    for (offset = data_offset + stream_offset + 0x10; offset < max_offset; offset += 0x10) {

                        /* beginning frame (if file loops won't have end frame) */
                        if (read_u32be(offset + 0x00, sf) == 0x00000000 && read_u32be(offset + 0x04, sf) == 0x00000000)
                            break;

                        stream_size += 0x10;

                        /* end frame */
                        if (read_u32be(offset + 0x00, sf) == 0x00077777 && read_u32be(offset + 0x04, sf) == 0x77777777)
                            break;
                    }

                    //;VGM_LOG("BNK: stream offset=%lx + %lx, new size=%x\n", data_offset, stream_offset, stream_size);
                }


                /* hack for PS3 files that use dual subsongs as stereo */
                if (total_subsongs == 2 && stream_size * 2 == data_size) {
                    channels = 2;
                    stream_size = stream_size*channels;
                    total_subsongs = 1;
                }
                interleave = stream_size / channels;

                /* PS Home Arcade has other flags? supposedly:
                 *  01 = reverb
                 *  02 = vol scale 20
                 *  04 = vol scale 50
                 *  06 = vol scale 100
                 *  08 = noise
                 *  10 = no dry
                 *  20 = no steal
                 *  40 = loop VAG
                 *  80 = PCM
                 *  100 = has advanced packets
                 *  200 = send LFE
                 *  400 = send center
                 */
                if ((flags & 0x80) && sblk_version <= 3) { 
                    codec = PCM16; /* rare [Wipeout HD (PS3)]-v3 */
                }
                else {
                    loop_flag = ps_find_loop_offsets(sf, start_offset, stream_size, channels, interleave, &loop_start, &loop_end);
                    loop_flag = (flags & 0x40); /* only applies to PS-ADPCM flags */

                    codec = PSX;
                }

                //postdata_size = 0x10; /* last frame may be garbage */
                break;

            case 0x08:
            case 0x09:
                type = read_u16(start_offset+0x00,sf);
                extradata_size = 0x08 + read_u32(start_offset+0x04,sf); /* 0x14 for AT9 */

                switch(type) {
                    case 0x00:
                        channels = 1;
                        codec = PSX;
                        interleave = 0x10;
                        break;

                    case 0x01:
                        channels = 1;
                        codec = PCM16;
                        interleave = 0x01;
                        break;


                    case 0x02: /* ATRAC9 mono */
                    case 0x05: /* ATRAC9 stereo */
                        if (read_u32(start_offset+0x08,sf) + 0x08 != extradata_size) /* repeat? */
                            goto fail;
                        channels = (type == 0x02) ? 1 : 2;

                        atrac9_info = read_u32be(start_offset+0x0c,sf);
                        /* 0x10: null? */
                        loop_length = read_u32(start_offset+0x14,sf);
                        loop_start  = read_u32(start_offset+0x18,sf);
                        loop_end = loop_start + loop_length; /* loop_start is -1 if not set */

                        codec = ATRAC9;
                        break;

                    default:
                        vgm_logi("BNK: unknown type %x (report)\n", type);
                        goto fail;
                }
                break;

            case 0x0d:
            case 0x0e:
            case 0x0f:
                type = read_u16(start_offset+0x00,sf);
                if (read_u32(start_offset+0x04,sf) != 0x01) /* type? */
                    goto fail;
                extradata_size = 0x10 + read_u32(start_offset+0x08,sf); /* 0x80 for AT9, 0x10 for PCM/PS-ADPCM */
                /* 0x0c: null? */

                switch(type) {
                    case 0x02: /* ATRAC9 mono */
                    case 0x05: /* ATRAC9 stereo */
                        if (read_u32(start_offset+0x10,sf) + 0x10 != extradata_size) /* repeat? */
                            goto fail;
                        channels = (type == 0x02) ? 1 : 2;

                        atrac9_info = read_u32be(start_offset+0x14,sf);
                        /* 0x18: null? */
                        /* 0x1c: channels? */
                        /* 0x20: null? */

                        loop_length = read_u32(start_offset+0x24,sf);
                        loop_start = read_u32(start_offset+0x28,sf);
                        loop_end = loop_start + loop_length; /* loop_start is -1 if not set */

                        codec = ATRAC9;
                        break;

                    case 0x01: /* PCM16LE mono? (NekoBuro/Polara sfx) */
                    case 0x04: /* PCM16LE stereo? (NekoBuro/Polara sfx) */
                        /* 0x10: null? */
                        channels = read_u32(start_offset+0x14,sf);
                        interleave = 0x02;

                        loop_start = read_u32(start_offset+0x18,sf);
                        loop_length = read_u32(start_offset+0x1c,sf);
                        loop_end = loop_start + loop_length; /* loop_start is -1 if not set */

                        codec = PCM16;
                        break;

                    case 0x00: /* HEVAG (test banks) */
                    case 0x03: /* HEVAG (Ikaruga) */
                        /* 0x10: null? */
                        channels = read_u32(start_offset+0x14,sf);
                        interleave = 0x10;

                        loop_start = read_u32(start_offset+0x18,sf);
                        loop_length = read_u32(start_offset+0x1c,sf);
                        loop_end = loop_start + loop_length; /* loop_start is -1 if not set */

                        codec = HEVAG;
                        //TODO: in v0x0f right before start_offset is the .vag filename, see if offset can be found
                        break;

                    default:
                        vgm_logi("BNK: unknown type %x (report)\n", type);
                        goto fail;
                }
                break;

            default:
                vgm_logi("BNK: unknown data version %x (report)\n", sblk_version);
                goto fail;
        }

        start_offset += extradata_size;
        stream_size -= extradata_size;
        stream_size -= postdata_size;
        //;VGM_LOG("BNK: offset=%lx, size=%x\n", start_offset, stream_size);
    }

    loop_flag = (loop_start >= 0) && (loop_end > 0);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
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

            vgmstream->num_samples = ps_bytes_to_samples(stream_size,channels);
            vgmstream->loop_start_sample = loop_start;
            vgmstream->loop_end_sample = loop_end;
            break;

        case HEVAG:
            vgmstream->coding_type = coding_HEVAG;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;

            vgmstream->num_samples = ps_bytes_to_samples(stream_size,channels);
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
