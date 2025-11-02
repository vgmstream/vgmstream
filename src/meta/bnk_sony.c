#include <math.h>
#include "meta.h"
#include "../coding/coding.h"
#include "../util/endianness.h"
#include "../util/spu_utils.h"

typedef enum { NONE, DUMMY, EXTERNAL, PSX, PCM16, MPEG, ATRAC9, HEVAG, RIFF_ATRAC9, XVAG_ATRAC9 } bnk_codec;

typedef struct {
    bnk_codec codec;
    bool big_endian;

    // bank related (internal)
    int sblk_version;
    uint32_t sblk_offset;
    uint32_t data_offset;
    uint32_t data_size;
    uint32_t zlsd_offset;
    uint32_t zlsd_size;

    uint32_t table1_offset; // usually sounds/cues (point to grains)
    uint32_t table2_offset; // usually grains/materials (point to waves)
    uint32_t table3_offset; // usually waves (point to streams)
    uint32_t table4_offset; // usually names
    uint32_t sounds_entries;
    uint32_t grains_entries;
    uint32_t stream_entries;
    uint32_t table1_suboffset;
    uint32_t table2_suboffset;
    uint32_t table1_entry_size;
    uint32_t table2_entry_offset;
    uint32_t table3_entry_offset;

    char bank_name[STREAM_NAME_SIZE];
    char stream_name[STREAM_NAME_SIZE];

    /* stream related */
    int total_subsongs;
    int target_subsong;

    int channels;
    int loop_flag;
    int sample_rate;
    int32_t num_samples;
    int32_t loop_start;
    int32_t loop_end;
    int32_t loop_length;
    int32_t encoder_delay;

    uint32_t start_offset;
    uint32_t stream_offset;

    uint32_t stream_size;
    uint32_t interleave;        //hardcoded in most versions

    uint16_t stream_flags;

    uint32_t atrac9_info;

    uint32_t extradata_size;
    uint32_t postdata_size;

    uint32_t subtype;
} bnk_header_t;

static bool parse_bnk_v3(STREAMFILE* sf, bnk_header_t* h);


/* .BNK - Sony's 989SND/SCREAM bank format - SCRiptable Engine for Audio Manipulation
 * [The Sly Collection (PS3), Puyo Puyo Tetris (PS4), NekoBuro: Cats Block (Vita)] */
VGMSTREAM* init_vgmstream_bnk_sony(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    char file_name[STREAM_NAME_SIZE];

    /* checks */
    if (!check_extensions(sf, "bnk"))
        return NULL;

    bnk_header_t h = {0};
    if (!parse_bnk_v3(sf, &h))
        return NULL;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(h.channels, h.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = h.sample_rate;
    vgmstream->num_streams = h.total_subsongs;
    vgmstream->stream_size = h.stream_size;

    vgmstream->meta_type = meta_BNK_SONY;

    if (h.stream_name[0]) {
        get_streamfile_basename(sf, file_name, STREAM_NAME_SIZE);
        if (h.bank_name[0] && strcmp(file_name, h.bank_name) != 0)
            snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%s/%s", h.bank_name, h.stream_name);
        else
            snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%s", h.stream_name);
    }

    switch(h.codec) {
        case DUMMY: {
            VGMSTREAM* temp_vs = NULL;

            temp_vs = init_vgmstream_silence_container(h.total_subsongs);
            if (!temp_vs) goto fail;

            temp_vs->meta_type = vgmstream->meta_type;

            close_vgmstream(vgmstream);
            return temp_vs;
        }

        case EXTERNAL: {
            VGMSTREAM* temp_vs = NULL;
            STREAMFILE* temp_sf = NULL;

            /* try with both stream_name and bank_name/stream_name? */
            temp_sf = open_streamfile_by_filename(sf, h.stream_name);
            if (!temp_sf) { /* create dummy stream if it can't be found */
                temp_vs = init_vgmstream_silence_container(h.total_subsongs);
                if (!temp_vs) goto fail;

                temp_vs->meta_type = vgmstream->meta_type;
                snprintf(temp_vs->stream_name, STREAM_NAME_SIZE, "%s [not found]", vgmstream->stream_name);

                close_vgmstream(vgmstream);
                return temp_vs;
            }

            /* are external streams always xvag? it shouldn't be hardcoded like this, but... */
            /* and at that point does this also need to be put behind #ifdef VGM_USE_ATRAC9? */
            /* known BNK v12 externals use XVAG MPEG but it functions differently in general */
            temp_vs = init_vgmstream_xvag(temp_sf);
            close_streamfile(temp_sf);
            if (!temp_vs) goto fail;

            temp_vs->num_streams = vgmstream->num_streams;
            temp_vs->meta_type = vgmstream->meta_type;
            strcpy(temp_vs->stream_name, vgmstream->stream_name);

            close_vgmstream(vgmstream);
            return temp_vs;
        }

#ifdef VGM_USE_ATRAC9
        case ATRAC9: {
            atrac9_config cfg = {0};

            cfg.channels = vgmstream->channels;
            cfg.config_data = h.atrac9_info;
            //cfg.encoder_delay = 0x00; //todo

            vgmstream->codec_data = init_atrac9(&cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_ATRAC9;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = atrac9_bytes_to_samples(h.stream_size, vgmstream->codec_data);
            vgmstream->loop_start_sample = h.loop_start;
            vgmstream->loop_end_sample = h.loop_end;
            break;
        }

        case RIFF_ATRAC9: {
            VGMSTREAM* temp_vs = NULL;
            STREAMFILE* temp_sf = NULL;

            temp_sf = setup_subfile_streamfile(sf, h.start_offset, h.stream_size, "at9");
            if (!temp_sf) goto fail;

            temp_vs = init_vgmstream_riff(temp_sf);
            close_streamfile(temp_sf);
            if (!temp_vs) goto fail;

            temp_vs->num_streams = vgmstream->num_streams;
            temp_vs->stream_size = vgmstream->stream_size;
            temp_vs->meta_type = vgmstream->meta_type;
            strcpy(temp_vs->stream_name, vgmstream->stream_name);

            close_vgmstream(vgmstream);
            return temp_vs;
        }

        case XVAG_ATRAC9: {
            VGMSTREAM* temp_vs = NULL;
            STREAMFILE* temp_sf = NULL;


            temp_sf = setup_subfile_streamfile(sf, h.start_offset, h.stream_size, "xvag");
            if (!temp_sf) goto fail;
            temp_sf->stream_index = 1;

            temp_vs = init_vgmstream_xvag(temp_sf);
            close_streamfile(temp_sf);
            if (!temp_vs) goto fail;

            /* maybe also a separate warning/fail if XVAG returns more than 1 subsong? */

            temp_vs->num_streams = vgmstream->num_streams;
            //temp_vs->stream_size = vgmstream->stream_size;
            temp_vs->meta_type = vgmstream->meta_type;
            strcpy(temp_vs->stream_name, vgmstream->stream_name);

            close_vgmstream(vgmstream);
            return temp_vs;
        }
#endif
#ifdef VGM_USE_MPEG
        case MPEG: {
            mpeg_custom_config cfg = {0};
            cfg.skip_samples = h.encoder_delay;

            vgmstream->layout_type = layout_none;
            vgmstream->codec_data = init_mpeg_custom(sf, h.start_offset, &vgmstream->coding_type, h.channels, MPEG_STANDARD, &cfg);
            if (!vgmstream->codec_data) goto fail;

            vgmstream->num_samples = h.num_samples;
            break;
        }
#endif

        case PCM16:
            if (h.interleave == 0)
                h.interleave = 0x02;

            vgmstream->coding_type = h.big_endian ? coding_PCM16BE : coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = h.interleave;

            vgmstream->num_samples = pcm_bytes_to_samples(h.stream_size, vgmstream->channels, 16);
            vgmstream->loop_start_sample = h.loop_start;
            vgmstream->loop_end_sample = h.loop_end;
            break;

        case PSX:
            if (h.interleave == 0)
                h.interleave = 0x10;

            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = h.interleave;

            vgmstream->num_samples = ps_bytes_to_samples(h.stream_size, h.channels);
            vgmstream->loop_start_sample = h.loop_start;
            vgmstream->loop_end_sample = h.loop_end;
            break;

        case HEVAG:
            if (h.interleave == 0)
                h.interleave = 0x10;

            vgmstream->coding_type = coding_HEVAG;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = h.interleave;

            vgmstream->num_samples = ps_bytes_to_samples(h.stream_size, h.channels);
            vgmstream->loop_start_sample = h.loop_start;
            vgmstream->loop_end_sample = h.loop_end;
            break;

        default:
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream, sf, h.start_offset))
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


/* base part: read section info */
static bool process_tables(STREAMFILE* sf, bnk_header_t* h) {
    read_u16_t read_u16 = h->big_endian ? read_u16be : read_u16le;
    read_u32_t read_u32 = h->big_endian ? read_u32be : read_u32le;

    /* - table1: sections, point to some materials (may be less than streams/materials)
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

    switch(h->sblk_version) {
        case 0x01: /* NHL FaceOff 2003 (PS2), Ratchet & Clank (PS2) */
            h->sounds_entries   = read_u16(h->sblk_offset+0x16,sf); /* entry size: ~0x0c */
            h->grains_entries   = read_u16(h->sblk_offset+0x18,sf); /* entry size: ~0x28 */
            h->stream_entries   = read_u16(h->sblk_offset+0x1a,sf); /* entry size: none (count) */
            h->table1_offset    = h->sblk_offset + read_u32(h->sblk_offset+0x1c,sf);
            h->table2_offset    = h->sblk_offset + read_u32(h->sblk_offset+0x20,sf);
            h->table3_offset    = h->table2_offset; /* mixed table in this version */
            h->table4_offset    = 0; /* not included */

            h->table1_entry_size = 0; /* not used */
            h->table1_suboffset = 0;
            h->table2_suboffset = 0;
            break;

        case 0x03: /* Yu-Gi-Oh! GX - The Beginning of Destiny (PS2) */
        case 0x04: /* EyePet (PS3), Test banks */
        case 0x05: /* Ratchet & Clank (PS3) */
        case 0x08: /* Playstation Home Arcade (Vita) */
        case 0x09: /* Puyo Puyo Tetris (PS4) */
            h->sounds_entries   = read_u16(h->sblk_offset+0x16,sf); /* entry size: ~0x0c (NumSounds) */
            h->grains_entries   = read_u16(h->sblk_offset+0x18,sf); /* entry size: ~0x08 (NumGrains) */
            h->stream_entries   = read_u16(h->sblk_offset+0x1a,sf); /* entry size: ~0x18 + variable (NumWaveforms) */
            h->table1_offset    = h->sblk_offset + read_u32(h->sblk_offset+0x1c,sf); /* sound offset */
            h->table2_offset    = h->sblk_offset + read_u32(h->sblk_offset+0x20,sf); /* grain offset */
            /* 0x24: VAG address? */
            /* 0x28: data size */
            /* 0x2c: RAM size */
            /* 0x30: next block offset */
            h->table3_offset    = h->sblk_offset + read_u32(h->sblk_offset+0x34,sf); /* grain data? */
            h->table4_offset    = h->sblk_offset + read_u32(h->sblk_offset+0x38,sf); /* block names */
            /* 0x3c: SFXUD? */

            h->table1_entry_size = 0x0c;
            h->table1_suboffset = 0x08;
            h->table2_suboffset = 0x00;
            break;

        case 0x0c: /* SingStar Ultimate Party (PS3/PS4) */
        case 0x0d: /* Polara (Vita), Crypt of the Necrodancer (Vita) */
        case 0x0e: /* Yakuza 6's Puyo Puyo (PS4) */
        case 0x0f: /* Ikaruga (PS4) */
        case 0x10: /* Ginga Force (PS4) */
            h->table1_offset    = h->sblk_offset + read_u32(h->sblk_offset+0x18,sf);
            h->table2_offset    = h->sblk_offset + read_u32(h->sblk_offset+0x1c,sf);
            h->table3_offset    = h->sblk_offset + read_u32(h->sblk_offset+0x2c,sf);
            h->table4_offset    = h->sblk_offset + read_u32(h->sblk_offset+0x30,sf);
            h->sounds_entries   = read_u16(h->sblk_offset+0x38,sf); /* entry size: ~0x24 */
            h->grains_entries   = read_u16(h->sblk_offset+0x3a,sf); /* entry size: ~0x08 */
            h->stream_entries   = read_u16(h->sblk_offset+0x3c,sf); /* entry size: ~0x5c + variable */

            h->table1_entry_size = 0x24;
            h->table1_suboffset = 0x0c;
            h->table2_suboffset = 0x00;
            break;

        /* later versions have a few more tables (some optional) and work slightly differently (header is part of wave) */
        case 0x1a: /* Demon's Souls (PS5) */
        case 0x1c: /* The Last of Us Part II */
        case 0x23: { /* The Last of Us (PC) */
            uint32_t bank_name_offset = h->sblk_offset + (h->sblk_version <= 0x1c ? 0x1c : 0x20);
            uint32_t tables_offset = h->sblk_offset + (h->sblk_version <= 0x1c ? 0x120 : 0x128);
            uint32_t counts_offset = tables_offset + (h->sblk_version <= 0x1c ? 0x98 : 0xb0);

          //h->table1_offset    = h->sblk_offset + read_u32(tables_offset+0x00,sf); /* sounds/cues */
          //h->table2_offset    = 0;
            h->table3_offset    = h->sblk_offset + read_u32(tables_offset+0x08,sf); /* wave offsets with info (integrated grains+waves?)*/
          //h->sounds_entries   = read_u16(counts_offset+0x00,sf);
          //h->grains_entries   = read_u16(counts_offset+0x02,sf);
            h->stream_entries   = read_u16(counts_offset+0x06,sf);

            read_string(h->bank_name, STREAM_NAME_SIZE, bank_name_offset, sf);
            break;
        }

        default:
            vgm_logi("BNK: unknown version %x (report)\n", h->sblk_version);
            goto fail;
    }

    //;VGM_LOG("BNK: table offsets=%x, %x, %x, %x\n", h->table1_offset, h->table2_offset, h->table3_offset, h->table4_offset);
    //;VGM_LOG("BNK: table entries=%i, %i, %i\n", h->sounds_entries, h->grains_entries, h->stream_entries);

    return true;
fail:
    return false;
}

/* header part: read wave info */
static bool process_headers(STREAMFILE* sf, bnk_header_t* h) {
    read_u16_t read_u16 = h->big_endian ? read_u16be : read_u16le;
    read_u32_t read_u32 = h->big_endian ? read_u32be : read_u32le;
    read_f32_t read_f32 = h->big_endian ? read_f32be : read_f32le;

    /* parse materials */
    h->total_subsongs = 0;
    h->target_subsong = sf->stream_index;
    if (h->target_subsong == 0) h->target_subsong = 1;

    switch(h->sblk_version) {
        case 0x01:
            /* table2/3 has size 0x28 entries, seemingly:
             * 0x00: subtype(01=sound)
             * 0x08: same as other versions (pitch, flags, offset...)
             * rest: padding
             * 0x18: stream offset
             * there is no stream size like in v0x03
             */

            for (int i = 0; i < h->grains_entries; i++) {
                uint32_t table2_type = read_u32(h->table2_offset + (i*0x28) + 0x00, sf);

                if (table2_type != 0x01)
                    continue;

                h->total_subsongs++;
                if (h->total_subsongs == h->target_subsong) {
                    h->table2_entry_offset = 0;
                    h->table3_entry_offset = (i * 0x28) + 0x08;
                    /* continue to count all subsongs */
                }

            }

            break;

        case 0x1a:
        case 0x1c:
        case 0x23:
            h->total_subsongs = h->stream_entries;
            h->table3_entry_offset = (h->target_subsong - 1) * 0x08;
            break;

        default:
            for (int i = 0; i < h->grains_entries; i++) {
                uint32_t table2_value, table2_subinfo, table2_subtype;

                table2_value = read_u32(h->table2_offset + (i * 0x08) + h->table2_suboffset + 0x00, sf);
                table2_subinfo = (table2_value >>  0) & 0xFFFF;
                table2_subtype = (table2_value >> 16) & 0xFFFF;
                if (table2_subtype != 0x0100)
                    continue; /* not sounds (ex. 1: waveform, 42: silence, 25: random, etc) */

                h->total_subsongs++;
                if (h->total_subsongs == h->target_subsong) {
                    h->table2_entry_offset = (i * 0x08);
                    h->table3_entry_offset = table2_subinfo;
                    /* continue to count all subsongs */
                }
            }

            break;
    }


    //;VGM_LOG("BNK: subsongs %i, table2_entry=%x, table3_entry=%x\n", h->total_subsongs, h->table2_entry_offset, h->table3_entry_offset);
    if (!h->zlsd_offset && (h->target_subsong < 0 || h->target_subsong > h->total_subsongs || h->total_subsongs < 1))
        return false;
    /* this means some subsongs repeat streams, that can happen in some sfx banks, whatevs */
    if (h->total_subsongs != h->stream_entries) {
        VGM_LOG("BNK: subsongs %i vs table3 %i don't match (repeated streams?)\n", h->total_subsongs, h->stream_entries);
        /* TODO: find dupes?  */
    }

    //;VGM_LOG("BNK: header entry at %x\n", h->table3_offset + h->table3_entry_offset);

    /* is currently working on ZLSD streams */
    if (h->zlsd_offset && h->target_subsong > h->total_subsongs)
        return true;

    uint32_t sndh_offset = h->table3_offset + h->table3_entry_offset;

    /* parse sounds */
    switch(h->sblk_version) {
        case 0x01:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x08:
        case 0x09: {
            /* "tone" */
            // 0x00: priority
            // 0x01: volume
            uint8_t center_note = read_u8 (sndh_offset + 0x02,sf);
            uint8_t center_fine = read_u8 (sndh_offset + 0x03,sf);
            // 0x04: pan
            // 0x06: map low
            // 0x07: map high
            // 0x08: pitch bend low
            // 0x09: pitch bend high
            // 0x0a: ADSR1
            // 0x0c: ADSR2
            h->stream_flags     = read_u16(sndh_offset + 0x0e,sf);
            h->stream_offset    = read_u32(sndh_offset + 0x10,sf);
            h->stream_size      = read_u32(sndh_offset + 0x14,sf);

            int16_t note = 60; // always defaults to middle C?
            int16_t fine = 0;
            int pitch = spu2_note_to_pitch(note, fine, center_note, center_fine);
            h->sample_rate = spu2_pitch_to_sample_rate(pitch);

            /* From SCREAM tool tests, waves can set base sample rate (48/44/22/11/8khz) + pitch in semitones,
             * then converted to center + fine shift:
             * 48000 + pitch   0.00 > center=0xc4, fine=0x00
             * 48000 + pitch   0.10 > center=0xc4, fine=0x0c
             * 48000 + pitch   0.50 > center=0xc4, fine=0x3f
             * 48000 + pitch   0.99 > center=0xc4, fine=0x7d
             * 48000 + pitch   1.00 > center=0xc5, fine=0x00
             * 48000 + pitch  12.00 > center=0xd0, fine=0x00
             * 48000 + pitch  24.00 > center=0xdc, fine=0x00
             * 48000 + pitch  56.00 > center=0xfc, fine=0x00
             * 48000 + pitch  68.00 > center=0x08, fine=0x00 > ?
             * 48000 + pitch -12.00 > center=0xb8, fine=0x00
             * 48000 + pitch  -0.10 > center=0xc3, fine=0x72
             * 48000 + pitch -0.001 > not allowed
             * 8000  + pitch   1.00 > center=0xa4, fine=0x7c
             * 8000  + pitch -12.00 > center=0x98, fine=0x7c
             * 8000  + pitch -48.00 > center=0x74, fine=0x7c
             */
            break;
        }

        case 0x0c:
        case 0x0d:
        case 0x0e:
        case 0x0f:
        case 0x10:
            h->stream_flags     = read_u16(sndh_offset+0x12,sf);
            h->stream_offset    = read_u32(sndh_offset+0x44,sf);
            h->stream_size      = read_u32(sndh_offset+0x48,sf);
            h->sample_rate = (int)read_f32(sndh_offset+0x4c,sf);
            break;

        case 0x1a:
        case 0x1c:
        case 0x23:
            h->stream_offset     = read_u32(sndh_offset+0x00,sf);
            h->sample_rate = 48000; // no sample rate (probably fixed to 48000/system's, but seen in RIFF)

            /* rest is part of data, handled later */
            break;

        default:
            VGM_LOG("BNK: missing version\n");
            return false;
    }

    //;VGM_LOG("BNK: header %x, stream at %x + %x\n", sndh_offset, h->data_offset + h->stream_offset, h->stream_size);
    return true;
}

/* name part: read names  */
static bool process_names(STREAMFILE* sf, bnk_header_t* h) {
    read_u16_t read_u16 = h->big_endian ? read_u16be : read_u16le;
    read_u32_t read_u32 = h->big_endian ? read_u32be : read_u32le;

    /* table4 can be nonexistent */
    if (h->table4_offset <= h->sblk_offset)
        return true;

    /* is currently working on ZLSD streams */
    if (h->zlsd_offset && h->target_subsong > h->total_subsongs)
        return true;

    int i;
    int table4_entry_id = -1;
    uint32_t table4_entry_idx, table4_entries_offset, table4_names_offset;
    uint32_t entry_offset, entry_count;
    uint32_t stream_name_offset;

    switch (h->sblk_version) {
        case 0x03:
            for (i = 0; i < h->sounds_entries; i++) {
                entry_offset = read_u32(h->table1_offset + (i * h->table1_entry_size) + 0x08, sf);
                entry_count = read_u8(h->table1_offset + (i * h->table1_entry_size) + 0x04, sf);

                /* is table2_entry_offset in the range of the expected section */
                if (h->table2_entry_offset >= entry_offset && h->table2_entry_offset < entry_offset + (entry_count * 0x08)) {
                    table4_entry_id = i;
                    break;
                }
            }

            /* table4:
             * 0x00: bank name (optional)
             * 0x08: name section offset
             * 0x0C-0x18: 3 null pointers (reserved?)
             * 0x18-0x58: 32 name chunk offset indices
             */

            /* Name chunks are organised as
             *  (name[0] + name[4] + name[8] + name[12]) % 32;
             * and using that as the index for the chunk offsets
             *  name_sect_offset + (chunk_idx[result] * 0x14);
             */
            read_string(h->bank_name, STREAM_NAME_SIZE, h->table4_offset, sf);

            table4_entries_offset = h->table4_offset + 0x18;
            table4_names_offset = h->table4_offset + read_u32(h->table4_offset + 0x08, sf);

            for (i = 0; i < 32; i++) {
                table4_entry_idx = read_u16(table4_entries_offset + (i * 2), sf);
                stream_name_offset = table4_names_offset + (table4_entry_idx * 0x14);
                /* searches the chunk until it finds the target name/index, or breaks at empty name */
                while (read_u8(stream_name_offset, sf)) {
                    /* in case it goes somewhere out of bounds unexpectedly */
                    if (((read_u8(stream_name_offset + 0x00, sf)
                        + read_u8(stream_name_offset + 0x04, sf)
                        + read_u8(stream_name_offset + 0x08, sf)
                        + read_u8(stream_name_offset + 0x0C, sf)) & 0x1F) != i)
                        goto fail;
                    if (read_u16(stream_name_offset + 0x10, sf) == table4_entry_id) {
                        read_string(h->stream_name, STREAM_NAME_SIZE, stream_name_offset, sf);
                        goto loop_break; /* to break out of the for+while loop simultaneously */
                        //break;
                    }
                    stream_name_offset += 0x14;
                }
            }
            goto fail; /* didn't find any valid index? */
        loop_break:
            break;

        case 0x04:
        case 0x05:
            /* a mix of v3 table1 parsing + v8-v16 table4 parsing */
            for (i = 0; i < h->sounds_entries; i++) {
                entry_offset = read_u32(h->table1_offset + (i * h->table1_entry_size) + 0x08, sf);
                entry_count = read_u8(h->table1_offset + (i * h->table1_entry_size) + 0x04, sf);

                if (h->table2_entry_offset >= entry_offset && h->table2_entry_offset < entry_offset + (entry_count * 0x08)) {
                    table4_entry_id = i;
                    break;
                }
            }

            /* table4:
             * 0x00: bank name (optional)
             * 0x08: name entries offset
             * 0x0C: name section offset
             *
             * name entries offset:
             * 0x00: name offset in name section
             * 0x04: name hash(?)
             * 0x08: ? (2x int16)
             * 0x0C: section index (int16)
             */
            read_string(h->bank_name, STREAM_NAME_SIZE, h->table4_offset, sf);

            table4_entries_offset = h->table4_offset + read_u32(h->table4_offset + 0x08, sf);
            table4_names_offset = h->table4_offset + read_u32(h->table4_offset + 0x0C, sf);

            for (i = 0; i < h->sounds_entries; i++) {
                if (read_u16(table4_entries_offset + (i * 0x10) + 0x0C, sf) == table4_entry_id) {
                    stream_name_offset = table4_names_offset + read_u32(table4_entries_offset + (i * 0x10), sf);
                    read_string(h->stream_name, STREAM_NAME_SIZE, stream_name_offset, sf);
                    break;
                }
            }
            break;

        case 0x08:
        case 0x09:
        case 0x0c:
        case 0x0d:
        case 0x0e:
        case 0x0f:
        case 0x10:
            /* find if this sound has an assigned name in table1 */
            for (i = 0; i < h->sounds_entries; i++) {
                entry_offset = read_u32(h->table1_offset + (i * h->table1_entry_size) + h->table1_suboffset + 0x00, sf);

                /* rarely (ex. Polara sfx) one name applies to multiple materials,
                 * from current entry_offset to next entry_offset (section offsets should be in order) */
                if (entry_offset <= h->table2_entry_offset) {
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
            read_string(h->bank_name, STREAM_NAME_SIZE, h->table4_offset, sf);

            table4_entries_offset = h->table4_offset + read_u32(h->table4_offset + 0x08, sf);
            table4_names_offset = table4_entries_offset + (0x10 * h->sounds_entries);
            //;VGM_LOG("BNK: t4_entries=%lx, t4_names=%lx\n", table4_entries_offset, table4_names_offset);

            /* get assigned name from table4 names */
            for (i = 0; i < h->sounds_entries; i++) {
                int entry_id = read_u16(table4_entries_offset + (i * 0x10) + 0x0c, sf);
                if (entry_id == table4_entry_id) {
                    stream_name_offset = table4_names_offset + read_u32(table4_entries_offset + (i * 0x10) + 0x00, sf);
                    read_string(h->stream_name, STREAM_NAME_SIZE, stream_name_offset, sf);
                    break;
                }
            }
            break;

        default:
            break;
    }

    //;VGM_LOG("BNK: stream_offset=%x, stream_size=%x, stream_name_offset=%x\n", h->stream_offset, h->stream_size, h->stream_name_offset);

    return true;
fail:
    return false;
}

static void process_extradata_base(STREAMFILE* sf, bnk_header_t* h, uint32_t info_offset) {
    read_u32_t read_u32 = h->big_endian ? read_u32be : read_u32le;

    h->subtype = read_u32(info_offset + 0x00, sf); //maybe flags?
    // 0x04: type? always 1
    h->extradata_size = read_u32(info_offset + 0x08, sf);
    // 0x0c: null? (part of size?) */

    h->extradata_size += 0x10;
}

static void process_extradata_0x10_pcm_psx(STREAMFILE* sf, bnk_header_t* h, uint32_t info_offset) {
    read_u32_t read_u32 = h->big_endian ? read_u32be : read_u32le;
    read_s32_t read_s32 = h->big_endian ? read_s32be : read_s32le;

    h->num_samples  = read_s32(info_offset + 0x10, sf); // typically null, see rarely in v0x09 (PSX) and v0x1a~0x23 (PCM))
    h->channels     = read_u32(info_offset + 0x14, sf);
    h->loop_start   = read_s32(info_offset + 0x18, sf);
    h->loop_length  = read_s32(info_offset + 0x1c, sf);
}

static void process_extradata_0x14_atrac9(STREAMFILE* sf, bnk_header_t* h, uint32_t info_offset) {
    read_u32_t read_u32 = h->big_endian ? read_u32be : read_u32le;
    read_s32_t read_s32 = h->big_endian ? read_s32be : read_s32le;

    if (read_u32(info_offset + 0x08, sf) != 0x14) {
        vgm_logi("BNK: unexpected extradata size (report)\n");
        return;
    }

    // 0x08: extradata size (0x14)
    h->atrac9_info    = read_u32be(info_offset + 0x0c, sf);
    // 0x10: null?
    h->loop_length      = read_s32(info_offset + 0x14, sf);
    h->loop_start       = read_s32(info_offset + 0x18, sf); // *after* length unlike PCM/PSX
}

static void process_extradata_0x80_atrac9(STREAMFILE* sf, bnk_header_t* h, uint32_t info_offset) {
    read_u32_t read_u32 = h->big_endian ? read_u32be : read_u32le;
    read_s32_t read_s32 = h->big_endian ? read_s32be : read_s32le;

    if (read_u32(info_offset + 0x10, sf) != 0x80) {
        vgm_logi("BNK: unexpected extradata size (report)\n");
        return;
    }

    // 0x10: extradata size (0x80)
    h->atrac9_info    = read_u32be(info_offset + 0x14, sf);
    // 0x18: null?
    h->channels         = read_u32(info_offset + 0x1c, sf);
    // 0x20: null?
    h->loop_length      = read_s32(info_offset + 0x24, sf);
    h->loop_start       = read_s32(info_offset + 0x28, sf); // *after* length unlike PCM/PSX (confirmed in both raw and RIFF)
    // rest: padding
}

static void process_extradata_0x80_mpeg(STREAMFILE* sf, bnk_header_t* h, uint32_t info_offset) {
    read_s32_t read_s32 = h->big_endian ? read_s32be : read_s32le;

    // 0x00: mpeg version? (1)
    // 0x04: mpeg layer? (3)
    // 0x08: ? (related to frame size, 0xC0 > 0x40, 0x120 > 0x60)
    // 0x0c: sample rate
    // 0x10: mpeg layer? (3)
    // 0x14: mpeg version? (1)
    // 0x18: channels
    // 0x1c: frame size
    h->encoder_delay    = read_s32(info_offset + 0x20, sf);
    h->num_samples      = read_s32(info_offset + 0x24, sf);
    // 0x28: ?
    // 0x2c: ?
    // 0x30: 0?
    // 0x34: data size
    // rest: padding
}

/* data part: parse extradata before the codec */
static bool process_data(STREAMFILE* sf, bnk_header_t* h) {
    read_u32_t read_u32 = h->big_endian ? read_u32be : read_u32le;
    read_u64_t read_u64 = h->big_endian ? read_u64be : read_u64le;

    /* is currently working on ZLSD streams */
    if (h->zlsd_offset && h->target_subsong > h->total_subsongs)
        return true;

    h->start_offset = h->data_offset + h->stream_offset;

    switch(h->sblk_version) {
        case 0x01:
        case 0x03:
        case 0x04:
        case 0x05:
            h->channels = 1;

            /* early versions don't have PS-ADPCM size, could check next offset but it's all kind of loopy */
            if (h->sblk_version <= 0x03 && h->stream_size == 0 && (h->stream_flags & 0x80) == 0) {
                uint32_t offset;
                uint32_t max_offset = get_streamfile_size(sf);

                h->stream_size += 0x10;
                for (offset = h->data_offset + h->stream_offset + 0x10; offset < max_offset; offset += 0x10) {

                    /* beginning frame (if file loops won't have end frame) */
                    /* checking the entire 16 byte frame, as it is possible
                     * for just the first 8 bytes to be empty [Bully (PS2)] */
                    if (read_u64be(offset + 0x00, sf) == 0x00000000 && read_u64be(offset + 0x08, sf) == 0x00000000)
                        break;

                    h->stream_size += 0x10;

                    /* end frame */
                    if (read_u32be(offset + 0x00, sf) == 0x00077777 && read_u32be(offset + 0x04, sf) == 0x77777777)
                        break;
                }

                //;VGM_LOG("BNK: stream offset=%lx + %lx, new size=%x\n", h->data_offset, stream_offset, stream_size);
            }


            /* hack for PS3 files that use dual subsongs as stereo */
            if (h->total_subsongs == 2 && h->stream_size * 2 == h->data_size) {
                h->channels = 2;
                h->stream_size = h->stream_size * h->channels;
                h->total_subsongs = 1;
                h->start_offset -= h->stream_offset; /* also channels may be inverted [Fat Princess (PS3)] */
            }
            h->interleave = h->stream_size / h->channels;

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
            if ((h->stream_flags & 0x80) && h->sblk_version <= 4) {
                /* rare [Wipeout HD (PS3)-v3, EyePet (PS3)-v4] */
                h->codec = PCM16;
            }
            else if ((h->stream_flags & 0x1000) && h->sblk_version == 5) {
                /* Uncharted (PS3) */
                process_extradata_0x80_mpeg(sf, h, h->start_offset + 0x00);
                h->extradata_size = 0x80;

                h->codec = MPEG;
            }
            else {
                h->loop_flag = ps_find_loop_offsets(sf, h->start_offset, h->stream_size, h->channels, h->interleave, &h->loop_start, &h->loop_end);
                h->loop_flag = (h->stream_flags & 0x40); /* only applies to PS-ADPCM flags */

                h->codec = PSX;
            }

            //postdata_size = 0x10; /* last frame may be garbage */
            break;

        case 0x08:
        case 0x09:
            h->subtype = read_u32(h->start_offset+0x00,sf);
            h->extradata_size = read_u32(h->start_offset+0x04,sf); /* 0x14 for AT9, 0x10/0x18 for PCM, 0x90 for MPEG */
            h->extradata_size += 0x08;

            switch(h->subtype) {
                case 0x00000000:
                    h->channels = 1;
                    h->codec = PSX;
                    break;

                case 0x00000001: /* PCM 1ch */
                case 0x00000004: /* PCM 2ch */
                    h->channels = (h->subtype == 0x01) ? 1 : 2;
                    h->codec = PCM16;
                    break;

                case 0x00000002: /* ATRAC9 / MPEG 1ch */
                case 0x00000005: /* ATRAC9 / MPEG 2ch */
                    h->channels = (h->subtype == 0x02) ? 1 : 2;

                    if (h->big_endian) {
                        /* The Last of Us demo (PS3) (size 0x90) */
                        process_extradata_0x80_mpeg(sf, h, h->start_offset + 0x08);
                        h->codec = MPEG;
                    }
                    else {
                        /* Puyo Puyo Tetris (PS4) */
                        process_extradata_0x14_atrac9(sf, h, h->start_offset);
                        h->codec = ATRAC9;
                    }
                    break;

                default:
                    vgm_logi("BNK: unknown subtype %08x (report)\n", h->subtype);
                    goto fail;
            }
            break;

        case 0x0c:
            /* two different variants under the same version (SingStar Ultimate Party PS3 vs PS4) */
            process_extradata_base(sf, h, h->start_offset);
            if (h->big_endian) {
                switch (h->subtype) { /* PS3 */
                    case 0x00000000: /* PS-ADPCM 1ch */
                        process_extradata_0x10_pcm_psx(sf, h, h->start_offset);
                        h->codec = PSX;
                        break;

                    case 0x00000001: /* PCM 1ch */
                        process_extradata_0x10_pcm_psx(sf, h, h->start_offset);
                        h->codec = PCM16;
                        break;

                    case 0x00000003: /* MP3 2ch */
                        process_extradata_0x80_mpeg(sf, h, h->start_offset + 0x10);
                        h->channels = 2;
                        h->codec = MPEG;
                        break;

                    default:
                        vgm_logi("BNK: unknown v08+ subtype %08x (report)\n", h->subtype);
                        goto fail;
                }
            }
            else {
                switch (h->subtype) { /* PS4 */
                    case 0x00000000: /* PCM 1ch */
                    case 0x00000001: /* PCM 2ch */
                        process_extradata_0x10_pcm_psx(sf, h, h->start_offset);
                        h->codec = PCM16;
                        break;

                    case 0x00010000: /* PS-ADPCM 1ch (HEVAG?) */
                        process_extradata_0x10_pcm_psx(sf, h, h->start_offset);
                        h->codec = PSX;
                        break;

                    default:
                        vgm_logi("BNK: unknown v08+ subtype %08x (report)\n", h->subtype);
                        goto fail;
                }
            }

            break;

        case 0x0d:
        case 0x0e:
        case 0x0f:
        case 0x10:
            //TODO: in v0x0f/10 some codecs have the original filename right before this, see if offset can be found
            process_extradata_base(sf, h, h->start_offset);
            switch(h->subtype) {
                case 0x00000001: /* PCM16LE 1ch [NekoBuro/Polara (Vita)-v0d sfx] */
                case 0x00000004: /* PCM16LE 2ch [NekoBuro/Polara (Vita)-v0d sfx] */
                    process_extradata_0x10_pcm_psx(sf, h, h->start_offset);
                    h->codec = PCM16;
                    break;

                case 0x00000000: /* HEVAG 1ch [Hero Must Die (Vita)-v0d, v0d test banks) - likely standard VAG */
                case 0x00000003: /* HEVAG 2ch [Ikaruga (PS4)-v0f] */
                    process_extradata_0x10_pcm_psx(sf, h, h->start_offset);
                    h->codec = HEVAG;
                    break;

                case 0x00000002: /* ATRAC9 1ch [Crypt of the Necrodancer (Vita)-v0d] */
                case 0x00000005: /* ATRAC9 2ch [Crypt of the Necrodancer (Vita)-v0d, Ikaruga (PS4)-v0f] */
                    process_extradata_0x80_atrac9(sf, h, h->start_offset);
                    h->codec = ATRAC9;
                    break;

                case 0x00030000: /* ATRAC9 1ch [Days Gone (PS4)-v10] */
                case 0x00030001: /* ATRAC9 2ch [Days Gone (PS4)-v10] */
                case 0x00030002: /* ATRAC9 4ch [Days Gone (PS4)-v10] */
                    process_extradata_0x80_atrac9(sf, h, h->start_offset);
                    h->codec = RIFF_ATRAC9;
                    break;
    
                default:
                    vgm_logi("BNK: unknown v0d+ subtype %08x (report)\n", h->subtype);
                    goto fail;
            }
            break;

        case 0x1a:
        case 0x1c:
        case 0x23: {
            // common [Demon's Souls (PS5), The Last of Us Part 2 (PC)]
            if (h->stream_offset == 0xFFFFFFFF) {
                h->channels = 1;
                h->codec = DUMMY;
                break;
            }

            /* pre-header with string + size */
            uint32_t info_offset = h->start_offset;

            uint32_t stream_name_size   = read_u64(info_offset + 0x00,sf);
            uint32_t stream_name_offset = info_offset + 0x08;
            info_offset += stream_name_size + 0x08;

            if (stream_name_size >= STREAM_NAME_SIZE || stream_name_size <= 0)
                stream_name_size = STREAM_NAME_SIZE;
            read_string(h->stream_name, stream_name_size, stream_name_offset, sf);

            h->stream_size = read_u64(info_offset + 0x00,sf); /* after this offset */
            h->stream_size += 0x08 + stream_name_size + 0x08;
            info_offset += 0x08;

            // size check is necessary, otherwise it risks a false positive with the ZLSD version number
            // (using 0x01 flag to detect whether it's an SBlk or ZLSD/exteral sound for now)
            if (info_offset + 0x08 > h->data_offset + h->data_size || read_u32(info_offset + 0x04, sf) != 0x01) {
                h->channels = 1;
                h->codec = EXTERNAL;
                break;
            }

            /* actual stream info;  */
            process_extradata_base(sf, h, info_offset);
            h->extradata_size += 0x08 + stream_name_size + 0x08;

            // lower 16bit: 0/1 for PCM, 0/1/2/3 for ATRAC9 (possibly channels / 2)
            switch (h->subtype >> 16) {
                case 0x0000: /* PCM [The Last of Us Part 1 (PC)-v23] */
                    process_extradata_0x10_pcm_psx(sf, h, info_offset);
                    h->codec = PCM16;
                    break;

                case 0x0003: /* ATRAC9 [The Last of Us Part 2 (PC)-v1c, Demon's Souls (PS5)-v1a] */
                case 0x0001: /* ATRAC9 [The Last of Us Part 1 (PC)-v23] */
                    process_extradata_0x80_atrac9(sf, h, info_offset);
                    h->codec = RIFF_ATRAC9;
                    break;

                default:
                    vgm_logi("BNK: unknown v1c+ subtype %08x (report)\n", h->subtype);
                    goto fail;
            }

            break;
        }

        default:
            vgm_logi("BNK: unknown data version %x (report)\n", h->sblk_version);
            goto fail;
    }

    h->start_offset += h->extradata_size;
    h->stream_size -= h->extradata_size;
    h->stream_size -= h->postdata_size;
    //;VGM_LOG("BNK: offset=%x, size=%x\n", h->start_offset, h->stream_size);

    // loop_start is typically -1 if not set
    if (h->loop_start < 0) {
        h->loop_start = 0;
        h->loop_length = 0;
    } 

    if (h->loop_length) {
        h->loop_end = h->loop_start + h->loop_length;
    }

    h->loop_flag = (h->loop_start >= 0) && (h->loop_end > 0);

    return true;
fail:
    return false;
}

/* zlsd part: parse external stream prefetch data */
static bool process_zlsd(STREAMFILE* sf, bnk_header_t* h) {
    if (!h->zlsd_offset)
        return true;

    /* TODO:  ZLSD contains FNV1-32 hashes of the SBlk external streams,
     * but with the way it's all currently set up, it isn't as simple to
     * map appropriate hashes to existing SBlk streams. So for now these
     * won't have a "proper" stream name visible.
     */

    int zlsd_subsongs, target_subsong;
    uint32_t zlsd_table_offset, zlsd_table_entry_offset, stream_name_hash;
    read_u32_t read_u32 = h->big_endian ? read_u32be : read_u32le;

    if (read_u32(h->zlsd_offset + 0x00, sf) != get_id32be("DSLZ"))
        return false;

    /* 0x04: version? (1) */
    zlsd_subsongs = read_u32(h->zlsd_offset + 0x08, sf);
    /* 0x0c: start (most of the time) */
    /* 0x10: start if 64-bit zlsd_subsongs? seen in SBlk 0x1A/0x1C */
    zlsd_table_offset = read_u32(h->zlsd_offset + 0x0C, sf);
    /* rest: null */

    /* files can have both SBlk+ZLSD streams */
    if (zlsd_subsongs < 1) {
        if (h->total_subsongs < 1)
            goto fail;
        return true;
    }

    if (!zlsd_table_offset)
        goto fail; /* 64-bit entries count? */

    /* per entry (for SBlk v0x23)
     * 00: fnv1-32 hash of the stream name
     * 04: stream offset (from this offset)
     * 08: null (part of offset?)
     * 0c: stream size
     * 10: offset/size?
     * 14/18: null */
    /* known streams are standard XVAG (no subsongs) */

    /* target_subsong is negative if it's working on SBlk streams */
    target_subsong = h->target_subsong - h->total_subsongs - 1;
    h->total_subsongs += zlsd_subsongs;

    if (h->target_subsong < 0 || h->target_subsong > h->total_subsongs)
        goto fail;

    if (target_subsong < 0)
        return true;

    zlsd_table_entry_offset = h->zlsd_offset + zlsd_table_offset + target_subsong * 0x18;
    h->start_offset = zlsd_table_entry_offset + 0x04 + read_u32(zlsd_table_entry_offset + 0x04, sf);
    h->stream_size = read_u32(zlsd_table_entry_offset + 0x0C, sf);
    stream_name_hash = read_u32(zlsd_table_entry_offset + 0x00, sf);

    /* should be a switch case, but no other formats known yet */
    if (!is_id32be(h->start_offset, sf, "XVAG")) {
        vgm_logi("BNK: unsupported ZLSD subfile found (report)\n");
        goto fail;
    }

    snprintf(h->stream_name, STREAM_NAME_SIZE, "%u [pre]", stream_name_hash);
    h->channels = 1; /* dummy, real channels will be retrieved from xvag/riff */
    h->codec = XVAG_ATRAC9;

    return true;
fail:
    return false;
}


/* parse SCREAM bnk (usually SFX but also used for music) */
static bool parse_bnk_v3(STREAMFILE* sf, bnk_header_t* h) {

    /* bnk/SCREAM tool version (v2 is a bit different, not seen v1) */
    if (read_u32be(0x00,sf) == 0x03) { /* PS3 */
        h->big_endian = 1;
    }
    else if (read_u32le(0x00,sf) == 0x03) { /* PS2/PSP/Vita/PS4/PS5 */
        h->big_endian = 0;
    }
    else {
        return false;
    }

    read_u32_t read_u32 = h->big_endian ? read_u32be : read_u32le;

    int sections = read_u32(0x04,sf); /* SBlk, data, ZLSD */
    if (sections < 2 || sections > 3)
        return false;
    /* in theory a bank can contain multiple blocks but only those are used */

    /* file is sometimes aligned to 0x10/0x800, so this can't be used for total size checks */
    h->sblk_offset = read_u32(0x08,sf);
    //h->sblk_size = read_u32(0x0c,sf);
    h->data_offset = read_u32(0x10,sf);
    h->data_size   = read_u32(0x14,sf);
    /* ZLSD footer, rare in earlier versions and common later (often empty) [Yakuza 6's Puyo Puyo (PS4)] */
    if (sections >= 3) {
        h->zlsd_offset = read_u32(0x18,sf);
        h->zlsd_size   = read_u32(0x1c,sf);
    }

    if (h->sblk_offset > 0x20)
        return false;

    /* Most table fields seems reserved/defaults and don't change much between subsongs or files,
     * so they aren't described in detail. Entry sizes are variable (usually flag + extra size xN)
     * so table offsets are needed. */


    /* SBlk part: parse header */
    if (read_u32(h->sblk_offset+0x00,sf) != get_id32be("klBS")) /* SBlk = SFX block */
        return false;
    h->sblk_version = read_u32(h->sblk_offset+0x04,sf);
    /* 0x08: flags? (h->sblk_version>=0x0d?, 0x03=Vita, 0x06=PS4, 0x05=PS5, 0x07=PS5)
     * - 04: non-fixed bank?
     * - 100: 'has names'
     * - 200: 'has user data' */
    /* version < v0x1a:
     * - 0x0c: block id
     * - 0x10: block number
     * - 0x11: padding
     * version >= v0x1a:
     * - 0x0c: uuid (0x10)
     * - 0x1c: bank name (0x100?)
     * version ~= v0x23:
     * - 0x0c: null (depends on flags? v1a=0x05, v23=0x07)
     * - 0x10: uuid (0x10)
     * - 0x20: bank name (0x100?)
     */
    //;VGM_LOG("BNK: h->sblk_offset=%lx, h->data_offset=%lx, h->sblk_version %x\n", h->sblk_offset, h->data_offset, h->sblk_version);
    //TODO handle, in rare cases may contain subsongs (unsure how are referenced but has its own number)

    if (!process_tables(sf, h))
        goto fail;
    if (!process_headers(sf, h))
        goto fail;
    if (!process_names(sf, h))
        goto fail;
    if (!process_data(sf, h))
        goto fail;
    if (!process_zlsd(sf, h))
        goto fail;

    return true;
fail:
    return false;
}
