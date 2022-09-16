#include <string.h>
#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "../util.h"
#include "riff_ogg_streamfile.h"

/* RIFF - Resource Interchange File Format, standard container used in many games */


/* return milliseconds */
static long parse_adtl_marker(unsigned char* marker) {
    long hh,mm,ss,ms;

    if (memcmp("Marker ",marker,7)) return -1;

    if (4 != sscanf((char*)marker+7,"%ld:%ld:%ld.%ld",&hh,&mm,&ss,&ms))
        return -1;

    return ((hh*60+mm)*60+ss)*1000+ms;
}

/* loop points have been found hiding here */
static void parse_adtl(off_t adtl_offset, off_t adtl_length, STREAMFILE* sf, long* loop_start, long* loop_end, int* loop_flag) {
    int loop_start_found = 0;
    int loop_end_found = 0;
    off_t current_chunk = adtl_offset+0x04;

    while (current_chunk < adtl_offset + adtl_length) {
        uint32_t chunk_type = read_u32be(current_chunk+0x00,sf);
        uint32_t chunk_size = read_u32le(current_chunk+0x04,sf);

        if (current_chunk+0x08+chunk_size > adtl_offset+adtl_length)
            return;

        switch(chunk_type) {
            case 0x6c61626c: { /* "labl" */
                unsigned char *labelcontent = malloc(chunk_size-0x04);
                if (!labelcontent) return;
                if (read_streamfile(labelcontent,current_chunk+0x0c, chunk_size-0x04,sf) != chunk_size-0x04) {
                    free(labelcontent);
                    return;
                }

                switch (read_32bitLE(current_chunk+8,sf)) {
                    case 1:
                        if (!loop_start_found && (*loop_start = parse_adtl_marker(labelcontent)) >= 0)
                            loop_start_found = 1;
                        break;
                    case 2:
                        if (!loop_end_found && (*loop_end = parse_adtl_marker(labelcontent)) >= 0)
                            loop_end_found = 1;
                        break;
                    default:
                        break;
                }

                free(labelcontent);
                break;
            }
            default:
                break;
        }

        current_chunk += 8 + chunk_size;
    }

    if (loop_start_found && loop_end_found)
        *loop_flag = 1;

    /* labels don't seem to be consistently ordered */
    if (*loop_start > *loop_end) {
        long temp = *loop_start;
        *loop_start = *loop_end;
        *loop_end = temp;
    }
}

typedef struct {
    off_t offset;
    off_t size;
    uint32_t codec;
    int sample_rate;
    int channels;
    uint32_t block_size;
    int bps;
    off_t extra_size;
    uint32_t channel_layout;

    int coding_type;
    int interleave;

    int is_at3;
    int is_at3p;
    int is_at9;
} riff_fmt_chunk;

static int read_fmt(int big_endian, STREAMFILE* sf, off_t offset, riff_fmt_chunk* fmt, int mwv) {
    uint32_t (*read_u32)(off_t,STREAMFILE*) = big_endian ? read_u32be : read_u32le;
    uint16_t (*read_u16)(off_t,STREAMFILE*) = big_endian ? read_u16be : read_u16le;

    fmt->offset = offset;
    fmt->size = read_u32(offset+0x04,sf);

    /* WAVEFORMAT */
    fmt->codec          = read_u16(offset+0x08+0x00,sf);
    fmt->channels       = read_u16(offset+0x08+0x02,sf);
    fmt->sample_rate    = read_u32(offset+0x08+0x04,sf);
  //fmt->avg_bps        = read_u32(offset+0x08+0x08,sf);
    fmt->block_size     = read_u16(offset+0x08+0x0c,sf);
    fmt->bps            = read_u16(offset+0x08+0x0e,sf);
    /* WAVEFORMATEX */
    if (fmt->size >= 0x10) {
        fmt->extra_size = read_u16(offset+0x08+0x10,sf);
        /* 0x1a+ depends on codec (ex. coef table for MSADPCM, samples_per_frame in MS-IMA, etc) */
    }
    /* WAVEFORMATEXTENSIBLE */
    if (fmt->codec == 0xFFFE && fmt->extra_size >= 0x16) {
      //fmt->extra_samples  = read_u16(offset+0x08+0x12,sf); /* valid_bits_per_sample or samples_per_block */
        fmt->channel_layout = read_u32(offset+0x08+0x14,sf);
        /* 0x10 guid at 0x20 */

        /* happens in various .at3/at9, may be a bug in their encoder b/c MS's defs set mono as FC */
        if (fmt->channels == 1 && fmt->channel_layout == speaker_FL) { /* other channels are fine */
            fmt->channel_layout = speaker_FC;
        }

        /* happens in few at3p, may be a bug in older tools as other games have ok flags [Ridge Racer 7 (PS3)] */
        if (fmt->channels == 6 && fmt->channel_layout == 0x013f) {
            fmt->channel_layout = 0x3f;
        }
    }

    if (!fmt->channels)
        goto fail;

    switch (fmt->codec) {
        case 0x0000:  /* Yamaha AICA ADPCM [Headhunter (DC), Bomber hehhe (DC), Rayman 2 (DC)] (unofficial) */
            if (fmt->bps != 4) goto fail;
            if (fmt->block_size != 0x02*fmt->channels &&
                fmt->block_size != 0x01*fmt->channels) goto fail;
            fmt->coding_type = coding_AICA_int;
            fmt->interleave = 0x01;
            break;

        case 0x0001: /* PCM */
            switch (fmt->bps) {
                case 24: /* Omori (PC) */
                    fmt->coding_type = coding_PCM24LE;
                    break;
                case 16:
                    fmt->coding_type = big_endian ? coding_PCM16BE : coding_PCM16LE;
                    /* broken block size [Rayman 2 (DC)] */
                    if (fmt->block_size == 0x02 && fmt->channels > 1)
                        fmt->block_size = 0x02 * fmt->channels;
                    break;
                case 8:
                    fmt->coding_type = coding_PCM8_U;
                    break;
                default:
                    goto fail;
            }
            fmt->interleave = fmt->block_size / fmt->channels;
            break;

        case 0x0002: /* MSADPCM */
            if (fmt->bps == 4) {
                fmt->coding_type = coding_MSADPCM;
                if (!msadpcm_check_coefs(sf, fmt->offset + 0x08 + 0x14))
                    goto fail;
            }
            else if (fmt->bps == 16 && fmt->block_size == 0x02 * fmt->channels && fmt->size == 0x14) {
                fmt->coding_type = coding_IMA; /* MX vs ATV Unleashed (PC) codec hijack */
            }
            else {
                goto fail;
            }
            break;

        case 0x0011:  /* MS-IMA ADPCM [Layton Brothers: Mystery Room (iOS/Android)] */
            if (fmt->bps != 4) goto fail;
            fmt->coding_type = coding_MS_IMA;
            break;

        case 0x0020:  /* Yamaha AICA ADPCM [Takuyo/Dynamix/etc DC games] (official-ish) */
            if (fmt->bps != 4) goto fail;
            fmt->coding_type = coding_AICA;
            /* official RIFF spec has 0x20 as 'Yamaha ADPCM', but data is probably not pure AICA
             * (maybe with headered frames and would need extra detection?) */
            break;

#ifdef VGM_USE_MPEG
        case 0x0055: /* MP3 [Bear in the Big Blue House: Bear's Imagine That! (PC)] (official) */
            fmt->coding_type = coding_MPEG_custom;
            /* some oddities, unsure if part of standard: 
             * - block size is 1 (in mono)
             * - bps is 16
             * - extra size 0x0c, has channels? and (possibly) approx frame size */
            break;
#endif

        case 0x0069:  /* XBOX IMA ADPCM [Dynasty Warriors 5 (Xbox)] */
            if (fmt->bps != 4) goto fail;
            fmt->coding_type = coding_XBOX_IMA;
            break;

        case 0x007A:  /* MS IMA ADPCM [LA Rush (PC), Psi Ops (PC)] (unofficial) */
            /* 0x007A is apparently "Voxware SC3" but in .MED it's just MS-IMA (0x11) */
            if (!check_extensions(sf,"med"))
                goto fail;

            if (fmt->bps == 4) /* normal MS IMA */
                fmt->coding_type = coding_MS_IMA;
            else if (fmt->bps == 3) /* 3-bit MS IMA, used in a very few files */
                goto fail; //fmt->coding_type = coding_MS_IMA_3BIT;
            else
                goto fail;
            break;

        case 0x0300:  /* IMA ADPCM [Chrono Ma:gia (Android)] (unofficial) */
            if (fmt->bps != 4) goto fail;
            if (fmt->block_size != 0x0400*fmt->channels) goto fail;
            if (fmt->size != 0x14) goto fail;
            if (fmt->channels != 1) goto fail; /* not seen */
            fmt->coding_type = coding_DVI_IMA;
            /* real 0x300 is "Fujitsu FM Towns SND" with block align 0x01 */
            break;

        case 0x0555: /* Level-5 0x555 ADPCM (unofficial) */
            if (!mwv) goto fail;
            fmt->coding_type = coding_L5_555;
            fmt->interleave = 0x12;
            break;

#ifdef VGM_USE_VORBIS
      //case 0x674f: /* Ogg Vorbis (mode 1) */
      //case 0x6750: /* Ogg Vorbis (mode 2) */
      //case 0x6751: /* Ogg Vorbis (mode 3) */
        case 0x676f: /* Ogg Vorbis (mode 1+) [Only One 2 (PC)] */
        case 0x6770: /* Ogg Vorbis (mode 2+) [Only One (PC)]*/
        case 0x6771: /* Ogg Vorbis (mode 3+) [Liar-soft games] */
            /* vorbis.acm codecs (official-ish, "+" = CBR-style modes?) */
            fmt->coding_type = coding_OGG_VORBIS;
            break;
#endif

#ifdef VGM_USE_FFMPEG
        case 0x0270: /* ATRAC3 */
            fmt->coding_type = coding_FFmpeg;
            fmt->is_at3 = 1;
            break;
#endif

        case 0xFFFE: { /* WAVEFORMATEXTENSIBLE (see ksmedia.h for known GUIDs) */
            uint32_t guid1 = read_u32  (offset+0x20,sf);
            uint32_t guid2 = (read_u16 (offset+0x24,sf) << 16u) |
                             (read_u16 (offset+0x26,sf));
            uint32_t guid3 = read_u32be(offset+0x28,sf);
            uint32_t guid4 = read_u32be(offset+0x2c,sf);
            //;VGM_LOG("riff: guid %08x %08x %08x %08x\n", guid1, guid2, guid3, guid4);

            /* PCM GUID (0x00000001,0000,0010,80,00,00,AA,00,38,9B,71) */
            if (guid1 == 0x00000001 && guid2 == 0x00000010 && guid3 == 0x800000AA && guid4 == 0x00389B71) {
                switch (fmt->bps) {
                    case 16:
                        fmt->coding_type = big_endian ? coding_PCM16BE : coding_PCM16LE;
                        fmt->interleave = 0x02;
                        break;
                    default:
                        goto fail;
                }
                break;
            }

            /* ATRAC3plus GUID (0xE923AABF,CB58,4471,A1,19,FF,FA,01,E4,CE,62) */
            if (guid1 == 0xE923AABF && guid2 == 0xCB584471 && guid3 == 0xA119FFFA && guid4 == 0x01E4CE62) {
#ifdef VGM_USE_FFMPEG
                fmt->coding_type = coding_FFmpeg;
                fmt->is_at3p = 1;
                break;
#elif defined(VGM_USE_MAIATRAC3PLUS)
                uint16_t bztmp = read_u16(offset+0x32,sf);
                bztmp = (bztmp >> 8) | (bztmp << 8);
                fmt->coding_type = coding_AT3plus;
                fmt->block_size = (bztmp & 0x3FF) * 8 + 8; /* should match fmt->block_size */
                fmt->is_at3p = 1;
                break;
#else
                goto fail;
#endif
            }

#ifdef VGM_USE_ATRAC9
            /* ATRAC9 GUID (0x47E142D2,36BA,4D8D,88,FC,61,65,4F,8C,83,6C) */
            if (guid1 == 0x47E142D2 && guid2 == 0x36BA4D8D && guid3 == 0x88FC6165 && guid4 == 0x4F8C836C) {
                fmt->coding_type = coding_ATRAC9;
                fmt->is_at9 = 1;
                break;
            }
#endif

            goto fail; /* unknown GUID */
        }

        default:
            /* FFmpeg may play it */
            //vgm_logi("RIFF: unknown codec 0x%04x (report)\n", fmt->format);
            goto fail;
    }

    return 1;

fail:
    return 0;
}

static int is_ue4_msadpcm(STREAMFILE* sf, riff_fmt_chunk* fmt, int fact_sample_count, off_t start_offset);
static size_t get_ue4_msadpcm_interleave(STREAMFILE* sf, riff_fmt_chunk* fmt, off_t start, size_t size);


VGMSTREAM* init_vgmstream_riff(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    riff_fmt_chunk fmt = {0};

    size_t file_size, riff_size, data_size = 0;
    off_t start_offset = 0;

    int fact_sample_count = 0;
    int fact_sample_skip = 0;

    int loop_flag = 0;
    long loop_start_ms = -1, loop_end_ms = -1;
    int32_t loop_start_wsmp = -1, loop_end_wsmp = -1;
    int32_t loop_start_smpl = -1, loop_end_smpl = -1;
    int32_t loop_start_cue = -1;
    int32_t loop_start_nxbf = -1;

    int FormatChunkFound = 0, DataChunkFound = 0, JunkFound = 0;

    int mwv = 0;
    off_t mwv_pflt_offset = -1;
    off_t mwv_ctrl_offset = -1;
    int ignore_riff_size = 0;


    /* checks*/
    if (!is_id32be(0x00,sf,"RIFF"))
        goto fail;

    /* .lwav: to avoid hijacking .wav
     * .xwav: fake for Xbox games (not needed anymore)
     * .da: The Great Battle VI (PS1)
     * .dax: Love Game's - Wai Wai Tennis (PS1)
     * .cd: Exector (PS)
     * .med: Psi Ops (PC)
     * .snd: Layton Brothers (iOS/Android)
     * .adx: Remember11 (PC) sfx
     * .adp: Headhunter (DC)
     * .xss: Spider-Man The Movie (Xbox)
     * .xsew: Mega Man X Legacy Collection (PC)
     * .adpcm: Angry Birds Transformers (Android)
     * .adw: Dead Rising 2 (PC)
     * .wd: Genma Onimusha (Xbox) voices
     * (extensionless): Myst III (Xbox)
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
     */
    if ( check_extensions(sf, "wav,lwav,xwav,da,dax,cd,med,snd,adx,adp,xss,xsew,adpcm,adw,wd,,sbv,wvx,str,at3,rws,aud,at9,ckd,saf,ima,nsa,pcm,xvag,ogg,logg,p1d") ) {
        ;
    }
    else if ( check_extensions(sf, "mwv") ) {
        mwv = 1;
    }
    else {
        goto fail;
    }

    riff_size = read_u32le(0x04,sf);

    if (!is_id32be(0x08,sf, "WAVE"))
        goto fail;

    file_size = get_streamfile_size(sf);

    /* some games have wonky sizes, selectively fix to catch bad rips and new mutations */
    if (file_size != riff_size + 0x08) {
        uint16_t codec = read_u16le(0x14,sf);

        if      ((codec & 0xFF00) == 0x6700 && riff_size + 0x08 + 0x01 == file_size)
            riff_size += 0x01; /* [Shikkoku no Sharnoth (PC), Only One 2 (PC)] (Sony Sound Forge?) */

        else if (codec == 0x0069 && riff_size == file_size)
            riff_size -= 0x08; /* [Dynasty Warriors 3 (Xbox), BloodRayne (Xbox)] */

        else if (codec == 0x0069 && riff_size + 0x04 == file_size)
            riff_size -= 0x04; /* [Halo 2 (PC)] (possibly bad extractor? 'Gravemind Tool') */

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

        else if (mwv) {
            int channels = read_u16le(0x16, sf); /* [Dragon Quest VIII (PS2), Rogue Galaxy (PS2)] */
            size_t file_size_fixed = riff_size + 0x08 + 0x04 * (channels - 1);

            if (file_size_fixed <= file_size && file_size - file_size_fixed < 0x10) {
                /* files inside HD6/DAT are also padded to 0x10 so need to fix file_size */
                file_size = file_size_fixed;
                riff_size = file_size - 0x08;
            }
        }

        else if (riff_size >= file_size && read_32bitBE(0x24,sf) == 0x4E584246) /* "NXBF" */
            riff_size = file_size - 0x08; /* [R:Racing Evolution (Xbox)] */

        else if (codec == 0x0011 && (riff_size / 2 / 2 == read_32bitLE(0x30,sf))) /* riff_size = pcm_size (always stereo, has fact at 0x30) */
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
            ignore_riff_size = 1; /* [Cross Gate (PC)] (last info LIST chunk has wrong size) */

        else if (codec == 0xFFFE && riff_size + 0x08 + 0x40 == file_size)
            file_size -= 0x40; /* [Megami no Etsubo (PSP)] (has extra padding in all files) */
    }

    /* check for truncated RIFF */
    if (file_size != riff_size + 0x08 && !ignore_riff_size) {
        vgm_logi("RIFF: wrong expected size (report/re-rip?)\n");
        VGM_LOG("riff: file_size = %x, riff_size+8 = %x\n", file_size, riff_size + 0x08); /* don't log to user */
        goto fail;
    }

    /* read through chunks to verify format and find metadata */
    {
        uint32_t current_chunk = 0x0c; /* start with first chunk */

        while (current_chunk < file_size) {
            uint32_t chunk_id = read_u32be(current_chunk + 0x00,sf); /* FOURCC */
            uint32_t chunk_size = read_u32le(current_chunk + 0x04,sf);

            /* allow broken last chunk [Cross Gate (PC)] */
            if (current_chunk + 0x08 + chunk_size > file_size) {
                VGM_LOG("RIFF: broken chunk at %x + 0x08 + %x > %x\n", current_chunk, chunk_size, file_size);
                break; /* truncated */
            }

            switch(chunk_id) {
                case 0x666d7420:    /* "fmt " */
                    if (FormatChunkFound) goto fail; /* only one per file */
                    FormatChunkFound = 1;

                    if (!read_fmt(0, sf, current_chunk, &fmt, mwv))
                        goto fail;

                    /* some Dreamcast/Naomi games again [Headhunter (DC), Bomber hehhe (DC), Rayman 2 (DC)] */
                    if (fmt.codec == 0x0000 && chunk_size == 0x12)
                        chunk_size += 0x02;
                    break;

                case 0x64617461:    /* "data" */
                    if (DataChunkFound) goto fail; /* only one per file */
                    DataChunkFound = 1;

                    start_offset = current_chunk + 0x08;
                    data_size = chunk_size;
                    break;

                case 0x4C495354:    /* "LIST" */
                    switch (read_32bitBE(current_chunk+0x08, sf)) {
                        case 0x6164746C:    /* "adtl" */
                            /* yay, atdl is its own little world */
                            parse_adtl(current_chunk + 0x8, chunk_size,
                                    sf,
                                    &loop_start_ms,&loop_end_ms,&loop_flag);
                            break;
                        default:
                            break;
                    }
                    break;

                case 0x736D706C:    /* "smpl" (RIFFMIDISample + MIDILoop chunk) */
                    /* check loop count/loop info (most common) */
                    /* 0x00: manufacturer id, 0x04: product id, 0x08: sample period, 0x0c: unity node,
                     * 0x10: pitch fraction, 0x14: SMPTE format, 0x18: SMPTE offset, 0x1c: loop count, 0x20: sampler data */
                    if (read_32bitLE(current_chunk+0x08+0x1c, sf) == 1) { /* handle only one loop (could contain N MIDILoop) */
                        /* 0x24: cue point id, 0x28: type (0=forward, 1=alternating, 2=backward)
                         * 0x2c: start, 0x30: end, 0x34: fraction, 0x38: play count */
                        if (read_32bitLE(current_chunk+0x08+0x28, sf) == 0) { /* loop forward */
                            loop_flag = 1;
                            loop_start_smpl = read_32bitLE(current_chunk+0x08+0x2c, sf);
                            loop_end_smpl   = read_32bitLE(current_chunk+0x08+0x30, sf) + 1; /* must add 1 as per spec (ok for standard WAV/AT3/AT9) */
                        }
                    }
                    break;

                case 0x77736D70:    /* "wsmp" (RIFFDLSSample + DLSLoop chunk)  */
                    /* check loop count/info (found in some Xbox games: Halo (non-looping), Dynasty Warriors 3, Crimson Sea) */
                    /* 0x00: size, 0x04: unity note, 0x06: fine tune, 0x08: gain, 0x10: loop count */
                    if (chunk_size >= 0x24
                            && read_32bitLE(current_chunk+0x08+0x00, sf) == 0x14
                            && read_32bitLE(current_chunk+0x08+0x10, sf) > 0
                            && read_32bitLE(current_chunk+0x08+0x14, sf) == 0x10) {
                        /* 0x14: size, 0x18: loop type (0=forward, 1=release), 0x1c: loop start, 0x20: loop length */
                        if (read_32bitLE(current_chunk+0x08+0x18, sf) == 0) { /* loop forward */
                            loop_flag = 1;
                            loop_start_wsmp = read_32bitLE(current_chunk+0x08+0x1c, sf);
                            loop_end_wsmp   = read_32bitLE(current_chunk+0x08+0x20, sf); /* must not add 1 as per spec */
                            loop_end_wsmp  += loop_start_wsmp;
                        }
                    }
                    break;

                case 0x66616374:    /* "fact" */
                    if (chunk_size == 0x04) { /* standard (usually for ADPCM, MS recommends setting for non-PCM codecs but optional) */
                        fact_sample_count = read_32bitLE(current_chunk+0x08, sf);
                    }
                    else if (chunk_size == 0x10 && is_id32be(current_chunk+0x08+0x04, sf, "LyN ")) {
                        goto fail; /* parsed elsewhere */
                    }
                    else if ((fmt.is_at3 || fmt.is_at3p) && chunk_size == 0x08) { /* early AT3 (mainly PSP games) */
                        fact_sample_count = read_32bitLE(current_chunk+0x08, sf);
                        fact_sample_skip  = read_32bitLE(current_chunk+0x0c, sf); /* base skip samples */
                    }
                    else if ((fmt.is_at3 || fmt.is_at3p) && chunk_size == 0x0c) { /* late AT3 (mainly PS3 games and few PSP games) */
                        fact_sample_count = read_32bitLE(current_chunk+0x08, sf);
                        /* 0x0c: base skip samples, ignored by decoder */
                        fact_sample_skip  = read_32bitLE(current_chunk+0x10, sf); /* skip samples with extra 184 */
                    }
                    else if (fmt.is_at9 && chunk_size == 0x0c) {
                        fact_sample_count = read_32bitLE(current_chunk+0x08, sf);
                        /* 0x0c: base skip samples (same as next field) */
                        fact_sample_skip  = read_32bitLE(current_chunk+0x10, sf);
                    }
                    break;

                case 0x4C795345:    /* "LySE" */
                    goto fail; /* parsed elsewhere */

                case 0x70666c74:    /* "pflt" (.mwv extension) */
                    if (!mwv) break;    /* ignore if not in an mwv */
                    mwv_pflt_offset = current_chunk; /* predictor filters */
                    break;

                case 0x6374726c:    /* "ctrl" (.mwv extension) */
                    if (!mwv) break;
                    loop_flag = read_32bitLE(current_chunk+0x08, sf);
                    mwv_ctrl_offset = current_chunk;
                    break;

                case 0x63756520:    /* "cue " (used in Source Engine for storing loop points) */
                    if (fmt.coding_type == coding_PCM8_U ||
                        fmt.coding_type == coding_PCM16LE ||
                        fmt.coding_type == coding_MSADPCM) {
                        uint32_t num_cues = read_32bitLE(current_chunk + 0x08, sf);

                        if (num_cues > 0) {
                            /* the second cue sets loop end point but it's not actually used by the engine */
                            loop_flag = 1;
                            loop_start_cue = read_32bitLE(current_chunk + 0x20, sf);
                        }
                    }
                    break;

                case 0x4E584246:    /* "NXBF" (Namco NuSound v1) [R:Racing Evolution (Xbox)] */
                    /* very similar to NUS's NPSF, but not quite like Cstr */
                    /* 0x00: "NXBF" id */
                    /* 0x04: version? (0x00001000 = 1.00?) */
                    /* 0x08: data size */
                    /* 0x0c: channels */
                    /* 0x10: null */
                    loop_start_nxbf = read_32bitLE(current_chunk + 0x08 + 0x14, sf);
                    /* 0x18: sample rate */
                    /* 0x1c: volume? (0x3e8 = 1000 = max) */
                    /* 0x20: type/flags? */
                    /* 0x24: flag? */
                    /* 0x28: null */
                    /* 0x2c: null */
                    /* 0x30: always 0x40 */
                    loop_flag = (loop_start_nxbf >= 0);
                    break;

                case 0x4A554E4B:    /* "JUNK" */
                    JunkFound = 1;
                    break;


                case 0x64737068: /* "dsph" */
                case 0x63776176: /* "cwav" */
                    goto fail; /* parse elsewhere */

                default:
                    /* ignorance is bliss */
                    break;
            }

            /* chunks are even-sized with padding byte (for 16b reads) as per spec (normally
             * pre-adjusted except for a few like Liar-soft's), at end may not have padding though
             * (done *after* chunk parsing since size without padding is needed) */
            if (chunk_size % 0x02 && current_chunk + 0x08 + chunk_size+0x01 <= file_size)
                chunk_size += 0x01;

            current_chunk += 0x08 + chunk_size;
        }
    }

    if (!FormatChunkFound || !DataChunkFound) goto fail;

    //todo improve detection using fmt sizes/values as Wwise's don't match the RIFF standard
    /* JUNK is an optional Wwise chunk, and Wwise hijacks the MSADPCM/MS_IMA/XBOX IMA ids (how nice).
     * To ensure their stuff is parsed in wwise.c we reject their JUNK, which they put almost always.
     * As JUNK is legal (if unusual) we only reject those codecs.
     * (ex. Cave PC games have PCM16LE + JUNK + smpl created by "Samplitude software") */
    if (JunkFound
            && check_extensions(sf,"wav,lwav") /* for some .MED IMA */
            && (fmt.coding_type==coding_MSADPCM /*|| fmt.coding_type==coding_MS_IMA*/ || fmt.coding_type==coding_XBOX_IMA))
        goto fail;

    /* ignore Beyond Good & Evil HD PS3 evil reuse of PCM codec */
    if (fmt.coding_type == coding_PCM16LE &&
            read_u32be(start_offset+0x00, sf) == 0x4D534643 && /* "MSF\43" */
            read_u32be(start_offset+0x34, sf) == 0xFFFFFFFF && /* always */
            read_u32be(start_offset+0x38, sf) == 0xFFFFFFFF &&
            read_u32be(start_offset+0x3c, sf) == 0xFFFFFFFF)
        goto fail;

    /* MSADPCM .ckd are parsed elsewhere, though they are valid so no big deal if parsed here (just that loops should be ignored) */
    if (fmt.codec == 0x0002 && check_extensions(sf, "ckd"))
        goto fail;

    /* ignore Gitaroo Man Live! (PSP) multi-RIFF (to allow chunked TXTH) */
    if (fmt.is_at3 && get_streamfile_size(sf) > 0x2800 && read_32bitBE(0x2800, sf) == 0x52494646) { /* "RIFF" */
        goto fail;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(fmt.channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = fmt.sample_rate;
    vgmstream->channel_layout = fmt.channel_layout;

    /* coding, layout, interleave */
    vgmstream->coding_type = fmt.coding_type;
    switch (fmt.coding_type) {
        case coding_MS_IMA:
        case coding_AICA:
        case coding_XBOX_IMA:
        case coding_IMA:
        case coding_DVI_IMA:
#ifdef VGM_USE_FFMPEG
        case coding_FFmpeg:
#endif
#ifdef VGM_USE_MAIATRAC3PLUS
        case coding_AT3plus:
#endif
#ifdef VGM_USE_ATRAC9
        case coding_ATRAC9:
#endif
#ifdef VGM_USE_VORBIS
        case coding_OGG_VORBIS:
#endif
            vgmstream->layout_type = layout_none;
            vgmstream->interleave_block_size = fmt.block_size;
            break;
#ifdef VGM_USE_MPEG
        case coding_MPEG_custom:
            vgmstream->layout_type = layout_none;
            break;
#endif
        case coding_MSADPCM:
            vgmstream->layout_type = layout_none;
            vgmstream->frame_size = fmt.block_size;
            break;

        default:
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = fmt.interleave;
            break;
    }

    /* samples, codec init (after setting coding to ensure proper close on failure) */
    switch (fmt.coding_type) {
        case coding_PCM24LE:
        case coding_PCM16LE:
        case coding_PCM8_U:
            vgmstream->num_samples = pcm_bytes_to_samples(data_size, fmt.channels, fmt.bps);
            break;

        case coding_L5_555:
            if (!mwv) goto fail;
            vgmstream->num_samples = data_size / 0x12 / fmt.channels * 32;

            /* coefs */
            {
                int i, ch;
                const int filter_order = 3;
                int filter_count = read_32bitLE(mwv_pflt_offset+0x0c, sf);
                if (filter_count > 0x20) goto fail;

                if (mwv_pflt_offset == -1 ||
                        read_32bitLE(mwv_pflt_offset+0x08, sf) != filter_order ||
                        read_32bitLE(mwv_pflt_offset+0x04, sf) < 8 + filter_count * 4 * filter_order)
                    goto fail;

                for (ch = 0; ch < fmt.channels; ch++) {
                    for (i = 0; i < filter_count * filter_order; i++) {
                        int coef = read_32bitLE(mwv_pflt_offset+0x10+i*0x04, sf);
                        vgmstream->ch[ch].adpcm_coef_3by32[i] = coef;
                    }
                }
            }

            break;

        case coding_MSADPCM:
            vgmstream->num_samples = msadpcm_bytes_to_samples(data_size, fmt.block_size, fmt.channels);
            if (fact_sample_count && fact_sample_count < vgmstream->num_samples)
                vgmstream->num_samples = fact_sample_count;
            break;

        case coding_MS_IMA:
            vgmstream->num_samples = ms_ima_bytes_to_samples(data_size, fmt.block_size, fmt.channels);
            if (fact_sample_count && fact_sample_count < vgmstream->num_samples)
                vgmstream->num_samples = fact_sample_count;
            break;

        case coding_AICA:
        case coding_AICA_int:
            vgmstream->num_samples = yamaha_bytes_to_samples(data_size, fmt.channels);
            break;

        case coding_XBOX_IMA:
            vgmstream->num_samples = xbox_ima_bytes_to_samples(data_size, fmt.channels);
            if (fact_sample_count && fact_sample_count < vgmstream->num_samples)
                vgmstream->num_samples = fact_sample_count; /* some (converted?) Xbox games have bigger fact_samples */
            break;

        case coding_IMA:
        case coding_DVI_IMA:
            vgmstream->num_samples = ima_bytes_to_samples(data_size, fmt.channels);
            break;

#ifdef VGM_USE_FFMPEG
        case coding_FFmpeg: {
            if (!fmt.is_at3 && !fmt.is_at3p) goto fail;

            vgmstream->codec_data = init_ffmpeg_atrac3_riff(sf, 0x00, NULL);
            if (!vgmstream->codec_data) goto fail;

            vgmstream->num_samples = fact_sample_count;
            if (loop_flag) {
                /* adjust RIFF loop/sample absolute values (with skip samples) */
                loop_start_smpl -= fact_sample_skip;
                loop_end_smpl   -= fact_sample_skip;

                /* happens with official tools when "fact" is not found */
                if (vgmstream->num_samples == 0)
                    vgmstream->num_samples = loop_end_smpl;
            }

            break;
        }
#endif
#ifdef VGM_USE_MAIATRAC3PLUS
        case coding_AT3plus: {
            vgmstream->codec_data = init_at3plus();

            /* get rough total samples but favor fact_samples if available (skip isn't correctly handled for now) */
            vgmstream->num_samples = atrac3plus_bytes_to_samples(data_size, fmt.block_size);
            if (fact_sample_count > 0 && fact_sample_count + fact_sample_skip < vgmstream->num_samples)
                vgmstream->num_samples = fact_sample_count + fact_sample_skip;
            break;
        }
#endif
#ifdef VGM_USE_ATRAC9
        case coding_ATRAC9: {
            atrac9_config cfg = {0};

            cfg.channels = vgmstream->channels;
            cfg.config_data = read_32bitBE(fmt.offset+0x08+0x2c,sf);
            cfg.encoder_delay = fact_sample_skip;

            vgmstream->codec_data = init_atrac9(&cfg);
            if (!vgmstream->codec_data) goto fail;

            vgmstream->num_samples = fact_sample_count;
            /* RIFF loop/sample values are absolute (with skip samples), adjust */
            if (loop_flag) {
                loop_start_smpl -= fact_sample_skip;
                loop_end_smpl   -= fact_sample_skip;
            }

            break;
        }
#endif
#ifdef VGM_USE_VORBIS
        case coding_OGG_VORBIS: {
            /* special handling of Liar-soft's buggy RIFF+Ogg made with Soundforge/vorbis.acm [Shikkoku no Sharnoth (PC)],
             * and rarely other devs, not always buggy [Kirara Kirara NTR (PC), No One 2 (PC)] */
            STREAMFILE* temp_sf = setup_riff_ogg_streamfile(sf, start_offset, data_size);
            if (!temp_sf) goto fail;

            vgmstream->codec_data = init_ogg_vorbis(temp_sf, 0x00, get_streamfile_size(temp_sf), NULL);
            if (!vgmstream->codec_data) goto fail;

            /* Soundforge includes fact_samples and should be equal to Ogg samples */
            vgmstream->num_samples = fact_sample_count;
            break;
        }
#endif

#ifdef VGM_USE_MPEG
        case coding_MPEG_custom: {
            mpeg_custom_config cfg = {0};

            vgmstream->codec_data = init_mpeg_custom(sf, start_offset, &vgmstream->coding_type, fmt.channels, MPEG_STANDARD, &cfg);
            if (!vgmstream->codec_data) goto fail;

            /* should provide "fact" but it's optional (some game files don't include it) */
            if (!fact_sample_count)
                fact_sample_count = mpeg_get_samples(sf, start_offset, data_size);
            vgmstream->num_samples = fact_sample_count;
        }
        break;
#endif

        default:
            goto fail;
    }

    /* UE4 uses interleaved mono MSADPCM, try to autodetect without breaking normal MSADPCM */
    if (fmt.coding_type == coding_MSADPCM && is_ue4_msadpcm(sf, &fmt, fact_sample_count, start_offset)) {
        vgmstream->coding_type = coding_MSADPCM_int;
        vgmstream->codec_config = 1; /* mark as UE4 MSADPCM */
        vgmstream->frame_size = fmt.block_size;
        vgmstream->layout_type = layout_interleave;
        vgmstream->interleave_block_size = get_ue4_msadpcm_interleave(sf, &fmt, start_offset, data_size);
        if (fmt.size == 0x36)
            vgmstream->num_samples = read_s32le(fmt.offset+0x08+0x32, sf);
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
    if (loop_flag) {
        if (loop_start_ms >= 0) {
            vgmstream->loop_start_sample = (long long)loop_start_ms*fmt.sample_rate/1000;
            vgmstream->loop_end_sample = (long long)loop_end_ms*fmt.sample_rate/1000;
            vgmstream->meta_type = meta_RIFF_WAVE_labl;
        }
        else if (loop_start_smpl >= 0) {
            vgmstream->loop_start_sample = loop_start_smpl;
            vgmstream->loop_end_sample = loop_end_smpl;
            /* end must add +1, but check in case of faulty tools */
            if (vgmstream->loop_end_sample - 1 == vgmstream->num_samples)
                vgmstream->loop_end_sample--;

            vgmstream->meta_type = meta_RIFF_WAVE_smpl;
        }
        else if (loop_start_wsmp >= 0) {
            vgmstream->loop_start_sample = loop_start_wsmp;
            vgmstream->loop_end_sample = loop_end_wsmp;
            vgmstream->meta_type = meta_RIFF_WAVE_wsmp;
        }
        else if (mwv && mwv_ctrl_offset != -1) {
            vgmstream->loop_start_sample = read_32bitLE(mwv_ctrl_offset+12, sf);
            vgmstream->loop_end_sample = vgmstream->num_samples;
        }
        else if (loop_start_cue != -1) {
            vgmstream->loop_start_sample = loop_start_cue;
            vgmstream->loop_end_sample = vgmstream->num_samples;
        }
        else if (loop_start_nxbf != -1) {
            switch (fmt.coding_type) {
                case coding_PCM16LE:
                    vgmstream->loop_start_sample = pcm_bytes_to_samples(loop_start_nxbf, vgmstream->channels, 16);
                    vgmstream->loop_end_sample = vgmstream->num_samples;
                    break;
                default:
                    break;
            }
        }
    }
    if (mwv) {
        vgmstream->meta_type = meta_RIFF_WAVE_MWV;
    }

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* UE4 MSADPCM is quite normal but has a few minor quirks we can use to detect it */
static int is_ue4_msadpcm(STREAMFILE* sf, riff_fmt_chunk* fmt, int fact_sample_count, off_t start) {

    /* multichannel ok */
    if (fmt->channels < 2)
        goto fail;

    /* UE4 class is "ADPCM", assume it's the extension too */
    if (!check_extensions(sf, "adpcm"))
        goto fail;

    /* UE4 encoder doesn't add "fact" */
    if (fact_sample_count != 0)
        goto fail;

    /* fixed block size */
    if (fmt->block_size != 0x200)
        goto fail;

    /* later UE4 versions use 0x36 */
    if (fmt->size != 0x32 && fmt->size != 0x36)
        goto fail;

    /* size 0x32 in older UE4 matches standard MSADPCM, so add extra detection */
    if (fmt->size == 0x32) {
        off_t offset = start;
        off_t max_offset = 5 * fmt->block_size; /* try N blocks */
        if (max_offset > get_streamfile_size(sf))
            max_offset = get_streamfile_size(sf);

        /* their encoder doesn't calculate optimal coefs and uses fixed values every frame
         * (could do it for fmt size 0x36 too but maybe they'll fix it in the future) */
        while (offset <= max_offset) {
            if (read_u8(offset+0x00, sf) != 0 || read_u16le(offset+0x01, sf) != 0x00E6)
                goto fail;
            offset += fmt->block_size;
        }
    }

    return 1;
fail:
    return 0;
}

/* for maximum annoyance later UE4 versions (~v4.2x?) interleave single frames instead of
 * half interleave, but don't have flags to detect so we need some heuristics. Most later
 * games with 0x36 chunk size use v2_interleave but notably Travis Strikes Again doesn't  */
static size_t get_ue4_msadpcm_interleave(STREAMFILE* sf, riff_fmt_chunk* fmt, off_t start, size_t size) {
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
    if (!is_blank_half && !is_blank_full) {
        return v1_interleave;
    }

    /* last frame is padded, and half interleave is not: should be regular interleave*/
    if (!is_blank_half && is_blank_full) {
        return v2_interleave;
    }

    /* last frame is silent-ish, so should at half interleave (TSA's SML_DarknessLoop_01, TSA_CAD_YAKATA)
     * this doesn't work too well b/c num_samples at 0x36 uses all data, may need adjustment */
    {

        int i;
        int empty_nibbles_full = 1, empty_nibbles_half = 1;

        for (i = 0; i < sizeof(nibbles_full); i++) {
            uint8_t n1 = ((nibbles_full[i] >> 0) & 0x0f);
            uint8_t n2 = ((nibbles_full[i] >> 4) & 0x0f);
            if ((n1 != 0x0 && n1 != 0xf && n1 != 0x1) || (n2 != 0x0 && n2 != 0xf && n2 != 0x1)) {
                empty_nibbles_full = 0;
                break;
            }
        }

        for (i = 0; i < sizeof(nibbles_half); i++) {
            uint8_t n1 = ((nibbles_half[i] >> 0) & 0x0f);
            uint8_t n2 = ((nibbles_half[i] >> 4) & 0x0f);
            if ((n1 != 0x0 && n1 != 0xf && n1 != 0x1) || (n2 != 0x0 && n2 != 0xf && n2 != 0x1)) {
                empty_nibbles_half = 0;
                break;
            }
        }

        if (empty_nibbles_full && empty_nibbles_half){
            VGM_LOG("v1 b\n");
            return v1_interleave;
        }
    }

    /* other tests? */

    return v2_interleave; /* favor newer games */
}

/* same but big endian, seen in the spec and in Kitchenette (PC) (possibly from Adobe Director) */
VGMSTREAM* init_vgmstream_rifx(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    riff_fmt_chunk fmt = {0};

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
        off_t current_chunk = 0xc; /* start with first chunk */

        while (current_chunk < file_size && current_chunk < riff_size+8) {
            uint32_t chunk_type = read_32bitBE(current_chunk,sf);
            off_t chunk_size = read_32bitBE(current_chunk+4,sf);

            if (current_chunk+8+chunk_size > file_size) goto fail;

            switch(chunk_type) {
                case 0x666d7420:    /* "fmt " */
                    /* only one per file */
                    if (FormatChunkFound) goto fail;
                    FormatChunkFound = 1;

                    if (!read_fmt(1, sf, current_chunk, &fmt, 0))
                        goto fail;

                    break;
                case 0x64617461:    /* data */
                    /* at most one per file */
                    if (DataChunkFound) goto fail;
                    DataChunkFound = 1;

                    start_offset = current_chunk + 8;
                    data_size = chunk_size;
                    break;
                case 0x736D706C:    /* smpl */
                    /* check loop count and loop info */
                    if (read_32bitBE(current_chunk+0x24, sf)==1) {
                        if (read_32bitBE(current_chunk+0x2c+4, sf)==0) {
                            loop_flag = 1;
                            loop_start_offset = read_32bitBE(current_chunk+0x2c+8, sf);
                            loop_end_offset = read_32bitBE(current_chunk+0x2c+0xc,sf) + 1;
                        }
                    }
                    break;
                default:
                    /* ignorance is bliss */
                    break;
            }

            current_chunk += 8+chunk_size;
        }
    }

    if (!FormatChunkFound || !DataChunkFound) goto fail;


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
