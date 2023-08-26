#include <math.h>
#include "meta.h"
#include "../coding/coding.h"
#include "../util/endianness.h"

typedef enum { NONE, DUMMY, PSX, PCM16, ATRAC9, HEVAG, RIFF_ATRAC9 } bnk_codec;

typedef struct {
    bnk_codec codec;
    int big_endian;

    /* bank related (internal)*/
    int sblk_version;
    uint32_t sblk_offset;
    uint32_t data_offset;
    uint32_t data_size;
    uint32_t zlsd_offset;
    uint32_t zlsd_size;

    uint32_t table1_offset; /* usually sounds/cues (point to grains) */
    uint32_t table2_offset; /* usually grains/materials (point to waves) */
    uint32_t table3_offset; /* usually waves (point to streams) */
    uint32_t table4_offset; /* usually names */
    uint32_t sounds_entries;
    uint32_t grains_entries;
    uint32_t stream_entries;
    uint32_t table1_suboffset;
    uint32_t table2_suboffset;
    uint32_t table1_entry_size;
    uint32_t table2_entry_offset;
    uint32_t table3_entry_offset;


    /* stream related */
    int total_subsongs;
    int target_subsong;

    int channels;
    int loop_flag;
    int sample_rate;
    int32_t num_samples;
    int32_t loop_start;
    int32_t loop_end;

    uint32_t start_offset;
    uint32_t stream_offset;
    uint32_t bank_name_offset;
    uint32_t stream_name_offset;
    uint32_t stream_name_size;

    uint32_t stream_size;
    uint32_t interleave;

    uint16_t stream_flags;
    uint32_t atrac9_info;
} bnk_header_t;

static bool parse_bnk_v3(STREAMFILE* sf, bnk_header_t* h);


/* .BNK - Sony's SCREAM bank format [The Sly Collection (PS3), Puyo Puyo Tetris (PS4), NekoBuro: Cats Block (Vita)] */
VGMSTREAM* init_vgmstream_bnk_sony(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    char bank_name[STREAM_NAME_SIZE] /*[8]*/, stream_name[STREAM_NAME_SIZE] /*[16]*/;
    bnk_header_t h = {0};

    /* checks */
    if (!parse_bnk_v3(sf, &h))
        return NULL;
    if (!check_extensions(sf, "bnk"))
        return NULL;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(h.channels, h.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = h.sample_rate;
    vgmstream->num_streams = h.total_subsongs;
    vgmstream->stream_size = h.stream_size;

    vgmstream->meta_type = meta_BNK_SONY;

    if (!h.stream_name_size)
        h.stream_name_size = STREAM_NAME_SIZE;

    if (!h.bank_name_offset && h.stream_name_offset) {
        read_string(vgmstream->stream_name, h.stream_name_size, h.stream_name_offset, sf);
    }
    else if (h.bank_name_offset && h.stream_name_offset) {
        read_string(bank_name, h.stream_name_size, h.bank_name_offset, sf);
        read_string(stream_name, h.stream_name_size, h.stream_name_offset, sf);
        snprintf(vgmstream->stream_name, h.stream_name_size, "%s/%s", bank_name, stream_name);
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
#endif
        case PCM16:
            vgmstream->coding_type = h.big_endian ? coding_PCM16BE : coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = h.interleave;

            vgmstream->num_samples = pcm_bytes_to_samples(h.stream_size, vgmstream->channels, 16);
            vgmstream->loop_start_sample = h.loop_start;
            vgmstream->loop_end_sample = h.loop_end;
            break;

        case PSX:
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = h.interleave;

            vgmstream->num_samples = ps_bytes_to_samples(h.stream_size, h.channels);
            vgmstream->loop_start_sample = h.loop_start;
            vgmstream->loop_end_sample = h.loop_end;
            break;

        case HEVAG:
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

static const uint16_t note_pitch_table[12] = {
    0x8000, 0x879C, 0x8FAC, 0x9837, 0xA145, 0xAADC,
    0xB504, 0xBFC8, 0xCB2F, 0xD744, 0xE411, 0xF1A1
};

static const uint16_t fine_pitch_table[128] = {
    0x8000, 0x800E, 0x801D, 0x802C, 0x803B, 0x804A, 0x8058, 0x8067,
    0x8076, 0x8085, 0x8094, 0x80A3, 0x80B1, 0x80C0, 0x80CF, 0x80DE,
    0x80ED, 0x80FC, 0x810B, 0x811A, 0x8129, 0x8138, 0x8146, 0x8155,
    0x8164, 0x8173, 0x8182, 0x8191, 0x81A0, 0x81AF, 0x81BE, 0x81CD,
    0x81DC, 0x81EB, 0x81FA, 0x8209, 0x8218, 0x8227, 0x8236, 0x8245,
    0x8254, 0x8263, 0x8272, 0x8282, 0x8291, 0x82A0, 0x82AF, 0x82BE,
    0x82CD, 0x82DC, 0x82EB, 0x82FA, 0x830A, 0x8319, 0x8328, 0x8337,
    0x8346, 0x8355, 0x8364, 0x8374, 0x8383, 0x8392, 0x83A1, 0x83B0,
    0x83C0, 0x83CF, 0x83DE, 0x83ED, 0x83FD, 0x840C, 0x841B, 0x842A,
    0x843A, 0x8449, 0x8458, 0x8468, 0x8477, 0x8486, 0x8495, 0x84A5,
    0x84B4, 0x84C3, 0x84D3, 0x84E2, 0x84F1, 0x8501, 0x8510, 0x8520,
    0x852F, 0x853E, 0x854E, 0x855D, 0x856D, 0x857C, 0x858B, 0x859B,
    0x85AA, 0x85BA, 0x85C9, 0x85D9, 0x85E8, 0x85F8, 0x8607, 0x8617,
    0x8626, 0x8636, 0x8645, 0x8655, 0x8664, 0x8674, 0x8683, 0x8693,
    0x86A2, 0x86B2, 0x86C1, 0x86D1, 0x86E0, 0x86F0, 0x8700, 0x870F,
    0x871F, 0x872E, 0x873E, 0x874E, 0x875D, 0x876D, 0x877D, 0x878C
};

static uint16_t ps_note_to_pitch(uint16_t center_note, uint16_t center_fine, uint16_t note, int16_t fine) {
    /* Derived from OpenGOAL, Copyright (c) 2020-2022 OpenGOAL Team, ISC License
     *
     * Permission to use, copy, modify, and/or distribute this software for any
     * purpose with or without fee is hereby granted, provided that the above
     * copyright notice and this permission notice appear in all copies.
     *
     * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
     * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
     * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
     * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
     * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
     * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
     * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
     */

    int fine_adjust, fine_idx, note_adjust, note_idx;
    int unk1, unk2, unk3; /* TODO: better variable names */
    uint16_t pitch;

    fine_idx = fine + center_fine;

    fine_adjust = fine_idx;
    if (fine_idx < 0)
        fine_adjust = fine_idx + 0x7F;

    fine_adjust /= 128;
    note_adjust = note + fine_adjust - center_note;
    unk3 = note_adjust / 6;

    if (note_adjust < 0)
        unk3--;

    fine_idx -= fine_adjust * 128;

    if (note_adjust < 0)
        unk2 = -1;
    else
        unk2 = 0;
    if (unk3 < 0)
        unk3--;

    unk2 = (unk3 / 2) - unk2;
    unk1 = unk2 - 2;
    note_idx = note_adjust - (unk2 * 12);

    if ((note_idx < 0) || ((note_idx == 0) && (fine_idx < 0))) {
        note_idx += 12;
        unk1 = unk2 - 3;
    }

    if (fine_idx < 0) {
        note_idx = (note_idx - 1) + fine_adjust;
        fine_idx += (fine_adjust + 1) * 128;
    }

    pitch = (note_pitch_table[note_idx] * fine_pitch_table[fine_idx]) >> 16;

    if (unk1 < 0)
        pitch = (pitch + (1 << (-unk1 - 1))) >> -unk1;

    return pitch;
}


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
        case 0x01: /* Ratchet & Clank (PS2) */
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
        case 0x04: /* Test banks */
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

        /* later version have a few more tables (some optional) and work slightly differently (header is part of wave) */
        case 0x1a: /* Demon's Souls (PS5) */
        case 0x23: { /* The Last of Us (PC) */
            uint32_t tables_offset = h->sblk_offset + (h->sblk_version <= 0x1a ? 0x120 : 0x128);
            uint32_t counts_offset = tables_offset + (h->sblk_version <= 0x1a ? 0x98 : 0xb0);

          //h->table1_offset    = h->sblk_offset + read_u32(tables_offset+0x00,sf); /* sounds/cues */
          //h->table2_offset    = 0;
            h->table3_offset    = h->sblk_offset + read_u32(tables_offset+0x08,sf); /* wave offsets with info (integrated grains+waves?)*/
          //h->sounds_entries   = read_u16(counts_offset+0x00,sf);
          //h->grains_entries   = read_u16(counts_offset+0x02,sf);
            h->stream_entries   = read_u16(counts_offset+0x06,sf);
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
    int i;
    uint32_t sndh_offset;

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

            for (i = 0; i < h->grains_entries; i++) {
                uint32_t table2_type = read_u32(h->table2_offset + (i*0x28) + 0x00, sf);

                if (table2_type != 0x01)
                    continue;

                h->total_subsongs++;
                if (h->total_subsongs == h->target_subsong) {
                    h->table2_entry_offset = 0;
                    h->table3_entry_offset = (i*0x28) + 0x08;
                    /* continue to count all subsongs */
                }

            }

            break;

        case 0x1a:
        case 0x23:
            h->total_subsongs = h->stream_entries;
            h->table3_entry_offset = (h->target_subsong - 1) * 0x08;
            break;

        default:
            for (i = 0; i < h->grains_entries; i++) {
                uint32_t table2_value, table2_subinfo, table2_subtype;

                table2_value = read_u32(h->table2_offset+(i*0x08) + h->table2_suboffset + 0x00,sf);
                table2_subinfo = (table2_value >>  0) & 0xFFFF;
                table2_subtype = (table2_value >> 16) & 0xFFFF;
                if (table2_subtype != 0x0100)
                    continue; /* not sounds (ex. 1: waveform, 42: silence, 25: random, etc) */

                h->total_subsongs++;
                if (h->total_subsongs == h->target_subsong) {
                    h->table2_entry_offset = (i*0x08);
                    h->table3_entry_offset = table2_subinfo;
                    /* continue to count all subsongs */
                }
            }

            break;
    }


    //;VGM_LOG("BNK: subsongs %i, table2_entry=%x, table3_entry=%x\n", h->total_subsongs, h->table2_entry_offset, h->table3_entry_offset);
    if (h->target_subsong < 0 || h->target_subsong > h->total_subsongs || h->total_subsongs < 1)
        goto fail;
    /* this means some subsongs repeat streams, that can happen in some sfx banks, whatevs */
    if (h->total_subsongs != h->stream_entries) {
        VGM_LOG("BNK: subsongs %i vs table3 %i don't match (repeated streams?)\n", h->total_subsongs, h->stream_entries);
        /* TODO: find dupes?  */
    }

    //;VGM_LOG("BNK: header entry at %x\n", h->table3_offset + h->table3_entry_offset);

    sndh_offset = h->table3_offset + h->table3_entry_offset;

    /* parse sounds */
    switch(h->sblk_version) {
        case 0x01:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x08:
        case 0x09: {
            uint16_t center_note, center_fine, pitch;
            bool is_negative;

            /* "tone" */
            /* 0x00: priority */
            /* 0x01: volume */
            center_note     = read_u8 (sndh_offset + 0x02,sf);
            center_fine     = read_u8 (sndh_offset + 0x03,sf);
            /* 0x04: pan */
            /* 0x06: map low */
            /* 0x07: map high */
            /* 0x08: pitch bend low */
            /* 0x09: pitch bend high */
            /* 0x0a: ADSR1 */
            /* 0x0c: ADSR2 */
            h->stream_flags     = read_u16(sndh_offset + 0x0e,sf);
            h->stream_offset    = read_u32(sndh_offset + 0x10,sf);
            h->stream_size      = read_u32(sndh_offset + 0x14,sf);

            /* if it isn't, then it's treated as 44100 base? (PS1?) */
            is_negative = center_note >> 7; /* center_note & 0x80; */

            if (is_negative)
                center_note = 0x100 - center_note;

            /* note/fine seems to always be set to 0x3C/0x00 */
            pitch = ps_note_to_pitch(center_note, center_fine, 0x3C, 0x00);

            if (pitch > 0x4000)
                pitch = 0x4000; /* 192000 Hz max */

            if (!is_negative) /* PS1 mode? */
                pitch = (pitch * 44100) / 48000;

            h->sample_rate = (pitch * 48000) / 0x1000;

            /* waves can set base sample rate (48/44/22/11/8khz) + pitch in semitones, then converted to center+fine
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

        case 0x0d:
        case 0x0e:
        case 0x0f:
        case 0x10:
            h->stream_flags     = read_u8 (sndh_offset+0x12,sf);
            h->stream_offset    = read_u32(sndh_offset+0x44,sf);
            h->stream_size      = read_u32(sndh_offset+0x48,sf);
            h->sample_rate = (int)read_f32(sndh_offset+0x4c,sf);
            break;

        case 0x1a: /* Demon's Souls (PS5) */
        case 0x23: /* The Last of Us (PC) */
            h->stream_offset     = read_u32(sndh_offset+0x00,sf);
            /* rest is part of data, handled later */
            break;

        default:
            VGM_LOG("BNK: missing version\n");
            goto fail;
    }

    //;VGM_LOG("BNK: stream at %lx + %x\n", h->stream_offset, h->stream_size);

    return true;
fail:
    return false;
}

/* name part: read names  */
static bool process_names(STREAMFILE* sf, bnk_header_t* h) {
    read_u16_t read_u16 = h->big_endian ? read_u16be : read_u16le;
    read_u32_t read_u32 = h->big_endian ? read_u32be : read_u32le;

    /* table4 can be nonexistent */
    if (h->table4_offset <= h->sblk_offset)
        return true;

    int i;
    int table4_entry_id = -1;
    uint32_t table4_entry_idx, table4_entries_offset, table4_names_offset;
    uint32_t entry_offset, entry_count;

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
                * 0x0C-0x14: 3 null pointers (reserved?)
                * 0x18-0x58: 32 name chunk offset indices
                */

            /* Name chunks are organised as
                *  (name[0] + name[4] + name[8] + name[12]) & 0x1F;
                * and using that as the index for the chunk offsets
                *  name_sect_offset + (chunk_idx[result] * 0x14);
                */
            if (read_u8(h->table4_offset, sf))
                h->bank_name_offset = h->table4_offset;

            table4_entries_offset = h->table4_offset + 0x18;
            table4_names_offset = h->table4_offset + read_u32(h->table4_offset + 0x08, sf);

            for (i = 0; i < 32; i++) {
                table4_entry_idx = read_u16(table4_entries_offset + (i * 2), sf);
                h->stream_name_offset = table4_names_offset + (table4_entry_idx * 0x14);
                /* searches the chunk until it finds the target name/index, or breaks at empty name */
                while (read_u8(h->stream_name_offset, sf)) {
                    /* in case it goes somewhere out of bounds unexpectedly */
                    if (((read_u8(h->stream_name_offset + 0x00, sf) + read_u8(h->stream_name_offset + 0x04, sf) +
                            read_u8(h->stream_name_offset + 0x08, sf) + read_u8(h->stream_name_offset + 0x0C, sf)) & 0x1F) != i)
                        goto fail;
                    if (read_u16(h->stream_name_offset + 0x10, sf) == table4_entry_id)
                        goto loop_break; /* to break out of the for+while loop simultaneously */
                        //break;
                    h->stream_name_offset += 0x14;
                }
            }
            //goto fail; /* didn't find any valid index? */
            h->stream_name_offset = 0;
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
            if (read_u8(h->table4_offset, sf))
                h->bank_name_offset = h->table4_offset;

            table4_entries_offset = h->table4_offset + read_u32(h->table4_offset + 0x08, sf);
            table4_names_offset = h->table4_offset + read_u32(h->table4_offset + 0x0C, sf);

            for (i = 0; i < h->sounds_entries; i++) {
                if (read_u16(table4_entries_offset + (i * 0x10) + 0x0C, sf) == table4_entry_id) {
                    h->stream_name_offset = table4_names_offset + read_u32(table4_entries_offset + (i * 0x10), sf);
                    break;
                }
            }
            break;

        case 0x08:
        case 0x09:
        case 0x0d:
        case 0x0e:
        case 0x0f:
        case 0x10:
            /* find if this sound has an assigned name in table1 */
            for (i = 0; i < h->sounds_entries; i++) {
                entry_offset = read_u16(h->table1_offset + (i * h->table1_entry_size) + h->table1_suboffset + 0x00, sf);

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
            if (read_u8(h->table4_offset, sf))
                h->bank_name_offset = h->table4_offset;

            table4_entries_offset = h->table4_offset + read_u32(h->table4_offset + 0x08, sf);
            table4_names_offset = table4_entries_offset + (0x10 * h->sounds_entries);
            //;VGM_LOG("BNK: t4_entries=%lx, t4_names=%lx\n", table4_entries_offset, table4_names_offset);

            /* get assigned name from table4 names */
            for (i = 0; i < h->sounds_entries; i++) {
                int entry_id = read_u32(table4_entries_offset + (i * 0x10) + 0x0c, sf);
                if (entry_id == table4_entry_id) {
                    h->stream_name_offset = table4_names_offset + read_u32(table4_entries_offset + (i * 0x10) + 0x00, sf);
                    break;
                }
            }
            break;

        default:
            break;
    }

    //;VGM_LOG("BNK: stream_offset=%lx, stream_size=%x, stream_name_offset=%lx\n", h->stream_offset, h->stream_size, h->stream_name_offset);

    return true;
fail:
    return false;
}

/* data part: parse extradata before the codec */
static bool process_data(STREAMFILE* sf, bnk_header_t* h) {
    read_u16_t read_u16 = h->big_endian ? read_u16be : read_u16le;
    read_u32_t read_u32 = h->big_endian ? read_u32be : read_u32le;
    read_s32_t read_s32 = h->big_endian ? read_s32be : read_s32le;
    read_u64_t read_u64 = h->big_endian ? read_u64be : read_u64le;

    int subtype, loop_length;
    uint32_t extradata_size = 0, postdata_size = 0;

    h->start_offset = h->data_offset + h->stream_offset;
    uint32_t info_offset = h->start_offset;

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

                    /* beginning frame (if file loops won't have end frame)
                        * checking the entire 16 byte block, as it is possible
                        * for just the first 8 bytes to be empty [Bully (PS2)] */
                    if (read_u32be(offset + 0x00, sf) == 0x00000000 && read_u32be(offset + 0x04, sf) == 0x00000000 &&
                        read_u32be(offset + 0x08, sf) == 0x00000000 && read_u32be(offset + 0x0C, sf) == 0x00000000)
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
            if ((h->stream_flags & 0x80) && h->sblk_version <= 3) {
                h->codec = PCM16; /* rare [Wipeout HD (PS3)]-v3 */
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
            subtype = read_u16(h->start_offset+0x00,sf);
            extradata_size = 0x08 + read_u32(h->start_offset+0x04,sf); /* 0x14 for AT9 */

            switch(subtype) {
                case 0x00:
                    h->channels = 1;
                    h->codec = PSX;
                    h->interleave = 0x10;
                    break;

                case 0x01:
                    h->channels = 1;
                    h->codec = PCM16;
                    h->interleave = 0x01;
                    break;


                case 0x02: /* ATRAC9 mono */
                case 0x05: /* ATRAC9 stereo */
                    if (read_u32(h->start_offset+0x08,sf) + 0x08 != extradata_size) { /* repeat? */
                        VGM_LOG("BNK: unknown subtype\n");
                        goto fail;
                    }
                    h->channels = (subtype == 0x02) ? 1 : 2;

                    h->atrac9_info = read_u32be(h->start_offset+0x0c,sf);
                    /* 0x10: null? */
                    loop_length    = read_u32(h->start_offset+0x14,sf);
                    h->loop_start  = read_u32(h->start_offset+0x18,sf);
                    h->loop_end = h->loop_start + loop_length; /* loop_start is -1 if not set */

                    h->codec = ATRAC9;
                    break;

                default:
                    vgm_logi("BNK: unknown subtype %x (report)\n", subtype);
                    goto fail;
            }
            break;

        case 0x0d:
        case 0x0e:
        case 0x0f:
        case 0x10:
            subtype = read_u16(h->start_offset+0x00,sf);
            if (read_u32(h->start_offset+0x04,sf) != 0x01) { /* type? */
                VGM_LOG("BNK: unknown subtype\n");
                goto fail;
            }
            extradata_size = 0x10 + read_u32(h->start_offset+0x08,sf); /* 0x80 for AT9, 0x10 for PCM/PS-ADPCM */
            /* 0x0c: null? */

            switch(subtype) {
                case 0x02: /* ATRAC9 mono */
                case 0x05: /* ATRAC9 stereo */
                    if (read_u32(h->start_offset+0x10,sf) + 0x10 != extradata_size) /* repeat? */
                        goto fail;
                    h->channels = (subtype == 0x02) ? 1 : 2;

                    h->atrac9_info = read_u32be(h->start_offset+0x14,sf);
                    /* 0x18: null? */
                    /* 0x1c: channels? */
                    /* 0x20: null? */

                    loop_length   = read_u32(h->start_offset+0x24,sf);
                    h->loop_start = read_u32(h->start_offset+0x28,sf);
                    h->loop_end = h->loop_start + loop_length; /* loop_start is -1 if not set */

                    h->codec = ATRAC9;
                    break;

                case 0x01: /* PCM16LE mono? (NekoBuro/Polara sfx) */
                case 0x04: /* PCM16LE stereo? (NekoBuro/Polara sfx) */
                    /* 0x10: null? */
                    h->channels = read_u32(h->start_offset+0x14,sf);
                    h->interleave = 0x02;

                    h->loop_start = read_u32(h->start_offset+0x18,sf);
                    loop_length   = read_u32(h->start_offset+0x1c,sf);
                    h->loop_end = h->loop_start + loop_length; /* loop_start is -1 if not set */

                    h->codec = PCM16;
                    break;

                case 0x00: /* HEVAG (test banks) */
                case 0x03: /* HEVAG (Ikaruga) */
                    /* 0x10: null? */
                    h->channels = read_u32(h->start_offset+0x14,sf);
                    h->interleave = 0x10;

                    h->loop_start = read_u32(h->start_offset+0x18,sf);
                    loop_length   = read_u32(h->start_offset+0x1c,sf);
                    h->loop_end = h->loop_start + loop_length; /* loop_start is -1 if not set */

                    h->codec = HEVAG;
                    //TODO: in v0x0f right before start_offset is the .vag filename, see if offset can be found
                    break;

                default:
                    vgm_logi("BNK: unknown subtype %x (report)\n", subtype);
                    goto fail;
            }
            break;

        case 0x1a:
        case 0x23:
            if (h->stream_offset == 0xFFFFFFFF) {
                h->channels = 1;
                h->codec = DUMMY;
                break;
            }

            /* pre-info */
            h->stream_name_size   = read_u64(info_offset+0x00,sf);
            h->stream_name_offset = info_offset + 0x08;
            info_offset += h->stream_name_size + 0x08;

            h->stream_size = read_u64(info_offset + 0x00,sf); /* after this offset */
            h->stream_size += 0x08 + h->stream_name_size + 0x08;
            /* 0x08: max block/etc size? (0x00010000/00030000) */
            /* 0x0c: always 1? */
            extradata_size = read_u64(info_offset + 0x10,sf) + 0x08 + h->stream_name_size + 0x18;
            info_offset += 0x18;

            /* actual stream info */
            /* 0x00: extradata size (without pre-info, also above) */
            h->atrac9_info = read_u32be(info_offset+0x04,sf);
            h->num_samples  = read_s32(info_offset+0x08,sf);
            h->channels     = read_u32(info_offset+0x0c,sf);
            h->loop_start   = read_s32(info_offset+0x10,sf);
            h->loop_end     = read_s32(info_offset+0x14,sf);
            /* 0x18: loop flag (0=loop, -1=no) */
            /* rest: null */
            /* no sample rate (probably fixed to 48000/system's, but seen in RIFF) */
            h->sample_rate = 48000;

            h->codec = RIFF_ATRAC9; /* unsure how other codecs would work */
            break;

        default:
            vgm_logi("BNK: unknown data version %x (report)\n", h->sblk_version);
            goto fail;
    }

    h->start_offset += extradata_size;
    h->stream_size -= extradata_size;
    h->stream_size -= postdata_size;
    //;VGM_LOG("BNK: offset=%x, size=%x\n", h->start_offset, h->stream_size);

    return true;
fail:
    return false;
}


/* zlsd part: parse extra footer (vox?) data */
static bool process_zlsd(STREAMFILE* sf, bnk_header_t* h) {
    if (!h->zlsd_offset)
        return true;

    read_u32_t read_u32 = h->big_endian ? read_u32be : read_u32le;

    if (read_u32(h->zlsd_offset+0x00,sf) != get_id32be("DSLZ"))
        return false;

    /* 0x04: version? (1) */
    int zlsd_count = read_u32(h->zlsd_offset+0x08,sf);
    /* 0x0c: start */
    /* rest: null */
    
    if (zlsd_count) {
        vgm_logi("BNK: unsupported ZLSD subsongs found\n");
        goto fail;
    }

    /* per entry (for v23)
     * 00: crc (not referenced elsewhere)
     * 04: stream offset (from this offset)
     * 08: null (part of offset?)
     * 0c: stream size
     * 10: offset/size?
     * 14: null */
    /* known streams are standard XVAG (no subsongs) */

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
    else if (read_u32le(0x00,sf) == 0x03) { /* PS2/PSP/Vita/PS4 */
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

    /* section sizes don't include padding (sometimes aligned to 0x10/0x800) */
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
     * - 0x0c: hash (0x10)
     * - 0x1c: filename (0x100?)
     * version ~= v0x23:
     * - 0x0c: null (depends on flags? v1a=0x05, v23=0x07)
     * - 0x10: hash (0x10)
     * - 0x20: filename (0x100?)
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

    h->loop_flag = (h->loop_start >= 0) && (h->loop_end > 0);

    return true;
fail:
    return false;
}
