#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "ea_eaac_streamfile.h"

/* EAAudioCore formats, EA's current audio middleware */

#define EAAC_VERSION_V0                 0x00 /* SNR/SNS */
#define EAAC_VERSION_V1                 0x01 /* SPS */

#define EAAC_CODEC_NONE                 0x00 /* internal 'codec not set' */
#define EAAC_CODEC_RESERVED             0x01 /* not used/reserved? /MP30/P6L0/P2B0/P2L0/P8S0/P8U0/PFN0? */
#define EAAC_CODEC_PCM                  0x02
#define EAAC_CODEC_EAXMA                0x03
#define EAAC_CODEC_XAS                  0x04
#define EAAC_CODEC_EALAYER3_V1          0x05
#define EAAC_CODEC_EALAYER3_V2_PCM      0x06
#define EAAC_CODEC_EALAYER3_V2_SPIKE    0x07
#define EAAC_CODEC_DSP                  0x08
#define EAAC_CODEC_EASPEEX              0x09
#define EAAC_CODEC_EATRAX               0x0a
#define EAAC_CODEC_EAOPUS               0x0c

#define EAAC_FLAG_NONE                  0x00
#define EAAC_FLAG_LOOPED                0x02
#define EAAC_FLAG_STREAMED              0x04

#define EAAC_BLOCKID0_DATA              0x00
#define EAAC_BLOCKID0_END               0x80 /* maybe meant to be a bitflag? */

#define EAAC_BLOCKID1_HEADER            0x48 /* 'H' */
#define EAAC_BLOCKID1_DATA              0x44 /* 'D' */
#define EAAC_BLOCKID1_END               0x45 /* 'E' */

static VGMSTREAM * init_vgmstream_eaaudiocore_header(STREAMFILE * streamHead, STREAMFILE * streamData, off_t header_offset, off_t start_offset, meta_t meta_type);
static size_t get_snr_size(STREAMFILE *streamFile, off_t offset);
static VGMSTREAM *parse_s10a_header(STREAMFILE *streamFile, off_t offset, uint16_t target_index, off_t ast_offset);



/* .SNR+SNS - from EA latest games (~2008-2013), v0 header */
VGMSTREAM * init_vgmstream_ea_snr_sns(STREAMFILE * streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamData = NULL;

    /* check extension, case insensitive */
    if (!check_extensions(streamFile,"snr"))
        goto fail;

    /* SNR headers normally need an external SNS file, but some have data [Burnout Paradise, NFL2013 (iOS)] */
    if (get_streamfile_size(streamFile) > 0x10) {
        off_t start_offset = get_snr_size(streamFile, 0x00);
        vgmstream = init_vgmstream_eaaudiocore_header(streamFile, streamFile, 0x00, start_offset, meta_EA_SNR_SNS);
        if (!vgmstream) goto fail;
    }
    else {
        streamData = open_streamfile_by_ext(streamFile,"sns");
        if (!streamData) goto fail;

        vgmstream = init_vgmstream_eaaudiocore_header(streamFile, streamData, 0x00, 0x00, meta_EA_SNR_SNS);
        if (!vgmstream) goto fail;
    }

    close_streamfile(streamData);
    return vgmstream;

fail:
    close_streamfile(streamData);
    return NULL;
}

/* .SPS - from EA latest games (~2014), v1 header */
VGMSTREAM * init_vgmstream_ea_sps(STREAMFILE * streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;

    /* check extension, case insensitive */
    if (!check_extensions(streamFile,"sps"))
        goto fail;

    /* SPS block start: 0x00(1): block flag (header=0x48); 0x01(3): block size (usually 0x0c-0x14) */
    if (read_8bit(0x00, streamFile) != EAAC_BLOCKID1_HEADER)
        goto fail;
    start_offset = read_32bitBE(0x00, streamFile) & 0x00FFFFFF;

    vgmstream = init_vgmstream_eaaudiocore_header(streamFile, streamFile, 0x04, start_offset, meta_EA_SPS);
    if (!vgmstream) goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* .SNU - from EA Redwood Shores/Visceral games (Dead Space, Dante's Inferno, The Godfather 2), v0 header */
VGMSTREAM * init_vgmstream_ea_snu(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, header_offset;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;

    /* check extension, case insensitive */
    if (!check_extensions(streamFile,"snu"))
        goto fail;

    /* EA SNU header (BE/LE depending on platform) */
    /* 0x00(1): related to sample rate? (03=48000)
     * 0x01(1): flags/count? (when set has extra block data before start_offset)
     * 0x02(1): always 0?
     * 0x03(1): channels? (usually matches but rarely may be 0)
     * 0x04(4): some size, maybe >>2 ~= number of frames
     * 0x08(4): start offset
     * 0x0c(4): some sub-offset? (0x20, found when @0x01 is set) */

    /* use start_offset as endianness flag */
    if (guess_endianness32bit(0x08,streamFile)) {
        read_32bit = read_32bitBE;
    } else {
        read_32bit = read_32bitLE;
    }

    header_offset = 0x10; /* SNR header */
    start_offset = read_32bit(0x08,streamFile); /* SPS blocks */

    vgmstream = init_vgmstream_eaaudiocore_header(streamFile, streamFile, header_offset, start_offset, meta_EA_SNU);
    if (!vgmstream) goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* .SPS - from Frostbite engine games, v1 header */
VGMSTREAM * init_vgmstream_ea_sps_fb(STREAMFILE *streamFile) { //todo remove in the future, use better extractors
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset = 0, header_offset = 0, sps_offset, max_offset;

    /* checks */
    /* should be .sps once extracted (filenames are hashed) */
    if (!check_extensions(streamFile,"sps"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x011006C0 &&  /* Need for Speed: The Run (PS3), Need for Speed: Rivals (PS4) */
        read_32bitBE(0x00,streamFile) != 0x01100180 &&  /* Need for Speed: The Run (X360) */
        read_32bitBE(0x00,streamFile) != 0x01100000)    /* Need for Speed: The Run (PC) */
        goto fail;

    /* file has a Frostbite descriptor (SoundWaveAsset segments) data before actual .sps, exact size unknown.
     * 0x00: segments/flags/sizes? 0x04: SegmentLength?, 0x08: SeekTableOffset?, 0x0c: mini SPS header
     * rest: unknown fields? may be padded? (ex. 0x22 > 0x24, 0x1d > 0x20 */

    /* actual offsets are probably somewhere but for now just manually search. */
    sps_offset = read_32bitBE(0x08, streamFile); /* seek table, number of entries unknown */
    max_offset = sps_offset + 0x3000;
    if (max_offset > get_streamfile_size(streamFile))
        max_offset = get_streamfile_size(streamFile);

    /* find .sps start block */
    while (sps_offset < max_offset) {
        if ((read_32bitBE(sps_offset, streamFile) & 0xFFFFFF00) == 0x48000000) {
            header_offset = sps_offset + 0x04;
            start_offset = sps_offset + (read_32bitBE(sps_offset, streamFile) & 0x00FFFFFF);
            break;
        }
        sps_offset += 0x04;
    }

    if (!start_offset)
        goto fail; /* not found */

    vgmstream = init_vgmstream_eaaudiocore_header(streamFile, streamFile, header_offset, start_offset, meta_EA_SPS);
    if (!vgmstream) goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* EA ABK - ABK header seems to be same as in the old games but the sound table is different and it contains SNR/SNS sounds instead */
VGMSTREAM * init_vgmstream_ea_abk_new(STREAMFILE *streamFile) {
    int is_dupe, total_sounds = 0, target_stream = streamFile->stream_index;
    off_t bnk_offset, header_table_offset, base_offset, unk_struct_offset, table_offset, snd_entry_offset, ast_offset;
    off_t num_entries_off, base_offset_off, entries_off, sound_table_offset_off;
    uint32_t i, j, k, num_sounds, total_sound_tables;
    uint16_t num_tables, bnk_index, bnk_target_index;
    uint8_t num_entries, extra_entries;
    off_t sound_table_offsets[0x2000];
    VGMSTREAM *vgmstream;
    int32_t (*read_32bit)(off_t,STREAMFILE*);
    int16_t (*read_16bit)(off_t,STREAMFILE*);

    /* check extension */
    if (!check_extensions(streamFile, "abk"))
        goto fail;

    if (read_32bitBE(0x00, streamFile) != 0x41424B43) /* "ABKC" */
        goto fail;

    /* use table offset to check endianness */
    if (guess_endianness32bit(0x1C,streamFile)) {
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
    } else {
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
    }

    if (target_stream == 0) target_stream = 1;
    if (target_stream < 0)
        goto fail;

    num_tables = read_16bit(0x0A, streamFile);
    header_table_offset = read_32bit(0x1C, streamFile);
    bnk_offset = read_32bit(0x20, streamFile);
    total_sound_tables = 0;
    bnk_target_index = 0xFFFF;
    ast_offset = 0;

    if (!bnk_offset || read_32bitBE(bnk_offset, streamFile) != 0x53313041) /* "S10A" */
        goto fail;

    /* set up some common values */
    if (header_table_offset == 0x5C) {
        /* the usual variant */
        num_entries_off = 0x24;
        base_offset_off = 0x2C;
        entries_off = 0x3C;
        sound_table_offset_off = 0x04;
    }
    else if (header_table_offset == 0x78) {
        /* FIFA 08 has a bunch of extra zeroes all over the place, don't know what's up with that */
        num_entries_off = 0x40;
        base_offset_off = 0x54;
        entries_off = 0x68;
        sound_table_offset_off = 0x0C;
    }
    else {
        goto fail;
    }

    for (i = 0; i < num_tables; i++) {
        num_entries = read_8bit(header_table_offset + num_entries_off, streamFile);
        extra_entries = read_8bit(header_table_offset + num_entries_off + 0x03, streamFile);
        base_offset = read_32bit(header_table_offset + base_offset_off, streamFile);
        if (num_entries == 0xff) goto fail; /* EOF read */

        for (j = 0; j < num_entries; j++) {
            unk_struct_offset = read_32bit(header_table_offset + entries_off + 0x04 * j, streamFile);
            table_offset = read_32bit(base_offset + unk_struct_offset + sound_table_offset_off, streamFile);

            /* For some reason, there are duplicate entries pointing at the same sound tables */
            is_dupe = 0;
            for (k = 0; k < total_sound_tables; k++)
            {
                if (table_offset==sound_table_offsets[k])
                {
                    is_dupe = 1;
                    break;
                }
            }

            if (is_dupe)
                continue;

            sound_table_offsets[total_sound_tables++] = table_offset;
            num_sounds = read_32bit(table_offset, streamFile);
            if (num_sounds == 0xffffffff) goto fail; /* EOF read */

            for (k = 0; k < num_sounds; k++) {
                /* 0x00: sound index */
                /* 0x02: ??? */
                /* 0x04: ??? */
                /* 0x08: streamed data offset */
                snd_entry_offset = table_offset + 0x04 + 0x0C * k;
                bnk_index = read_16bit(snd_entry_offset + 0x00, streamFile);

                /* some of these are dummies */
                if (bnk_index == 0xFFFF)
                    continue;

                total_sounds++;
                if (target_stream == total_sounds) {
                    bnk_target_index = bnk_index;
                    ast_offset = read_32bit(snd_entry_offset + 0x08, streamFile);
                }
            }
        }

        header_table_offset += entries_off + num_entries * 0x04 + extra_entries * 0x04;
    }

    if (bnk_target_index == 0xFFFF || ast_offset == 0)
        goto fail;
    
    vgmstream = parse_s10a_header(streamFile, bnk_offset, bnk_target_index, ast_offset);
    if (!vgmstream)
        goto fail;

    vgmstream->num_streams = total_sounds;
    return vgmstream;

fail:
    return NULL;
}

/* EA S10A header - seen inside new ABK files. Putting it here in case it's encountered stand-alone. */
static VGMSTREAM * parse_s10a_header(STREAMFILE *streamFile, off_t offset, uint16_t target_index, off_t ast_offset) {
    uint32_t num_sounds;
    off_t snr_offset, sns_offset;
    STREAMFILE *astFile = NULL;
    VGMSTREAM *vgmstream;

    /* header is always big endian */
    /* 0x00: header magic */
    /* 0x04: zero */
    /* 0x08: number of files */
    /* 0x0C: offsets table */
    if (read_32bitBE(offset + 0x00, streamFile) != 0x53313041) /* "S10A" */
        goto fail;

    num_sounds = read_32bitBE(offset + 0x08, streamFile);
    if (num_sounds == 0 || target_index > num_sounds)
        goto fail;

    snr_offset = offset + read_32bitBE(offset + 0x0C + 0x04 * target_index, streamFile);

    if (ast_offset == 0xFFFFFFFF) {
        /* RAM asset */
        sns_offset = snr_offset + get_snr_size(streamFile, snr_offset);
        //;VGM_LOG("EA S10A: RAM at sns=%lx, sns=%lx\n", snr_offset, sns_offset);
        vgmstream = init_vgmstream_eaaudiocore_header(streamFile, streamFile, snr_offset, sns_offset, meta_EA_SNR_SNS);
        if (!vgmstream)
            goto fail;
    }
    else {
        /* streamed asset */
        astFile = open_streamfile_by_ext(streamFile, "ast");
        if (!astFile)
            goto fail;

        if (read_32bitBE(0x00, astFile) != 0x53313053) /* "S10S" */
            goto fail;

        sns_offset = ast_offset;
        //;VGM_LOG("EA S10A: stream at sns=%lx, sns=%lx\n", snr_offset, sns_offset);
        vgmstream = init_vgmstream_eaaudiocore_header(streamFile, astFile, snr_offset, sns_offset, meta_EA_SNR_SNS);
        if (!vgmstream)
            goto fail;

        close_streamfile(astFile);
    }

    return vgmstream;

fail:
    close_streamfile(astFile);
    return NULL;
}

/* EA HDR/STH/DAT - seen in early 7th-gen games, used for storing speech */
VGMSTREAM * init_vgmstream_ea_hdr_sth_dat(STREAMFILE *streamFile) {
    int target_stream = streamFile->stream_index;
    uint32_t i;
    uint8_t userdata_size, total_sounds, block_id;
    off_t snr_offset, sns_offset;
    size_t file_size, block_size;
    STREAMFILE *datFile = NULL, *sthFile = NULL;
    VGMSTREAM *vgmstream;

    /* 0x00: ID */
    /* 0x02: userdata size */
    /* 0x03: number of files */
    /* 0x04: sub-ID (used for different police voices in NFS games) */
    /* 0x08: alt number of files? */
    /* 0x09: zero */
    /* 0x0A: ??? */
    /* 0x0C: zero */
    /* 0x10: table start */

    sthFile = open_streamfile_by_ext(streamFile, "sth");
    if (!sthFile)
        goto fail;

    datFile = open_streamfile_by_ext(streamFile, "dat");
    if (!datFile)
        goto fail;

    /* STH always starts with the first offset of zero */
    sns_offset = read_32bitLE(0x00, sthFile);
    if (sns_offset != 0)
        goto fail;

    /* check if DAT starts with a correct SNS block */
    block_id = read_8bit(0x00, datFile);
    if (block_id != EAAC_BLOCKID0_DATA && block_id != EAAC_BLOCKID0_END)
        goto fail;

    userdata_size = read_8bit(0x02, streamFile);
    total_sounds = read_8bit(0x03, streamFile);
    if (read_8bit(0x08, streamFile) > total_sounds)
        goto fail;

    if (target_stream == 0) target_stream = 1;
    if (target_stream < 0 || total_sounds == 0 || target_stream > total_sounds)
        goto fail;

    /* offsets in HDR are always big endian */
    //snr_offset = (off_t)read_16bitBE(0x10 + (0x02+userdata_size) * (target_stream-1), streamFile) + 0x04;
    //sns_offset = read_32bit(snr_offset, sthFile);

    /* we can't reliably detect byte endianness so we're going to find the sound the hacky way */
    /* go through blocks until we reach the goal sound */
    file_size = get_streamfile_size(datFile);
    snr_offset = 0;
    sns_offset = 0;

    for (i = 0; i < total_sounds; i++) {
        snr_offset = (off_t)read_16bitBE(0x10 + (0x02+userdata_size) * i, streamFile) + 0x04;

        if (i == target_stream - 1)
            break;

        while (1) {
            if (sns_offset >= file_size)
                goto fail;

            block_id = read_8bit(sns_offset, datFile);
            block_size = read_32bitBE(sns_offset, datFile) & 0x00FFFFFF;
            if (block_size == 0)
                goto fail;

            if (block_id != EAAC_BLOCKID0_DATA && block_id != EAAC_BLOCKID0_END)
                goto fail;

            sns_offset += block_size;

            if (block_id == EAAC_BLOCKID0_END)
                break;
        }
    }

    block_id = read_8bit(sns_offset, datFile);
    if (block_id != EAAC_BLOCKID0_DATA && block_id != EAAC_BLOCKID0_END)
        goto fail;

    vgmstream = init_vgmstream_eaaudiocore_header(sthFile, datFile, snr_offset, sns_offset, meta_EA_SNR_SNS);
    if (!vgmstream)
        goto fail;

    vgmstream->num_streams = total_sounds;
    close_streamfile(sthFile);
    close_streamfile(datFile);
    return vgmstream;

fail:
    close_streamfile(sthFile);
    close_streamfile(datFile);
    return NULL;
}

/* ************************************************************************* */

typedef struct {
    int version;
    int codec;
    int channel_config;
    int sample_rate;
    int flags;

    int streamed;
    int channels;

    int num_samples;
    int loop_start;
    int loop_end;
    int loop_flag;

    off_t stream_offset;
    off_t loop_offset;
} eaac_header;

static segmented_layout_data* build_segmented_eaaudiocore_looping(STREAMFILE *streamData, eaac_header *eaac);
static layered_layout_data* build_layered_eaaudiocore_eaxma(STREAMFILE *streamFile, eaac_header *eaac);


/* EA newest header from RwAudioCore (RenderWare?) / EAAudioCore library (still generated by sx.exe).
 * Audio "assets" come in separate RAM headers (.SNR/SPH) and raw blocked streams (.SNS/SPS),
 * or together in pseudoformats (.SNU, .SBR+.SBS banks, .AEMS, .MUS, etc).
 * Some .SNR include stream data, while .SPS have headers so .SPH is optional. */
static VGMSTREAM * init_vgmstream_eaaudiocore_header(STREAMFILE * streamHead, STREAMFILE * streamData, off_t header_offset, off_t start_offset, meta_t meta_type) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE* temp_streamFile = NULL;
    uint32_t header1, header2;
    eaac_header eaac = {0};


    /* EA SNR/SPH header */
    header1 = (uint32_t)read_32bitBE(header_offset + 0x00, streamHead);
    header2 = (uint32_t)read_32bitBE(header_offset + 0x04, streamHead);
    eaac.version = (header1 >> 28) & 0x0F; /* 4 bits */
    eaac.codec   = (header1 >> 24) & 0x0F; /* 4 bits */
    eaac.channel_config = (header1 >> 18) & 0x3F; /* 6 bits */
    eaac.sample_rate = (header1 & 0x03FFFF); /* 18 bits (some Dead Space 2 (PC) do use 96000) */
    eaac.flags = (header2 >> 28) & 0x0F; /* 4 bits *//* TODO: maybe even 3 bits and not 4? */
    eaac.num_samples = (header2 & 0x0FFFFFFF); /* 28 bits */
    /* rest is optional, depends on used flags and codec (handled below) */
    eaac.stream_offset = start_offset;

    /* V0: SNR+SNS, V1: SPR+SPS (no apparent differences, other than block flags) */
    if (eaac.version != EAAC_VERSION_V0 && eaac.version != EAAC_VERSION_V1) {
        VGM_LOG("EA EAAC: unknown version\n");
        goto fail;
    }

    /* catch unknown/garbage values just in case */
    if (eaac.flags != EAAC_FLAG_NONE && !(eaac.flags & (EAAC_FLAG_LOOPED | EAAC_FLAG_STREAMED))) {
        VGM_LOG("EA EAAC: unknown flags 0x%02x\n", eaac.flags);
        goto fail;
    }

    /* Non-streamed sounds are stored as a single block (may not set block end flags) */
    eaac.streamed = (eaac.flags & EAAC_FLAG_STREAMED) != 0;

    /* get loops (fairly involved due to the multiple layouts and mutant streamfiles)
     * full loops aren't too uncommon [Dead Space (PC) stream sfx/ambiance, FIFA 98 (PS3) RAM sfx],
     * while actual looping is very rare [Need for Speed: World (PC)] */
    if (eaac.flags & EAAC_FLAG_LOOPED) {
        eaac.loop_flag = 1;
        eaac.loop_start = read_32bitBE(header_offset+0x08, streamHead);
        eaac.loop_end = eaac.num_samples;

        /* RAM assets only have one block, even if they (rarely) set loop_start > 0 */
        if (eaac.streamed)
            eaac.loop_offset = read_32bitBE(header_offset+0x0c, streamHead);
        else
            eaac.loop_offset = eaac.stream_offset; /* implicit */

        //todo EATrax has extra values in header, which would coexist with loop values
        if (eaac.codec == EAAC_CODEC_EATRAX) {
            VGM_LOG("EA EAAC: unknown loop header for EATrax\n");
            goto fail;
        }

        //todo need more cases to test how layout/streamfiles react
        if (eaac.loop_start > 0 && !(eaac.codec == EAAC_CODEC_EALAYER3_V1 ||
                eaac.codec == EAAC_CODEC_EALAYER3_V2_PCM || eaac.codec == EAAC_CODEC_EALAYER3_V2_SPIKE)) {
            VGM_LOG("EA EAAC: unknown actual looping for non-EALayer3\n");
            goto fail;
        }
    }

    /* accepted channel configs only seem to be mono/stereo/quad/5.1/7.1, from debug strings */
    switch(eaac.channel_config) {
        case 0x00: eaac.channels = 1; break;
        case 0x01: eaac.channels = 2; break;
        case 0x03: eaac.channels = 4; break;
        case 0x05: eaac.channels = 6; break;
        case 0x07: eaac.channels = 8; break;
        default:
            VGM_LOG("EA EAAC: unknown channel config 0x%02x\n", eaac.channel_config);
            goto fail; /* fail with unknown values just in case */
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(eaac.channels,eaac.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = eaac.sample_rate;
    vgmstream->num_samples = eaac.num_samples;
    vgmstream->loop_start_sample = eaac.loop_start;
    vgmstream->loop_end_sample = eaac.loop_end;
    vgmstream->meta_type = meta_type;

    /* EA decoder list and known internal FourCCs */
    switch(eaac.codec) {

        case EAAC_CODEC_PCM: /* "P6B0": PCM16BE [NBA Jam (Wii)] */
            vgmstream->coding_type = coding_PCM16_int;
            vgmstream->codec_endian = 1;
            vgmstream->layout_type = layout_blocked_ea_sns;
            break;

#ifdef VGM_USE_FFMPEG
        case EAAC_CODEC_EAXMA: { /* "EXm0": EA-XMA [Dante's Inferno (X360)] */
            vgmstream->layout_data = build_layered_eaaudiocore_eaxma(streamData, &eaac);
            if (!vgmstream->layout_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_layered;
            break;
        }
#endif

        case EAAC_CODEC_XAS: /* "Xas1": EA-XAS [Dead Space (PC/PS3)] */
            vgmstream->coding_type = coding_EA_XAS;
            vgmstream->layout_type = layout_blocked_ea_sns;
            break;

#ifdef VGM_USE_MPEG
        case EAAC_CODEC_EALAYER3_V1:         /* "EL31": EALayer3 v1 [Need for Speed: Hot Pursuit (PS3)] */
        case EAAC_CODEC_EALAYER3_V2_PCM:     /* "L32P": EALayer3 v2 "PCM" [Battlefield 1943 (PS3)] */
        case EAAC_CODEC_EALAYER3_V2_SPIKE: { /* "L32S": EALayer3 v2 "Spike" [Dante's Inferno (PS3)] */
            mpeg_custom_config cfg = {0};
            mpeg_custom_t type = (eaac.codec == 0x05 ? MPEG_EAL31b : (eaac.codec == 0x06) ? MPEG_EAL32P : MPEG_EAL32S);

            /* EALayer3 needs custom IO that removes blocks on reads to fix some edge cases in L32P
             * and to properly apply discard modes (see ealayer3 decoder)
             * (otherwise, and after removing discard code, it'd work with layout_blocked_ea_sns) */

            start_offset = 0x00; /* must point to the custom streamfile's beginning */

            if (eaac.streamed && eaac.loop_start > 0) { /* special (if hacky) loop handling, see comments */
                segmented_layout_data *data = build_segmented_eaaudiocore_looping(streamData, &eaac);
                if (!data) goto fail;
                vgmstream->layout_data = data;
                vgmstream->coding_type = data->segments[0]->coding_type;
                vgmstream->layout_type = layout_segmented;
            }
            else {
                temp_streamFile = setup_eaac_streamfile(streamData, eaac.version, eaac.codec, eaac.streamed,0,0, eaac.stream_offset);
                if (!temp_streamFile) goto fail;

                vgmstream->codec_data = init_mpeg_custom(temp_streamFile, start_offset, &vgmstream->coding_type, vgmstream->channels, type, &cfg);
                if (!vgmstream->codec_data) goto fail;
                vgmstream->layout_type = layout_none;
            }

            break;
        }
#endif

        case EAAC_CODEC_DSP: /* "Gca0"?: DSP [Need for Speed: Nitro (Wii) sfx] */
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_blocked_ea_sns;
            /* DSP coefs are read in the blocks */
            break;

#ifdef VGM_USE_ATRAC9
        case EAAC_CODEC_EATRAX: { /* EATrax (unknown FourCC) [Need for Speed: Most Wanted (Vita)] */
            atrac9_config cfg = {0};

            /* EATrax is "buffered" ATRAC9, uses custom IO since it's kind of complex to add to the decoder */

            start_offset = 0x00; /* must point to the custom streamfile's beginning */

            cfg.channels = eaac.channels;
            cfg.config_data = read_32bitBE(header_offset + 0x08,streamHead);
            /* 0x10: frame size? (same as config data?) */
            /* actual data size without blocks, LE b/c why make sense (but don't use it in case of truncated files) */
            //total_size = read_32bitLE(header_offset + 0x0c,streamHead);

            vgmstream->codec_data = init_atrac9(&cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_ATRAC9;
            vgmstream->layout_type = layout_none;

            temp_streamFile = setup_eaac_streamfile(streamData, eaac.version, eaac.codec, eaac.streamed,0,0, eaac.stream_offset);
            if (!temp_streamFile) goto fail;

            break;
        }
#endif

        case EAAC_CODEC_EASPEEX: /* EASpeex (libspeex variant, base versions vary: 1.0.5, 1.2beta3) */
            /* TODO */
        case EAAC_CODEC_EAOPUS: /* EAOpus (inside each SNS/SPS block is 16b frame size + standard? Opus packet) */
            /* TODO */
        default:
            VGM_LOG("EA EAAC: unknown codec 0x%02x\n", eaac.codec);
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream,temp_streamFile ? temp_streamFile : streamData,start_offset))
        goto fail;
    close_streamfile(temp_streamFile);
    return vgmstream;

fail:
    close_streamfile(temp_streamFile);
    close_vgmstream(vgmstream);
    return NULL;
}

static size_t get_snr_size(STREAMFILE *streamFile, off_t offset) {
    switch (read_8bit(offset + 0x04, streamFile) >> 4 & 0x0F) { /* flags */
    case EAAC_FLAG_LOOPED | EAAC_FLAG_STREAMED:     return 0x10;
    case EAAC_FLAG_LOOPED:                          return 0x0C;
    default:                                        return 0x08;
    }
}


/* Actual looping uses 2 block sections, separated by a block end flag *and* padded.
 *
 * We use the segmented layout, since the eaac_streamfile doesn't handle padding properly ATM
 * (getting EALayer3 frame sizes + skip sizes can be fairly involved), plus seems likely
 * that after a block end the decoder needs to be reset (not possible from a streamfile).
 *
 * Or could fix the blocked_layout+L32P bug, though that involves a lot of rewrites.
 * So this is the simplest, surest way ATM (if very ugly). */
// todo consider better ways to handle this once more looped files for other codecs are found
static segmented_layout_data* build_segmented_eaaudiocore_looping(STREAMFILE *streamData, eaac_header *eaac) {
    segmented_layout_data *data = NULL;
    STREAMFILE* temp_streamFile[2] = {0};
    off_t offsets[2] = { eaac->stream_offset, eaac->loop_offset };
    int num_samples[2] = { eaac->loop_start, eaac->num_samples - eaac->loop_start};
    int segment_count = 2; /* intro/loop */
    int i;


    /* init layout */
    data = init_layout_segmented(segment_count);
    if (!data) goto fail;

    for (i = 0; i < segment_count; i++) {
        temp_streamFile[i] = setup_eaac_streamfile(streamData, eaac->version,eaac->codec,eaac->streamed,0,0, offsets[i]);
        if (!temp_streamFile[i]) goto fail;

        data->segments[i] = allocate_vgmstream(eaac->channels, 0);
        if (!data->segments[i]) goto fail;
        data->segments[i]->sample_rate = eaac->sample_rate;
        data->segments[i]->num_samples = num_samples[i];
        //data->segments[i]->meta_type = eaac->meta_type; /* bleh */

        switch(eaac->codec) {
#ifdef VGM_USE_MPEG
            case EAAC_CODEC_EALAYER3_V1:
            case EAAC_CODEC_EALAYER3_V2_PCM:
            case EAAC_CODEC_EALAYER3_V2_SPIKE: {
                mpeg_custom_config cfg = {0};
                mpeg_custom_t type = (eaac->codec == 0x05 ? MPEG_EAL31b : (eaac->codec == 0x06) ? MPEG_EAL32P : MPEG_EAL32S);

                data->segments[i]->codec_data = init_mpeg_custom(temp_streamFile[i], 0x00, &data->segments[i]->coding_type, eaac->channels, type, &cfg);
                if (!data->segments[i]->codec_data) goto fail;
                data->segments[i]->layout_type = layout_none;
                break;
            }
#endif
            default:
                goto fail;
        }

        if (!vgmstream_open_stream(data->segments[i],temp_streamFile[i],0x00))
            goto fail;
    }

    /* setup segmented VGMSTREAMs */
    if (!setup_layout_segmented(data))
        goto fail;
    return data;

fail:
    for (i = 0; i < segment_count; i++)
        close_streamfile(temp_streamFile[i]);
    free_layout_segmented(data);
    return NULL;
}

static layered_layout_data* build_layered_eaaudiocore_eaxma(STREAMFILE *streamData, eaac_header *eaac) {
    layered_layout_data* data = NULL;
    STREAMFILE* temp_streamFile = NULL;
    int i, layers = (eaac->channels+1) / 2;


    /* init layout */
    data = init_layout_layered(layers);
    if (!data) goto fail;

    /* open each layer subfile (1/2ch streams: 2ch+2ch..+1ch or 2ch+2ch..+2ch).
     * EA-XMA uses completely separate 1/2ch streams, unlike standard XMA that interleaves 1/2ch streams
     * with a skip counter to reinterleave (so EA-XMA streams don't have skips set) */
    for (i = 0; i < layers; i++) {
        int layer_channels = (i+1 == layers && eaac->channels % 2 == 1) ? 1 : 2; /* last layer can be 1/2ch */

        /* build the layer VGMSTREAM */
        data->layers[i] = allocate_vgmstream(layer_channels, eaac->loop_flag);
        if (!data->layers[i]) goto fail;

        data->layers[i]->sample_rate = eaac->sample_rate;
        data->layers[i]->num_samples = eaac->num_samples;
        data->layers[i]->loop_start_sample = eaac->loop_start;
        data->layers[i]->loop_end_sample = eaac->loop_end;

#ifdef VGM_USE_FFMPEG
        {
            uint8_t buf[0x100];
            int bytes, block_size, block_count;
            size_t stream_size;

            temp_streamFile = setup_eaac_streamfile(streamData, eaac->version, eaac->codec, eaac->streamed,i,layers, eaac->stream_offset);
            if (!temp_streamFile) goto fail;

            stream_size = get_streamfile_size(temp_streamFile);
            block_size = 0x10000; /* unused */
            block_count = stream_size / block_size + (stream_size % block_size ? 1 : 0);

            bytes = ffmpeg_make_riff_xma2(buf, 0x100, data->layers[i]->num_samples, stream_size, data->layers[i]->channels, data->layers[i]->sample_rate, block_count, block_size);
            data->layers[i]->codec_data = init_ffmpeg_header_offset(temp_streamFile, buf,bytes, 0x00, stream_size);
            if (!data->layers[i]->codec_data) goto fail;

            data->layers[i]->coding_type = coding_FFmpeg;
            data->layers[i]->layout_type = layout_none;
        }
#else
        goto fail;
#endif

        if ( !vgmstream_open_stream(data->layers[i], temp_streamFile, 0x00) ) {
            goto fail;
        }
    }

    /* setup layered VGMSTREAMs */
    if (!setup_layout_layered(data))
        goto fail;
    close_streamfile(temp_streamFile);
    return data;

fail:
    close_streamfile(temp_streamFile);
    free_layout_layered(data);
    return NULL;
}
