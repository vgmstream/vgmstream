#include <string.h>
#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "../util.h"
#include "../util/channel_mappings.h"
#include "../util/chunks.h"
#include "../util/endianness.h"
#include "riff_ogg_streamfile.h"

/* RIFF - Resource Interchange File Format, standard container used in many games */

//typedef enum { PCM, MS_ADPCM, MS_IMA, XBOX_ADPCM, ATRAC3, ATRAC3PLUS, ATRAC9, OGG_CBR, AICA, AICA_int, LEVEL7 } riff_codec_t;
typedef struct {
    bool loop_flag;

    bool loop_smpl;
    int32_t loop_start_smpl;
    int32_t loop_end_smpl;

    bool loop_cue;
    int32_t loop_start_cue;
    int32_t loop_end_cue;

    bool loop_labl;
    long loop_start_ms;
    long loop_end_ms;

    bool loop_rgn;
    long loop_region_size;

    bool loop_ctrl;
    int32_t loop_start_ctrl;

    bool loop_wsmp;
    int32_t loop_start_wsmp;
    int32_t loop_end_wsmp;

    bool loop_nxbf;
    int32_t loop_start_nxbf;

} riff_smp_t;

typedef struct {
    off_t offset;
    off_t size;

    uint16_t format;
    int sample_rate;
    uint16_t channels;
    uint16_t block_size;
    int bps;
    off_t extra_size;
    uint32_t channel_layout;
    uint32_t at9_extradata;

    coding_t coding_type;
    uint32_t interleave;

    bool is_at3;
    bool is_at3p;
    bool is_at9;
}
riff_fmt_t;

typedef struct {
    int sample_count;
    int sample_skip;
}
riff_fact_t;

typedef struct {
    riff_fmt_t fmt;
    riff_smp_t smp;
    riff_fact_t fact;

    uint32_t file_size;
    uint32_t riff_size;
    bool ignore_riff_size;

    uint32_t junk_offset;
    uint32_t junk_size;

    uint32_t pflt_offset;
    uint32_t pflt_size;

    uint32_t data_offset;
    uint32_t data_size;

    uint32_t shft_offset;
    uint32_t shift_value;
}
riff_header_t;


/* parse "Marker hh:mm:ss.ms" to milliseconds */
static long parse_adtl_marker_ms(unsigned char* marker, size_t marker_size) {
    int hh, mm, ss, ms;
    int n, m;

    if (marker_size < 20)
        return -1;

    // 00:00:00.NNN, rare (ms as-is)
    m = sscanf((char*)marker, "Marker %02d:%02d:%02d.%03d%n", &hh, &mm, &ss, &ms, &n);
    if (m == 4 && n == 12) {
        return ((hh * 60 + mm) * 60 + ss) * 1000 + ms;
    }

    // 00:00:00.NN, common (ms .15 = 150)
    m = sscanf((char*)marker, "Marker %02d:%02d:%02d.%02d", &hh, &mm, &ss, &ms);
    if (m == 4) {
        return ((hh * 60 + mm) * 60 + ss) * 1000 + ms * 10;
    }

    return -1;
}

/* loop points have been found hiding here (ex. set by Sound Forge) */
static void parse_adtl(uint32_t adtl_offset, uint32_t adtl_length, STREAMFILE* sf, riff_smp_t* si) {
    bool labl_start_found = false;
    bool labl_end_found = false;
    uint32_t chunk_offset = adtl_offset + 0x04;
    unsigned char label_content[128]; //arbitrary max

    while (chunk_offset < adtl_offset + adtl_length) {
        uint32_t chunk_type = read_u32be(chunk_offset + 0x00,sf);
        uint32_t chunk_size = read_u32le(chunk_offset + 0x04,sf);

        if (chunk_offset + 0x08 + chunk_size > adtl_offset + adtl_length) {
            return; // broken adtl?
        }
        chunk_offset += 0x08;

        switch(chunk_type) {
            case 0x6c61626c: { /* "labl" [Advanced Power Dolls 2 (PC), Redline (PC)]  */
                int label_size = chunk_size - 0x04;
                if (label_size >= sizeof(label_content))
                    break;

                int cue_id = read_s32le(chunk_offset + 0x00,sf);
                if (read_streamfile(label_content, chunk_offset + 0x04, label_size,sf) != label_size)
                    return;
                label_content[label_size] = '\0'; // labl null-terminates but just in case

                // find "Marker", though rarely "loop" or "Region" can be found to mark loop cues [Portal (PC), Touhou Suimusou (PC)]
                int loop_value = parse_adtl_marker_ms(label_content, sizeof(label_content));
                if (loop_value < 0)
                    break;

                switch (cue_id) {
                    case 1:
                        if (labl_start_found)
                            break;
                        si->loop_start_ms = loop_value;
                        labl_start_found = (loop_value >= 0);
                        break;
                    case 2:
                        if (labl_end_found)
                            break;
                        si->loop_end_ms = loop_value;
                        labl_end_found = (loop_value >= 0);
                        break;
                    default:
                        break;
                }

                break;
            }

            case 0x6C747874: { /* "ltxt" [Touhou Suimusou (PC), Redline (PC)] */
                if (si->loop_rgn)
                    break;

                int cue_id = read_s32le(chunk_offset + 0x00,sf);
                int32_t cue_point = read_s32le(chunk_offset + 0x04,sf);
                if (!is_id32be(chunk_offset + 0x08, sf, "rgn "))
                    break;
                if (cue_id == 1) {
                    si->loop_rgn = true;
                    si->loop_region_size = cue_point;

                    // assumes cues go first (cue_id should exist?)
                    if (si->loop_cue && !si->loop_end_cue) {
                        si->loop_end_cue = si->loop_start_cue + si->loop_region_size;
                    }
                }

                break;
            }

            default: // "note" also exists
                break;
        }

        /* chunks are even-adjusted like main RIFF chunks */
        if (chunk_size % 0x02 && chunk_offset + chunk_size + 0x01 <= adtl_offset + adtl_length)
            chunk_size += 0x01;

        chunk_offset += chunk_size;
    }

    if (labl_start_found && labl_end_found) {
        si->loop_labl = true;
        si->loop_flag = true;
    }

    // in rare cases loop start cue+labl is found, but doesn't seem to mean loop [Caesar III (PC)]
    if (labl_start_found && !labl_end_found) {
        si->loop_labl = true;
        si->loop_flag = false; // 1 cue is treated as loop start, so force as loop end
    }

    /* labels don't seem to be consistently ordered */
    if (si->loop_start_ms > si->loop_end_ms) {
        long temp = si->loop_start_ms;
        si->loop_start_ms = si->loop_end_ms;
        si->loop_end_ms = temp;
    }

}


// see mmeapi.h (WAVEFORMAT) and mmreg.h (WAVEFORMATEX) for a more detailed codec list, also riffmci / RIFFNEW docs
static bool parse_fmt(riff_fmt_t* fmt, STREAMFILE* sf, uint32_t offset, uint32_t size, bool big_endian) {
    read_u32_t read_u32 = get_read_u32(big_endian);
    read_u16_t read_u16 = get_read_u16(big_endian);

    fmt->offset = offset;
    fmt->size = size;

    /* WAVEFORMAT */
    fmt->format         = read_u16(offset + 0x00,sf);
    fmt->channels       = read_u16(offset + 0x02,sf);
    fmt->sample_rate    = read_u32(offset + 0x04,sf);
  //fmt->avg_bps        = read_u32(offset + 0x08,sf); // "for buffer estimation"
    fmt->block_size     = read_u16(offset + 0x0c,sf);
    fmt->bps            = read_u16(offset + 0x0e,sf);

    /* WAVEFORMATEX */
    if (fmt->size >= 0x10) {
        fmt->extra_size = read_u16(offset + 0x10,sf);
        /* 0x1a+ depends on codec (ex. coef table for MSADPCM, samples_per_frame in MS-IMA, etc) */
        if (fmt->extra_size > size - 0x12) {
            VGM_LOG("RIFF: fmt with wrong extra size\n");
        }
    }

    /* WAVEFORMATEXTENSIBLE */
    if (fmt->format == 0xFFFE && fmt->extra_size >= 0x16) {
      //fmt->extra_samples  = read_u16(offset + 0x12,sf); /* valid_bits_per_sample or samples_per_block */
        fmt->channel_layout = read_u32(offset + 0x14,sf);
        /* 0x10 guid at 0x20 */

        /* happens in various .at3/at9, may be a bug in their encoder b/c MS's defs set mono as FC */
        if (fmt->channels == 1 && fmt->channel_layout == speaker_FL) { /* other channels are fine */
            fmt->channel_layout = speaker_FC;
        }

        /* happens in few at3p, may be a bug in older tools as other games have ok flags [Ridge Racer 7 (PS3)] */
        if (fmt->channels == 6 && fmt->channel_layout == 0x013f) {
            fmt->channel_layout = fmt->channel_layout & 0x00FF;
        }
    }

    if (fmt->channels == 0)
        return false;

    switch (fmt->format) {
        case 0x0000:    // Yamaha AICA ADPCM [Headhunter (DC), Bomber hehhe (DC), Rayman 2 (DC)] (unofficial, WAVE_FORMAT_UNKNOWN)
            if (fmt->bps != 4)
                return false;
            if (fmt->block_size != 0x02 * fmt->channels &&
                fmt->block_size != 0x01 * fmt->channels)
                return false;
            fmt->coding_type = coding_AICA_int;
            fmt->interleave = 0x01;
            break;

        case 0x0001:    // WAVE_FORMAT_PCM
            switch (fmt->bps) {
                case 32: // Get Off My Lawn! (PC)
                    fmt->coding_type = coding_PCM32LE;
                    break;
                case 24: // Tinertia (PC), Beatbuddy (WiiU)
                    fmt->coding_type = coding_PCM24LE;
                    break;
                case 16: // common
                    fmt->coding_type = big_endian ? coding_PCM16BE : coding_PCM16LE;
                    // broken block size [Rayman 2 (DC)]
                    if (fmt->block_size == 0x02 && fmt->channels > 1)
                        fmt->block_size = 0x02 * fmt->channels;
                    break;
                case 8: // The Lost Vikings 2 (PC), Phoenix Wright: Ace Attorney (iOS)
                    fmt->coding_type = coding_PCM8_U;
                    break;
                default:
                    return false;
            }
            fmt->interleave = fmt->block_size / fmt->channels; // in practice bps/8
            break;

        case 0x0002:    // WAVE_FORMAT_ADPCM [Descent: Freespace (PC)]
            if (fmt->bps == 4) {
                // ADPCMWAVEFORMAT extra data:
                //  00: samples per frame
                //  02: num coefs (16b), always 7
                //  04: N x2 coefs (configurable but in practice fixed, first 7 coeffs must be the same)
                fmt->coding_type = coding_MSADPCM;
                if (!msadpcm_check_coefs(sf, fmt->offset + 0x14))
                    return false;
            }
            else if (fmt->bps == 16 && fmt->size == 0x14 && fmt->block_size == 0x02 * fmt->channels) {
                // MX vs ATV Unleashed (PC) codec hijack
                // extra data size is 0, but has a 16-bit value after it, always 0x0040
                fmt->coding_type = coding_IMA;
            }
            else {
                return false;
            }
            break;

        case 0x0003:    // WAVE_FORMAT_IEEE_FLOAT [Cube World (PC), SphereZor (WiiU)]
            if (fmt->bps == 32) {
              fmt->coding_type = coding_PCMFLOAT;
            }
            else {
              return false;
            }
            fmt->interleave = fmt->block_size / fmt->channels;
            break;

        case 0x0011:    // WAVE_FORMAT_IMA_ADPCM / WAVE_FORMAT_DVI_ADPCM (actually MS-IMA ADPCM) [Layton Brothers: Mystery Room (iOS/Android)]
            // IMAADPCMWAVEFORMAT extra data:
            //  00: samples per frame (16b)
            if (fmt->bps != 4)
                return false;
            fmt->coding_type = coding_MS_IMA;
            break;

        case 0x0020:    // WAVE_FORMAT_YAMAHA_ADPCM [Takuyo/Dynamix/etc DC games] (official-ish)
            if (fmt->bps != 4)
                return false;
            fmt->coding_type = coding_AICA;
            /* official RIFF spec has 0x20 as 'Yamaha ADPCM', but data is probably not pure AICA
             * (maybe with headered frames and would need extra detection?) */
            break;

#ifdef VGM_USE_MPEG
        case 0x0055:    // WAVE_FORMAT_MPEGLAYER3 [Bear in the Big Blue House: Bear's Imagine That! (PC), Eclipse (PC)] (official)
            fmt->coding_type = coding_MPEG_custom;
            // some oddities, unsure if part of standard:
            // - block size is 1 (in mono)
            // - bps is 16 for some games
            // - extra size 0x0c, has channels? and (possibly) approx frame size
            break;
#endif

        case 0x0069:    // XBOX IMA ADPCM [Dynasty Warriors 5 (Xbox)] (unofficial, WAVE_FORMAT_VOXWARE_BYTE_ALIGNED)
            // IMAADPCMWAVEFORMAT extra data:
            //  00: samples per frame (16b)
            if (fmt->bps != 4)
                return false;
            fmt->coding_type = coding_XBOX_IMA;
            break;

        case 0x007A:    // MS IMA ADPCM [LA Rush (PC), Psi Ops (PC)] (unofficial, WAVE_FORMAT_VOXWARE_SC3)
            if (!check_extensions(sf,"med"))
                return false;

            if (fmt->bps == 4) // normal MS IMA */
                fmt->coding_type = coding_MS_IMA;
            else if (fmt->bps == 3) //TODO: 3-bit MS IMA, used in a very few files
                return false; //fmt->coding_type = coding_MS_IMA_3BIT;
            else
                return false;
            break;

        case 0x0300:    // IMA ADPCM [Chrono Ma:gia (Android)] (unofficial, WAVE_FORMAT_FM_TOWNS_SND)
            if (fmt->bps != 4)
                return false;
            if (fmt->block_size != 0x0400 * fmt->channels)
                return false;
            if (fmt->size != 0x14)
                return false;
            if (fmt->channels != 1)
                return false; // not seen
            // has extra data 0x02 with some varying 16-bit value
            fmt->coding_type = coding_DVI_IMA;
            // real 0x300 is "Fujitsu FM Towns SND" with block align 0x01
            break;

        case 0x0555:    // Level-5 ADPCM (unofficial)
            fmt->coding_type = coding_LEVEL5;
            fmt->interleave = 0x12;
            break;

        case 0x0917:    // IMA ADPCM [Splash Studios: Piper (PC)] (unofficial)
            // has IMAADPCMWAVEFORMAT extra data as well
            if (fmt->bps != 4)
                return false;
            if (fmt->block_size != 0x0200 * fmt->channels)
                return false;
            if (fmt->size != 0x14)
                return false;
            if (fmt->channels != 1)
                return false;
            fmt->coding_type = coding_MS_IMA;
            break;

#ifdef VGM_USE_VORBIS
      //case 0x674f:    // WAVE_FORMAT_VORBIS1  / WAVE_FORMAT_OGG_VORBIS_MODE_1 ('Og')
      //case 0x6750:    // WAVE_FORMAT_VORBIS2  / WAVE_FORMAT_OGG_VORBIS_MODE_2 ('Pg')
      //case 0x6751:    // WAVE_FORMAT_VORBIS3  / WAVE_FORMAT_OGG_VORBIS_MODE_3 ('Qg')
        case 0x676f:    // WAVE_FORMAT_VORBIS1P / WAVE_FORMAT_OGG_VORBIS_MODE_1_PLUS ('og') [Only One 2 (PC)]
        case 0x6770:    // WAVE_FORMAT_VORBIS2P / WAVE_FORMAT_OGG_VORBIS_MODE_2_PLUS ('pg') [Only One (PC)]
        case 0x6771:    // WAVE_FORMAT_VORBIS3P / WAVE_FORMAT_OGG_VORBIS_MODE_3_PLUS ('qg') [Liar-soft games]
            // vorbis.acm codecs by H.Mutsuki (somewhat accepted as official-ish). From docs/source:
            // - mode1: standard Ogg in "data"
            // - mode2: Ogg header is part of extra data (meant to be only in fmt?)
            //   In practice known files stores the header twice, in "fmt" and "data" (vorbis.acm version 2002-02-01).
            // - mode3: in theory doesn't store headers (info/comments/codebooks) and uses a fixed one in vorbis.acm (dump.inl).
            //   In practice known files work like mode1 (vorbis.acm version 2002-02-01).
            // - mode*+ = "pseudo CBR" modes, that add a 2nd empty stream for padding (ID -1).
            //   Data sizes it's rather inconsistent so not sure about its purpose.
            //   This fake stream causes issues in current libs though (see riff_ogg_streamfile).
            //
            // OGGWAVEFORMAT extra data:
            //  00: vorbis acm version (date in LE hex: 0x20020201)
            //  04: libvorbis version (date as well: 0x20011231)
            //  08+: reserved for mode2
            fmt->coding_type = coding_OGG_VORBIS;
            break;
#endif

#ifdef VGM_USE_FFMPEG
        case 0x0270:    // ATRAC3 (officially WAVE_FORMAT_SONY_SCX, WAVE_FORMAT_SONY_ATRAC3 = 0x0272)
            fmt->coding_type = coding_FFmpeg;
            fmt->is_at3 = true;
            break;
#endif

        case 0xFFFE: {  // WAVEFORMATEXTENSIBLE (see ksmedia.h for known GUIDs)
            if (fmt->extra_size < 0x16) // 0x12 + (0x06 + 0x10)
                return false;
            uint32_t guid1 = read_u32  (offset + 0x18,sf); // 32-bit
            uint32_t guid2 = (read_u16 (offset + 0x1C,sf) << 16) | (read_u16 (offset + 0x1E,sf)); // 16-bit + 16-bit
            uint32_t guid3 = read_u32be(offset + 0x20,sf); // 8-bit x4
            uint32_t guid4 = read_u32be(offset + 0x24,sf); // 8-bit x4
            //;VGM_LOG("riff: guid %08x %08x %08x %08x\n", guid1, guid2, guid3, guid4);

            // PCM GUID (0x00000001,0000,0010,80,00,00,AA,00,38,9B,71)
            if (guid1 == 0x00000001 && guid2 == 0x00000010 && guid3 == 0x800000AA && guid4 == 0x00389B71) {
                switch (fmt->bps) {
                    case 16:
                        fmt->coding_type = big_endian ? coding_PCM16BE : coding_PCM16LE;
                        fmt->interleave = 0x02;
                        break;
                    default:
                        return false;
                }
                break;
            }

            // ATRAC3plus GUID (0xE923AABF,CB58,4471,A1,19,FF,FA,01,E4,CE,62)
            if (guid1 == 0xE923AABF && guid2 == 0xCB584471 && guid3 == 0xA119FFFA && guid4 == 0x01E4CE62) {
#ifdef VGM_USE_FFMPEG
                fmt->coding_type = coding_FFmpeg;
                fmt->is_at3p = true;

                // known files have this but just in case don't enforce it
                //if (fmt->extra_size < 0x22)
                //    return false;
                // 00: encoder config?
                // 04: null/reserved?
                // 08: null/reserved?
                break;
#else
                return false;
#endif
            }

#ifdef VGM_USE_ATRAC9
            // ATRAC9 GUID (0x47E142D2,36BA,4D8D,88,FC,61,65,4F,8C,83,6C)
            if (guid1 == 0x47E142D2 && guid2 == 0x36BA4D8D && guid3 == 0x88FC6165 && guid4 == 0x4F8C836C) {
                fmt->coding_type = coding_ATRAC9;
                fmt->is_at9 = true;

                if (fmt->extra_size < 0x22)
                    return false;
                // 00: ATRAC9 version (apparently: 1=normal, 2=band extension)
                fmt->at9_extradata = read_u32be(offset + 0x28 + 0x04,sf);
                // 08: null/reserved?
                break;
            }
#endif

            return false; // unknown GUID
        }

        case 0xFFFF:  // UltraMarine lib extension [Doura III (PC)]
            if (fmt->bps != 4)
                return false;
            if (fmt->block_size != 0x01 * fmt->channels)
                return false;
            if (fmt->size != 0x10)
                return false;
            if (fmt->channels > 2)
                return false;

            fmt->coding_type = coding_OKI_UM;
            break;

        default:
            // FFmpeg may play it
            //vgm_logi("RIFF: unknown codec 0x%04x (report)\n", fmt->format);
            return false;
    }

    return true;
}

static bool is_ue4_msadpcm(STREAMFILE* sf, riff_fmt_t* fmt, int fact_sample_count, off_t start_offset, uint32_t data_size);
static size_t get_ue4_msadpcm_interleave(STREAMFILE* sf, riff_fmt_t* fmt, off_t start, size_t size);


/* some games have wonky sizes, selectively fix to catch bad rips and new mutations */
static void fix_sizes(riff_header_t* riff, STREAMFILE* sf) {
    uint32_t riff_size = riff->riff_size;
    uint32_t file_size = get_streamfile_size(sf);

    if (file_size != riff_size + 0x08) {
        uint16_t codec = read_u16le(0x14,sf);

        if      ((codec & 0xFF00) == 0x6700 && riff_size + 0x08 + 0x01 == file_size)
            riff_size += 0x01; /* [Shikkoku no Sharnoth (PC), Only One 2 (PC)] (vorbis.acm) */

        else if (codec == 0x0069 && riff_size == file_size)
            riff_size -= 0x08; /* [Dynasty Warriors 3 (Xbox), BloodRayne (Xbox)] */

        else if (codec == 0x0069 && riff_size + 0x04 == file_size)
            riff_size -= 0x04; /* [Halo 2 (PC)] (possibly bad extractor? 'Gravemind Tool') */

        else if (codec == 0x0069 && riff_size + 0x10 == file_size)
            riff_size += 0x08; /* [Fast and the Furious (Xbox)] ("HASH" chunk + 4 byte hash) */

        else if (codec == 0x0000 && riff_size + 0x04 == file_size)
            riff_size -= 0x04; /* [Headhunter (DC), Bomber hehhe (DC)] */

        else if (codec == 0x0000 && riff_size == file_size)
            riff_size -= 0x08; /* [Rayman 2 (DC)] */

        else if (codec == 0x0000 && riff_size + 0x08 + 0x02 == file_size)
            riff_size -= 0x02; /* [Rayman 2 (DC)]-dcz */

        else if (codec == 0x0300 && riff_size == file_size)
            riff_size -= 0x08; /* [Chrono Ma:gia (Android)] */

        else if (codec == 0xFFFE && riff_size + 0x08 + 0x18 == file_size)
            riff_size += 0x18; /* [F1 2011 (Vita)] (adds a "pada" chunk but RIFF size wasn't updated) */

        else if (codec == 0x0555) {
            int channels = read_u16le(0x16, sf); /* [Dragon Quest VIII (PS2), Rogue Galaxy (PS2)] */
            size_t file_size_fixed = riff_size + 0x08 + 0x04 * (channels - 1);

            if (file_size_fixed <= file_size && file_size - file_size_fixed < 0x10) {
                /* files inside HD6/DAT are also padded to 0x10 so need to fix file_size */
                file_size = file_size_fixed;
                riff_size = file_size - 0x08;
            }
        }

        else if (riff_size >= file_size && is_id32be(0x24,sf, "NXBF"))
            riff_size = file_size - 0x08; /* [R:Racing Evolution (Xbox)] */

        else if (codec == 0x0011 && (riff_size / 2 / 2 == read_u32le(0x30,sf))) /* riff_size = pcm_size (always stereo, has fact at 0x30) */
            riff_size = file_size - 0x08; /* [Asphalt 6 (iOS)] (sfx/memory wavs have ok sizes?) */

        else if (codec == 0xFFFE && riff_size + 0x08 + 0x30 == file_size)
            riff_size += 0x30; /* [E.X. Troopers (PS3)] (adds "ver /eBIT/tIME/mrkr" empty chunks but RIFF size wasn't updated) */

        else if (codec == 0xFFFE && riff_size + 0x08 + 0x38 == file_size)
            riff_size += 0x38; /* [Sengoku Basara 4 (PS3)] (adds "ver /eBIT/tIME/mrkr" chunks but RIFF size wasn't updated) */

        else if (codec == 0x0002 && riff_size + 0x08 + 0x1c == file_size)
            riff_size += 0x1c; /* [Mega Man X Legacy Collection (PC)] (adds "ver /tIME/ver " chunks but RIFF size wasn't updated) */

        else if (codec == 0x0001 && (
                    riff_size + 0x08 + 0x08 == file_size || riff_size + 0x08 + 0x09 == file_size ||
                    riff_size + 0x08 - 0x3E == file_size || riff_size + 0x08 - 0x02 == file_size))
            riff->ignore_riff_size = true; /* [Cross Gate (PC)] (last info LIST chunk has wrong size) */

        else if (codec == 0xFFFE && riff_size + 0x08 + 0x40 == file_size)
            file_size -= 0x40; /* [Megami no Etsubo (PSP)] (has extra padding in all files) */

        else if (codec == 0x0011 && file_size - riff_size - 0x08 <= 0x900 && is_id32be(riff_size + 0x08, sf, "cont"))
            riff_size = file_size - 0x08; /* [Shin Megami Tensei: Imagine (PC)] (extra "cont" info 0x800/0x900 chunk) */

        else if (codec == 0x0001 && riff_size % 0x02 && riff_size + 0x08 + 0x01 == file_size)
            riff_size += 0x01; // padding byte, rarely seen (spec isn't too clear about RIFF's size) [Delta Force 2 (PC)]

        else if (codec == 0xFFFF && riff_size + 0x08 + 0x26 == file_size)
            riff_size += 0x26; /* [Doura III (PC)] (doesn't seem to include size for extra chunks) */
    }

    riff->riff_size = riff_size;
    riff->file_size = file_size;
}

/* skip mutant RIFFs that should be parsed elsewhere, after reading the whole format */
static bool is_valid_riff(riff_header_t* riff, STREAMFILE* sf) {

    //TODO: improve detection using fmt sizes/values as Wwise's don't match the RIFF standard for some codecs
    // JUNK is an optional Wwise chunk, and Wwise hijacks the MSADPCM/MS_IMA/XBOX IMA ids (how nice).
    // To ensure their stuff is parsed in wwise.c we reject their JUNK, which they put almost always.
    // As JUNK is legal (if unusual) we only reject those codecs.
    // (ex. Cave PC games have PCM16LE + JUNK + smpl created by "Samplitude software") */
    if (riff->junk_offset
            && (riff->fmt.coding_type == coding_MSADPCM || riff->fmt.coding_type == coding_XBOX_IMA /*|| riff->fmt.coding_type==coding_MS_IMA*/)
            && check_extensions(sf,"wav,lwav") /* for some .MED IMA */
            ) {
        return false;
    }

    // Ignore Beyond Good & Evil HD PS3 evil reuse of the PCM codec in Ubi Jade.
    // Normally padded and rejected, but just in case they are manually de-padded
    // Defined sample rate is not actually used (bgm=32000, sfx=~10000-12000, real=48000).
    if (riff->fmt.format == 0x0001 &&
            riff->fmt.size == 0x10 && riff->fmt.sample_rate <= 32000 &&
            read_u32be(riff->data_offset+0x00, sf) == get_id32be("MSF\x43") &&
            read_u32be(riff->data_offset+0x34, sf) == 0xFFFFFFFF && // always
            read_u32be(riff->data_offset+0x38, sf) == 0xFFFFFFFF &&
            read_u32be(riff->data_offset+0x3c, sf) == 0xFFFFFFFF) {
        return false;
    }

    // MSADPCM .ckd are parsed elsewhere, though they are valid so no big deal if parsed here (just that loops should be ignored)
    if (riff->fmt.format == 0x0002 && check_extensions(sf, "ckd")) {
        return false;
    }

#if 0
    // Ignore Gitaroo Man Live! (PSP) multi-RIFF (to allow chunked TXTH).
    // Shouldn't be needed as RIFF sizes are validated now and differences are large
    if (riff->fmt.is_at3 && get_streamfile_size(sf) > 0x2800 && read_u32be(0x2800, sf) == get_id32be("RIFF")) {
        return false;
    }
#endif

    return true;
}

static bool parse_riff(riff_header_t* riff, STREAMFILE* sf) {
    uint32_t chunk_offset = 0x0c; /* start with first chunk */
    uint32_t max_offset = riff->file_size;

    while (chunk_offset < max_offset) {
        uint32_t chunk_type = read_u32be(chunk_offset + 0x00,sf); /* FOURCC */
        uint32_t chunk_size = read_u32le(chunk_offset + 0x04,sf);

        /* allow broken last chunk [Cross Gate (PC)] */
        if (chunk_offset + 0x08 + chunk_size > riff->file_size) {
            VGM_LOG("RIFF: broken chunk at %x + 0x08 + %x > %x\n", chunk_offset, chunk_size, riff->file_size);
            break; /* truncated */
        }
        chunk_offset += 0x08;

        switch(chunk_type) {
            case 0x666d7420:    /* "fmt " (format description) */
                if (riff->fmt.offset)
                    return false; // only one per file

                if (!parse_fmt(&riff->fmt, sf, chunk_offset, chunk_size, false))
                    return false;

                /* some Dreamcast/Naomi games [Headhunter (DC), Bomber hehhe (DC), Rayman 2 (DC)] */
                if (riff->fmt.format == 0x0000 && chunk_size == 0x12)
                    chunk_size += 0x02;
                break;

            case 0x64617461:    /* "data" (main payload) */
                if (riff->data_offset)
                    return false; // only one per file
                riff->data_offset = chunk_offset;
                riff->data_size = chunk_size;
                break;

            case 0x4A554E4B:    /* "JUNK" (padding) */
                riff->junk_offset = chunk_offset;
                riff->junk_size = chunk_size;
                break;


            case 0x66616374:    /* "fact" (sample info) */
                if (chunk_size == 0x04) {
                        /* standard (usually for ADPCM, MS recommends setting for non-PCM codecs but optional) */
                    riff->fact.sample_count = read_s32le(chunk_offset + 0x00, sf);
                }
                else if (chunk_size == 0x10 && is_id32be(chunk_offset + 0x04, sf, "LyN ")) {
                    return false; // parsed elsewhere
                }
                else if ((riff->fmt.is_at3 || riff->fmt.is_at3p) && chunk_size == 0x08) {
                    /* early AT3 (mainly PSP games) */
                    riff->fact.sample_count = read_s32le(chunk_offset + 0x00, sf);
                    riff->fact.sample_skip  = read_s32le(chunk_offset + 0x04, sf); // base skip samples
                }
                else if ((riff->fmt.is_at3 || riff->fmt.is_at3p) && chunk_size == 0x0c) {
                    /* late AT3 (mainly PS3 games and few PSP games) */
                    riff->fact.sample_count = read_s32le(chunk_offset + 0x00, sf);
                    // 0x04: base skip samples, ignored by decoder
                    riff->fact.sample_skip  = read_s32le(chunk_offset + 0x08, sf); // skip samples with extra 184
                }
                else if (riff->fmt.is_at9 && chunk_size == 0x0c) {
                    riff->fact.sample_count = read_s32le(chunk_offset + 0x00, sf);
                    // 0x04: base skip samples (same as next field)
                    riff->fact.sample_skip  = read_s32le(chunk_offset + 0x08, sf);
                }
                else {
                    vgm_logi("RIFF: unknown 'fact' format (report)\n");
                }
                break;

            case 0x4C495354:    /* "LIST" */
                switch (read_u32be(chunk_offset, sf)) {
                    case 0x6164746C:    /* "adtl" (loop info, sometimes) */
                        /* yay, atdl is its own little world */
                        parse_adtl(chunk_offset, chunk_size, sf, &riff->smp);
                        break;
                    default:
                        break;
                }
                break;

            case 0x736D706C:    /* "smpl" (loop info, RIFFMIDISample + MIDILoop chunk) */
                /* check loop count/loop info (most fields are reserved for midi and null/irrelevant for RIFF) */
                // 0x00: manufacturer id
                // 0x04: product id
                // 0x08: sample period
                // 0x0c: unity node
                // 0x10: pitch fraction
                // 0x14: SMPTE format
                // 0x18: SMPTE offset
                // 0x1c: loop count (may contain N MIDILoop)
                // 0x20: sampler data
                // 0x24: per loop point:
                //   0x00: cue point id
                //   0x04: type (0=forward, 1=alternating, 2=backward)
                //   0x08: loop start
                //   0x0c: loop end
                //   0x10: fraction
                //   0x14: play count
                if (read_u32le(chunk_offset + 0x1c, sf) != 1) { /* handle only 1 loop */
                    VGM_LOG("RIFF: found multiple smpl loop points, ignoring\n");
                    break;
                }

                if (read_u32le(chunk_offset + 0x24 + 0x04, sf) == 0) { /* loop forward */
                    riff->smp.loop_start_smpl = read_s32le(chunk_offset + 0x24 + 0x08, sf);
                    riff->smp.loop_end_smpl   = read_s32le(chunk_offset + 0x24 + 0x0c, sf);
                    riff->smp.loop_smpl = true;
                    riff->smp.loop_flag = true;
                }
                break;

            case 0x77736D70:    /* "wsmp" (loop info, RIFFDLSSample + DLSLoop chunk)  */
                /* check loop count/info (found in some Xbox games: Halo (non-looping), Dynasty Warriors 3/4/5, Crimson Sea) */
                // 0x00: size
                // 0x04: unity note
                // 0x06: fine tune
                // 0x08: gain
                // 0x10: loop count
                // 0x14: per loop:
                //   0x00: size
                //   0x04: loop type (0=forward, 1=release)
                //   0x08: loop start
                //   0x0c: loop length
                if (chunk_size < 0x24
                    || read_u32le(chunk_offset + 0x00, sf) != 0x14
                    || read_s32le(chunk_offset + 0x10, sf) <= 0
                    || read_u32le(chunk_offset + 0x14, sf) != 0x10) {
                    VGM_LOG("RIFF: found incorrect wsmp loop points, ignoring\n");
                    break;
                }

                if (read_u32le(chunk_offset + 0x14 + 0x04, sf) == 0) { /* loop forward */
                    riff->smp.loop_start_wsmp = read_s32le(chunk_offset + 0x14 + 0x08, sf);
                    riff->smp.loop_end_wsmp   = read_s32le(chunk_offset + 0x14 + 0x0c, sf); /* must *not* add 1 as per spec (region) */
                    riff->smp.loop_end_wsmp  += riff->smp.loop_start_wsmp;
                    riff->smp.loop_wsmp = true;
                    riff->smp.loop_flag = true;
                }
                break;

            case 0x63756520: {  /* "cue " (loop info, used in Source Engine amd also seen cue + adtl in Sound Forge) [Team Fortress 2 (PC)] */
                if (!(riff->fmt.coding_type == coding_PCM8_U || riff->fmt.coding_type == coding_PCM16LE || riff->fmt.coding_type == coding_MSADPCM))
                    break;

                /* handle loop_start or start + end (more are possible but usually means custom regions);
                    * could have have other meanings but is often used for loops */
                int num_cues = read_s32le(chunk_offset + 0x00, sf);
                if (num_cues <= 0 || num_cues > 2)
                    break;

                uint32_t cue_offset = chunk_offset + 0x04;
                for (int i = 0; i < num_cues; i++) {
                    // 0x00: id (usually 0x01, 0x02 ... but may be unordered)
                    // 0x04: position (usually same as sample point)
                    // 0x08: fourcc type
                    // 0x0c: "chunk start", relative offset (null in practice)
                    // 0x10: "block start", relative offset (null in practice)
                    // 0x14: sample offset
                    uint32_t cue_id     = read_s32le(cue_offset + 0x00, sf);
                    uint32_t cue_point  = read_s32le(cue_offset + 0x14, sf);
                    cue_offset += 0x18;

                    switch (cue_id) {
                        case 1:
                            riff->smp.loop_start_cue = cue_point;
                            break;
                        case 2:
                            riff->smp.loop_end_cue = cue_point;
                            break;
                        default:
                            break;
                    }
                }

                // cues may be unordered so swap if needed
                if (riff->smp.loop_end_cue > 0 && riff->smp.loop_start_cue > riff->smp.loop_end_cue) {
                    int32_t tmp = riff->smp.loop_start_cue;
                    riff->smp.loop_start_cue = riff->smp.loop_end_cue;
                    riff->smp.loop_end_cue = tmp;
                }
                riff->smp.loop_cue = true;
                riff->smp.loop_flag = true;

                /* assumes "cue" goes before "adtl" (has extra detection for some cases) */
                break;
            }

            case 0x4E584246:    /* "NXBF" (Namco NuSound v1 extension) [R:Racing Evolution (Xbox)] */
                /* very similar to NUS's NPSF, but not quite like Cstr */
                // 0x00: "NXBF" id
                // 0x04: version? (0x00001000 = 1.00?)
                // 0x08: data size
                // 0x0c: channels
                // 0x10: null
                riff->smp.loop_start_nxbf = read_s32le(chunk_offset + 0x14, sf);
                // 0x18: sample rate
                // 0x1c: volume? (0x3e8 = 1000 = max)
                // 0x20: type/flags?
                // 0x24: flag?
                // 0x28: null
                // 0x2c: null
                // 0x30: always 0x40
                riff->smp.loop_nxbf = true;
                riff->smp.loop_flag = (riff->smp.loop_start_nxbf >= 0);
                break;


            case 0x70666c74:    /* "pflt" (.mwv extension, predictor filters) */
                riff->pflt_offset = chunk_offset;
                riff->pflt_size = chunk_size;
                break;

            case 0x6374726c:    /* "ctrl" (.mwv extension) */
                riff->smp.loop_flag        = read_s32le(chunk_offset + 0x00, sf);
                riff->smp.loop_start_ctrl  = read_s32le(chunk_offset + 0x04, sf);
                riff->smp.loop_ctrl = true;
                break;

            case 0x73686674:    /* "shft" (UltraMarine lib extension) */
                riff->shft_offset = chunk_offset;
                if (chunk_size < 0x04)
                    return false;
                riff->shift_value = read_u32le(chunk_offset + 0x00, sf);
                break;

#if 0
            // known files include this chunk, but lib doesn't seem to read it
            case 0x656E6376:    /* "encv" (UltraMarine lib extension) */
                riff->encv_offset = chunk_offset;
                //if (chunk_size != 0x08)
                //    return false;
                // 00: encoding (always "adpu")
                // 04: version? (always 5) 
                break;
#endif

            case 0x4C795345: /* "LySE" (Ubisoft LyN mutant RIFF) */
            case 0x64737068: /* "dsph" (UbiArt Framework mutant RIFF) */
            case 0x63776176: /* "cwav" (UbiArt Framework mutant RIFF) */
                return false; // parsed elsewhere

            default:
                /* ignorance is bliss */
                break;
        }

        /* chunks are even-sized with padding byte (for 16b reads) as per spec (normally
         * pre-adjusted except for a few like Liar-soft's), at end may not have padding though
         * (done *after* chunk parsing since size without padding is needed) */
        if (chunk_size % 0x02 && chunk_offset + chunk_size + 0x01 <= riff->file_size)
            chunk_size += 0x01;

        chunk_offset += chunk_size;
    }

    if (!riff->fmt.offset || !riff->data_offset)
        return false;

    if (!is_valid_riff(riff, sf))
        return NULL;

    return true;
}

VGMSTREAM* init_vgmstream_riff(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;

    /* checks*/
    if (!is_id32be(0x00,sf,"RIFF"))
        return NULL;

    riff_header_t riff = {0};

    riff.riff_size = read_u32le(0x04,sf);
    if (!is_id32be(0x08,sf, "WAVE"))
        return NULL;

    /* .lwav: to avoid hijacking .wav
     * .xwav: fake for Xbox games (not needed anymore)
     * .da: The Great Battle VI (PS1)
     * .dax: Love Game's - Wai Wai Tennis (PS1)
     * .cd: Exector (PS)
     * .med: Psi Ops (PC)
     * .snd: Layton Brothers (iOS/Android)
     * .adx: Remember11 (PC) sfx
     * .adp: Headhunter (DC), UltraMarine lib
     * .xss: Spider-Man The Movie (Xbox)
     * .xsew: Mega Man X Legacy Collection (PC)
     * .adpcm: Angry Birds Transformers (Android)
     * .adw: Dead Rising 2 (PC)
     * .wd: Genma Onimusha (Xbox) voices
     * (extensionless): Myst III (Xbox), Delta Force 2 (PC)
     * .sbv: Spongebob Squarepants - The Movie (PC)
     * .wvx: Godzilla - Destroy All Monsters Melee (Xbox)
     * .str: Harry Potter and the Philosopher's Stone (Xbox)
     * .at3: standard ATRAC3
     * .rws: Climax ATRAC3 [Silent Hill Origins (PSP), Oblivion (PSP)]
     * .aud: EA Replay ATRAC3
     * .at9: standard ATRAC9
     * .ckd: renamed ATRAC9 [Rayman Origins (Vita)]
     * .saf: Whacked! (Xbox)
     * .mwv: Level-5 games [Dragon Quest VIII (PS2), Rogue Galaxy (PS2)]
     * .ima: Baja: Edge of Control (PS3/X360)
     * .nsa: Studio Ring games that uses NScripter [Hajimete no Otetsudai (PC)]
     * .pcm: Silent Hill Arcade (PC)
     * .xvag: Uncharted Golden Abyss (Vita)[ATRAC9]
     * .ogg/logg: Luftrausers (Vita)[ATRAC9]
     * .p1d: Farming Simulator 15 (Vita)[ATRAC9]
     * .xms: Ty the Tasmanian Tiger (Xbox)
     * .mus: Burnout Legends/Dominator (PSP)
     * .dat/ldat: RollerCoaster Tycoon 1/2 (PC), Winning Eleven 2008 (AC)
     * .wma/lwma: SRS: Street Racing Syndicate (Xbox), Fast and the Furious (Xbox)
     * .caf: Topple (iOS)
     * .wax: Lamborghini (Xbox)
     * .voi: Sol Trigger (PSP)[ATRAC3]
     * .se: Rockman X4 (PC)
     * .v: Rozen Maiden: Duellwalzer (PS2)
     * .xst: Animaniacs: The Great Edgar Hunt (Xbox)
     * .wxv: Dariusburst (PSP)[ATRAC3]
     * .vag: Knight Rider (PS2)
     * .xbw: Elminage: Yami no Fujo to Kamigami no Yubiwa (PS2)
     * .at9psv: Touhou Kobuto V - Burst Battle (Vita)[ATRAC9]
     * .bgm: Kuon no Kizuna - Sairin Mikotonori Portable (PSP)[ATRAC3]
     */
    if (!check_extensions(sf, "wav,lwav,xwav,mwv,da,dax,cd,med,snd,adx,adp,xss,xsew,adpcm,adw,wd,,sbv,wvx,str,at3,rws,aud,at9,ckd,saf,ima,nsa,pcm,xvag,ogg,logg,p1d,xms,mus,dat,ldat,wma,lwma,caf,wax,voi,se,v,xst,wxv,vag,xbw,at9psv,bgm")) {
        return NULL;
    }

    /* check for truncated RIFF */
    fix_sizes(&riff, sf);
    if (riff.file_size != riff.riff_size + 0x08 && !riff.ignore_riff_size) {
        vgm_logi("RIFF: wrong expected size (report/re-rip?)\n");
        VGM_LOG("riff: file_size = %x, riff_size+8 = %x\n", riff.file_size, riff.riff_size + 0x08); /* don't log to user */
        return NULL;
    }

    /* read through chunks to verify format and find metadata */
    if (!parse_riff(&riff, sf))
        return NULL;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(riff.fmt.channels, riff.smp.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = riff.fmt.sample_rate;
    vgmstream->channel_layout = riff.fmt.channel_layout;

    /* coding, layout, interleave */
    vgmstream->coding_type = riff.fmt.coding_type;
    switch (riff.fmt.coding_type) {
        case coding_MS_IMA:
        case coding_AICA:
        case coding_XBOX_IMA:
        case coding_IMA:
        case coding_DVI_IMA:
#ifdef VGM_USE_FFMPEG
        case coding_FFmpeg:
#endif
#ifdef VGM_USE_ATRAC9
        case coding_ATRAC9:
#endif
#ifdef VGM_USE_VORBIS
        case coding_OGG_VORBIS:
#endif
            vgmstream->layout_type = layout_none;
            vgmstream->interleave_block_size = riff.fmt.block_size;
            break;
#ifdef VGM_USE_MPEG
        case coding_MPEG_custom:
            vgmstream->layout_type = layout_none;
            break;
#endif
        case coding_MSADPCM:
            vgmstream->layout_type = layout_none;
            vgmstream->frame_size = riff.fmt.block_size;
            break;

        default:
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = riff.fmt.interleave;
            break;
    }

    /* samples, codec init (after setting coding to ensure proper close on failure) */
    switch (riff.fmt.coding_type) {
        case coding_PCM32LE:
        case coding_PCM24LE:
        case coding_PCM16LE:
        case coding_PCM8_U:
        case coding_PCMFLOAT:
            vgmstream->num_samples = pcm_bytes_to_samples(riff.data_size, riff.fmt.channels, riff.fmt.bps);
            break;

        case coding_LEVEL5: {
            int filter_order, filter_count;
            if (!riff.pflt_offset) goto fail;

            vgmstream->num_samples = riff.data_size / 0x12 / riff.fmt.channels * 32;

            /* coefs */
            filter_order = read_s32le(riff.pflt_offset + 0x00, sf);
            filter_count = read_s32le(riff.pflt_offset + 0x04, sf);
            if (filter_order != 3 || filter_count > 32 ||
                riff.pflt_size < 0x08 + filter_count * 0x04 * filter_order)
                goto fail;

            for (int ch = 0; ch < riff.fmt.channels; ch++) {
                for (int i = 0; i < filter_count * filter_order; i++) {
                    int coef = read_s32le(riff.pflt_offset + 0x08 + i * 0x04, sf);
                    vgmstream->ch[ch].adpcm_coef_3by32[i] = coef;
                }
            }

            break;
        }

        case coding_MSADPCM:
            vgmstream->num_samples = msadpcm_bytes_to_samples(riff.data_size, riff.fmt.block_size, riff.fmt.channels);
            if (riff.fact.sample_count && riff.fact.sample_count < vgmstream->num_samples)
                vgmstream->num_samples = riff.fact.sample_count;
            break;

        case coding_MS_IMA:
            vgmstream->num_samples = ms_ima_bytes_to_samples(riff.data_size, riff.fmt.block_size, riff.fmt.channels);
            if (riff.fact.sample_count && riff.fact.sample_count < vgmstream->num_samples)
                vgmstream->num_samples = riff.fact.sample_count;
            break;

        case coding_AICA:
        case coding_AICA_int:
            vgmstream->num_samples = yamaha_bytes_to_samples(riff.data_size, riff.fmt.channels);
            break;

        case coding_OKI_UM:
            if (riff.shft_offset == 0)
                goto fail;
            if (riff.shift_value > 16) // arbitrary max
                goto fail;
            vgmstream->codec_config = riff.shift_value;
            vgmstream->interleave_block_size = 0x01;
            vgmstream->num_samples = riff.fact.sample_count;
            break;

        case coding_XBOX_IMA:
            vgmstream->num_samples = xbox_ima_bytes_to_samples(riff.data_size, riff.fmt.channels);
            if (riff.fact.sample_count && riff.fact.sample_count < vgmstream->num_samples)
                vgmstream->num_samples = riff.fact.sample_count; /* some (converted?) Xbox games have bigger fact_samples */
            break;

        case coding_IMA:
        case coding_DVI_IMA:
            vgmstream->num_samples = ima_bytes_to_samples(riff.data_size, riff.fmt.channels);
            break;

#ifdef VGM_USE_FFMPEG
        case coding_FFmpeg: {
            if (!riff.fmt.is_at3 && !riff.fmt.is_at3p) goto fail;

            vgmstream->codec_data = init_ffmpeg_atrac3_riff(sf, 0x00, NULL);
            if (!vgmstream->codec_data) goto fail;

            vgmstream->num_samples = riff.fact.sample_count;
            if (riff.smp.loop_flag) {
                /* adjust RIFF loop/sample absolute values (with skip samples) */
                riff.smp.loop_start_smpl -= riff.fact.sample_skip;
                riff.smp.loop_end_smpl   -= riff.fact.sample_skip;

                /* happens with official tools when "fact" is not found */
                if (vgmstream->num_samples == 0)
                    vgmstream->num_samples = riff.smp.loop_end_smpl + 1;
            }

            break;
        }
#endif
#ifdef VGM_USE_ATRAC9
        case coding_ATRAC9: {
            atrac9_config cfg = {0};

            cfg.channels = vgmstream->channels;
            cfg.config_data = riff.fmt.at9_extradata;
            cfg.encoder_delay = riff.fact.sample_skip;

            vgmstream->codec_data = init_atrac9(&cfg);
            if (!vgmstream->codec_data) goto fail;

            vgmstream->num_samples = riff.fact.sample_count;
            /* RIFF loop/sample values are absolute (with skip samples), adjust */
            if (riff.smp.loop_flag) {
                riff.smp.loop_start_smpl -= riff.fact.sample_skip;
                riff.smp.loop_end_smpl   -= riff.fact.sample_skip;
            }

            break;
        }
#endif
#ifdef VGM_USE_VORBIS
        case coding_OGG_VORBIS: {
            /* special handling of Liar-soft's buggy RIFF+Ogg made with Soundforge/vorbis.acm [Shikkoku no Sharnoth (PC)],
             * and rarely other devs, not always buggy [Kirara Kirara NTR (PC), No One 2 (PC)] */
            STREAMFILE* temp_sf = setup_riff_ogg_streamfile(sf, riff.data_offset, riff.data_size);
            if (!temp_sf) goto fail;

            vgmstream->codec_data = init_ogg_vorbis(temp_sf, 0x00, get_streamfile_size(temp_sf), NULL);
            if (!vgmstream->codec_data) goto fail;

            /* Soundforge includes fact_samples and should be equal to Ogg samples */
            vgmstream->num_samples = riff.fact.sample_count;
            break;
        }
#endif

#ifdef VGM_USE_MPEG
        case coding_MPEG_custom: {
            mpeg_custom_config cfg = {0};

            vgmstream->codec_data = init_mpeg_custom(sf, riff.data_offset, &vgmstream->coding_type, riff.fmt.channels, MPEG_STANDARD, &cfg);
            if (!vgmstream->codec_data) goto fail;

            /* should provide "fact" but it's optional (some game files don't include it) */
            if (!riff.fact.sample_count)
                riff.fact.sample_count = mpeg_get_samples(sf, riff.data_offset, riff.data_size);
            vgmstream->num_samples = riff.fact.sample_count;
        }
        break;
#endif

        default:
            goto fail;
    }

    /* UE4 uses interleaved mono MSADPCM, try to autodetect without breaking normal MSADPCM */
    if (riff.fmt.coding_type == coding_MSADPCM && is_ue4_msadpcm(sf, &riff.fmt, riff.fact.sample_count, riff.data_offset, riff.data_size)) {
        vgmstream->coding_type = coding_MSADPCM_mono;
        vgmstream->codec_config = 1; /* mark as UE4 MSADPCM */
        vgmstream->frame_size = riff.fmt.block_size;
        vgmstream->layout_type = layout_interleave;
        vgmstream->interleave_block_size = get_ue4_msadpcm_interleave(sf, &riff.fmt, riff.data_offset, riff.data_size);
        if (riff.fmt.size == 0x36)
            vgmstream->num_samples = read_s32le(riff.fmt.offset + 0x32, sf);
        else if (riff.fmt.size == 0x32)
            vgmstream->num_samples = msadpcm_bytes_to_samples(riff.data_size / riff.fmt.channels, riff.fmt.block_size, 1);
    }

    /* Dynasty Warriors 5 (Xbox) 6ch interleaves stereo frames, probably not official */
    if (vgmstream->coding_type == coding_XBOX_IMA && vgmstream->channels > 2) {
        vgmstream->layout_type = layout_interleave;
        vgmstream->interleave_block_size = 0x24; /* block_size / channels */
        if (vgmstream->channels > 2 && vgmstream->channels % 2 != 0)
            goto fail; /* only 2ch+..+2ch layout is known */
    }


    /* meta, loops */
    vgmstream->meta_type = meta_RIFF_WAVE;
    if (riff.smp.loop_flag) {
        /* order matters as tools may rarely include multiple chunks (like smpl + cue/adtl) [Redline (PC)] */
        if (riff.smp.loop_smpl) { /* most common */
            vgmstream->loop_start_sample = riff.smp.loop_start_smpl;
            vgmstream->loop_end_sample = riff.smp.loop_end_smpl + 1;
            vgmstream->meta_type = meta_RIFF_WAVE_smpl;

            // end adds +1 as per spec, but check in case of faulty tools
            if (vgmstream->loop_end_sample - 1 == vgmstream->num_samples)
                vgmstream->loop_end_sample--;
        }
        else if (riff.smp.loop_cue && riff.smp.loop_labl) { /* [Advanced Power Dolls 2 (PC)] */
            /* favor cues as labels are valid but converted samples are slightly off */
            vgmstream->loop_start_sample = riff.smp.loop_start_cue;
            vgmstream->loop_end_sample = riff.smp.loop_end_cue + 1;
            vgmstream->meta_type = meta_RIFF_WAVE_cue;

            // end adds +1 as per spec, but check in case of faulty tools
            if (vgmstream->loop_end_sample - 1 == vgmstream->num_samples)
                vgmstream->loop_end_sample--;
        }
        else if (riff.smp.loop_cue && riff.smp.loop_rgn) { /* [Touhou Suimusou (PC)] */
            vgmstream->loop_start_sample = riff.smp.loop_start_cue;
            vgmstream->loop_end_sample = riff.smp.loop_end_cue;
            vgmstream->meta_type = meta_RIFF_WAVE_cue;
        }
        else if (riff.smp.loop_labl && riff.smp.loop_start_ms >= 0) { /* possible without cue? */
            vgmstream->loop_start_sample = (long long)riff.smp.loop_start_ms * riff.fmt.sample_rate / 1000;
            vgmstream->loop_end_sample = (long long)riff.smp.loop_end_ms * riff.fmt.sample_rate / 1000;
            vgmstream->meta_type = meta_RIFF_WAVE_labl;
        }
        else if (riff.smp.loop_cue) { /* [Team Fortress 2 (PC), Portal (PC)] */
            /* in Source engine ignores the loop end cue; usually doesn't set labl/ltxt (seen "loop" label in Portal) */
            vgmstream->loop_start_sample = riff.smp.loop_start_cue;
            vgmstream->loop_end_sample = vgmstream->num_samples;
            vgmstream->meta_type = meta_RIFF_WAVE_cue;
        }
        else if (riff.smp.loop_ctrl && riff.fmt.coding_type == coding_LEVEL5) {
            vgmstream->loop_start_sample = riff.smp.loop_start_ctrl;
            vgmstream->loop_end_sample = vgmstream->num_samples;
            vgmstream->meta_type = meta_RIFF_WAVE_ctrl;
        }
        else if (riff.smp.loop_wsmp) {
            vgmstream->loop_start_sample = riff.smp.loop_start_wsmp;
            vgmstream->loop_end_sample = riff.smp.loop_end_wsmp;
            vgmstream->meta_type = meta_RIFF_WAVE_wsmp;
        }
        else if (riff.smp.loop_nxbf) {
            switch (riff.fmt.coding_type) {
                case coding_PCM16LE:
                    vgmstream->loop_start_sample = pcm16_bytes_to_samples(riff.smp.loop_start_nxbf, vgmstream->channels);
                    vgmstream->loop_end_sample = vgmstream->num_samples;
                    break;
                default:
                    break;
            }
        }
    }

    if (!vgmstream_open_stream(vgmstream, sf, riff.data_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

static bool is_ue4_msadpcm_blocks(STREAMFILE* sf, riff_fmt_t* fmt, uint32_t offset, uint32_t data_size) {
    uint32_t max_offset = 10 * fmt->block_size; /* try N blocks */
    if (max_offset > offset + data_size)
        max_offset = offset + data_size;

    /* UE4 encoder doesn't calculate optimal coefs and uses certain values every frame.
     * Implicitly this should reject stereo frames (not used in UE4), that have scale/coefs in different positions. */
    while (offset < max_offset) {
        uint8_t coefs = read_u8(offset+0x00, sf);
        uint16_t scale = read_u16le(offset+0x01, sf);

        /* mono frames should only fill the lower bits (4b index) */
        if (coefs > 0x07)
            return false;

        /* size 0x36 always uses scale 0x00E6 and coefs 0x00 while size 0x32 usually does, except for early
         * games where it may use more standard values [2013: Infected Wars (iPhone)] */
        if (fmt->block_size == 0x200) {
            if (scale == 0x00E6 && coefs != 0x00)
                return false;
        }
        else {
            if (scale > 0x4000) { /* observed max (high scales exists) */
                VGM_LOG("RIFF: unexpected UE4 MSADPCM scale=%x\n", scale);
                return false;
            }
        }

        offset += fmt->block_size;
    }

    return true;
}

/* UE4 MSADPCM has a few minor quirks we can use to detect it */
static bool is_ue4_msadpcm(STREAMFILE* sf, riff_fmt_t* fmt, int fact_sample_count, off_t start, uint32_t data_size) {

    /* UE4 allows >=2ch (sample rate may be anything), while mono files are just regular MSADPCM */
    if (fmt->channels < 2)
        return false;

    /* UE4 encoder doesn't add "fact" while regular encoders usually do (but not always) */
    if (fact_sample_count != 0)
        return false;

    /* later UE4 versions use fmt size 0x36 (unlike standard MSADPCM's 0x32), and only certain block sizes */
    if (fmt->size == 0x36) {
        if (!(fmt->block_size == 0x200))
            return false;
    }
    else if (fmt->size == 0x32) {
        /* other than 0x200 is rarely used [2013: Infected Wars (iPhone)] */
        if (!(fmt->block_size == 0x200 || fmt->block_size == 0x9b || fmt->block_size == 0x69))
            return false;

        /* could do it for fmt size 0x36 too but not important */
        if (!is_ue4_msadpcm_blocks(sf, fmt, start, data_size))
            return false;
    }
    else {
        return false;
    }

    /* UE4's class is "ADPCM", assume it's the extension too (also safer since can't tell UE4 MSADPCM from .wav ADPCM in some cases) */
    if (!check_extensions(sf, "adpcm"))
        return false;

    return true;
}

/* for maximum annoyance later UE4 versions (~v4.2x?) interleave single frames instead of
 * half interleave, but don't have flags to detect so we need some heuristics. Most later
 * games with 0x36 chunk size use v2_interleave but notably Travis Strikes Again doesn't */
static size_t get_ue4_msadpcm_interleave(STREAMFILE* sf, riff_fmt_t* fmt, off_t start, size_t size) {
    size_t v1_interleave = size / fmt->channels;
    size_t v2_interleave = fmt->block_size;
    uint8_t nibbles_half[0x20] = {0};
    uint8_t nibbles_full[0x20] = {0};
    int nibbles_size = sizeof(nibbles_full);
    uint8_t empty[0x20] = {0};
    int is_blank_half, is_blank_full;


    /* old versions */
    if (fmt->size == 0x32)
        return v1_interleave;

    /* 6ch only observed in later versions [Fortnite (PC)], not padded */
    if (fmt->channels > 2 || fmt->channels < 2)
        return v2_interleave;

    read_streamfile(nibbles_half, start + v1_interleave - nibbles_size, nibbles_size, sf);
    is_blank_half = memcmp(nibbles_half, empty, nibbles_size) == 0;

    read_streamfile(nibbles_full, start + size - nibbles_size, nibbles_size, sf);
    is_blank_full = memcmp(nibbles_full, empty, nibbles_size) == 0;

    /* last frame is almost always padded, so should at half interleave */
    if (!is_blank_half && !is_blank_full)
        return v1_interleave;

    /* last frame is padded, and half interleave is not: should be regular interleave */
    if (!is_blank_half && is_blank_full)
        return v2_interleave;

    /* last frame is silent-ish, so should at half interleave (TSA's SML_DarknessLoop_01, TSA_CAD_YAKATA)
     * this doesn't work too well b/c num_samples at 0x36 uses all data, may need adjustment */
    {
        int empty_nibbles_full = 1, empty_nibbles_half = 1;

        for (int i = 0; i < sizeof(nibbles_full); i++) {
            uint8_t n1 = ((nibbles_full[i] >> 0) & 0x0f);
            uint8_t n2 = ((nibbles_full[i] >> 4) & 0x0f);
            if ((n1 != 0x0 && n1 != 0xf && n1 != 0x1) || (n2 != 0x0 && n2 != 0xf && n2 != 0x1)) {
                empty_nibbles_full = 0;
                break;
            }
        }

        for (int i = 0; i < sizeof(nibbles_half); i++) {
            uint8_t n1 = ((nibbles_half[i] >> 0) & 0x0f);
            uint8_t n2 = ((nibbles_half[i] >> 4) & 0x0f);
            if ((n1 != 0x0 && n1 != 0xf && n1 != 0x1) || (n2 != 0x0 && n2 != 0xf && n2 != 0x1)) {
                empty_nibbles_half = 0;
                break;
            }
        }

        if (empty_nibbles_full && empty_nibbles_half)
            return v1_interleave;
    }

    /* other tests? */

    return v2_interleave; /* favor newer games */
}

#if 0
// Big endian RIFX is in the spec but no known games use it. Probably defined for powerPC Mac
// that were big endian, but seems Mac ports of games around that era used AIFC or regular RIFF from PC.
// This meta was added for Wwise, but now wwise.c handles it; to be removed later unless actual cases are found.

/* same but big endian, seen in the spec */
VGMSTREAM* init_vgmstream_rifx(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    riff_fmt_t fmt = {0};

    size_t file_size, riff_size, data_size = 0;
    off_t start_offset = 0;

    int loop_flag = 0;
    off_t loop_start_offset = -1;
    off_t loop_end_offset = -1;

    int FormatChunkFound = 0, DataChunkFound = 0;


    /* checks */
    if (!is_id32be(0x00,sf, "RIFX"))
        goto fail;

    if (!check_extensions(sf, "wav,lwav"))
        goto fail;

    if (!is_id32be(0x08,sf, "WAVE"))
        goto fail;

    riff_size = read_u32be(0x04,sf);
    file_size = get_streamfile_size(sf);

    /* check for truncated RIFF */
    if (file_size < riff_size+8) goto fail;

    /* read through chunks to verify format and find metadata */
    {
        off_t chunk_offset = 0xc; /* start with first chunk */

        while (chunk_offset < file_size && chunk_offset < riff_size+8) {
            uint32_t chunk_type = read_u32be(chunk_offset+0x00,sf);
            uint32_t chunk_size = read_u32be(chunk_offset+0x04,sf);

            if (chunk_offset + 0x08 + chunk_size > file_size)
                goto fail;
            chunk_offset += 0x08;

            switch(chunk_type) {
                case 0x666d7420:    /* "fmt " */
                    /* only one per file */
                    if (FormatChunkFound) goto fail;
                    FormatChunkFound = 1;

                    if (!parse_fmt(&fmt, sf, chunk_offset, chunk_size, true))
                        goto fail;

                    break;
                case 0x64617461:    /* data */
                    /* at most one per file */
                    if (DataChunkFound) goto fail;
                    DataChunkFound = 1;

                    start_offset = chunk_offset;
                    data_size = chunk_size;
                    break;
                case 0x736D706C:    /* smpl */
                    /* check loop count and loop info */
                    if (read_u32be(chunk_offset+0x1C, sf) == 1) {
                        if (read_u32be(chunk_offset + 0x24 + 0x04, sf)==0) {
                            loop_flag = 1;
                            loop_start_offset = read_u32be(chunk_offset + 0x24 + 0x08, sf);
                            loop_end_offset = read_u32be(chunk_offset + 0x24 + 0x0c,sf) + 1;
                        }
                    }
                    break;
                default:
                    /* ignorance is bliss */
                    break;
            }

            chunk_offset += chunk_size;
        }
    }

    if (!FormatChunkFound || !DataChunkFound)
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(fmt.channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = fmt.sample_rate;

    /* init, samples */
    switch (fmt.coding_type) {
        case coding_PCM16BE:
        case coding_PCM8_U:
            vgmstream->num_samples = pcm_bytes_to_samples(data_size, vgmstream->channels, fmt.bps);
            break;
        default:
            goto fail;
    }

    /* coding, layout, interleave */
    vgmstream->coding_type = fmt.coding_type;
    switch (fmt.coding_type) {
        default:
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = fmt.interleave;
            break;
    }

    /* meta, loops */
    vgmstream->meta_type = meta_RIFX_WAVE;
    if (loop_flag) {
        if (loop_start_offset >= 0) {
            vgmstream->loop_start_sample = loop_start_offset;
            vgmstream->loop_end_sample = loop_end_offset;
            /* end must add +1, but check in case of faulty tools */
            if (vgmstream->loop_end_sample - 1 == vgmstream->num_samples)
                vgmstream->loop_end_sample--;

            vgmstream->meta_type = meta_RIFX_WAVE_smpl;
        }
    }


    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
#endif
