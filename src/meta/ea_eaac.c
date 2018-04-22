#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "ea_eaac_streamfile.h"

/* EAAudioCore formats, EA's current audio middleware */

static VGMSTREAM * init_vgmstream_eaaudiocore_header(STREAMFILE * streamHead, STREAMFILE * streamData, off_t header_offset, off_t start_offset, meta_t meta_type);

/* .SNR+SNS - from EA latest games (~2008-2013), v0 header */
VGMSTREAM * init_vgmstream_ea_snr_sns(STREAMFILE * streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamData = NULL;

    /* check extension, case insensitive */
    if (!check_extensions(streamFile,"snr"))
        goto fail;

    /* SNR headers normally need an external SNS file, but some have data [Burnout Paradise, NFL2013 (iOS)] */
    if (get_streamfile_size(streamFile) > 0x10) {
        off_t start_offset;

        switch(read_8bit(0x04,streamFile)) { /* flags */
            case 0x60: start_offset = 0x10; break;
            case 0x20: start_offset = 0x0c; break;
            default:   start_offset = 0x08; break;
        }

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
    if (read_8bit(0x00, streamFile) != 0x48)
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
    if ((uint32_t)read_32bitLE(0x08,streamFile) > 0x0000FFFF) {
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

/* .SPS? - from Frostbite engine games? [Need for Speed Rivals (PS4)], v1 header */
VGMSTREAM * init_vgmstream_ea_sps_fb(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset = 0, header_offset = 0, sps_offset, max_offset;

    /* checks */
    /* assumed to be .sps (no extensions in the archives) */
    if (!check_extensions(streamFile,"sps"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x011006C0)
        goto fail;


    /* file has some kind of data before .sps, exact offset unknown.
     * Actual offsets are probably somewhere but for now just manually search. */
    sps_offset = read_32bitBE(0x08, streamFile); /* points to some kind of table, number of entries unknown */
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

/* EA newest header from RwAudioCore (RenderWare?) / EAAudioCore library (still generated by sx.exe).
 * Audio "assets" come in separate RAM headers (.SNR/SPH) and raw blocked streams (.SNS/SPS),
 * or together in pseudoformats (.SNU, .SBR+.SBS banks, .AEMS, .MUS, etc).
 * Some .SNR include stream data, while .SPS have headers so .SPH is optional. */
static VGMSTREAM * init_vgmstream_eaaudiocore_header(STREAMFILE * streamHead, STREAMFILE * streamData, off_t header_offset, off_t start_offset, meta_t meta_type) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE* temp_streamFile = NULL;
    int channel_count, loop_flag = 0, version, codec, channel_config, sample_rate, flags;
    uint32_t num_samples, loop_start = 0, loop_end = 0;

    /* EA SNR/SPH header */
    version = (read_8bit(header_offset + 0x00,streamHead) >> 4) & 0x0F;
    codec   = (read_8bit(header_offset + 0x00,streamHead) >> 0) & 0x0F;
    channel_config = read_8bit(header_offset + 0x01,streamHead) & 0xFE;
    sample_rate = read_32bitBE(header_offset + 0x00,streamHead) & 0x1FFFF; /* some Dead Space 2 (PC) uses 96000 */
    flags = (uint8_t)read_8bit(header_offset + 0x04,streamHead) & 0xFE; //todo upper nibble only? (the first bit is part of size)
    num_samples = (uint32_t)read_32bitBE(header_offset + 0x04,streamHead) & 0x01FFFFFF;
    /* rest is optional, depends on flags header used (ex. SNU and SPS may have bigger headers):
     *  &0x20: 1 int (usually 0x00), &0x00/40: nothing, &0x60: 2 ints (usually 0x00 and 0x14) */

    /* V0: SNR+SNS, V1: SPR+SPS (not apparent differences, other than the block flags used) */
    if (version != 0 && version != 1) {
        VGM_LOG("EA SNS/SPS: unknown version\n");
        goto fail;
    }

    /* 0x40: stream asset, 0x20: full loop, 0x00: default/RAM asset */
    if (flags != 0x60 && flags != 0x40 && flags != 0x20 && flags != 0x00) {
        VGM_LOG("EA SNS/SPS: unknown flag 0x%02x\n", flags);
        goto fail;
    }

    /* seen in sfx and Dead Space ambient tracks */
    if (flags & 0x20) {
        loop_flag = 1;
        loop_start = 0;
        loop_end = num_samples;
    }

    /* accepted channel configs only seem to be mono/stereo/quad/5.1/7.1 */
    //channel_count = ((channel_config >> 2) & 0xf) + 1; /* likely, but better fail with unknown values */
    switch(channel_config) {
        case 0x00: channel_count = 1; break;
        case 0x04: channel_count = 2; break;
        case 0x0c: channel_count = 4; break;
        case 0x14: channel_count = 6; break;
        case 0x1c: channel_count = 8; break;
        default:
            VGM_LOG("EA SNS/SPS: unknown channel config 0x%02x\n", channel_config);
            goto fail;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;
    vgmstream->meta_type = meta_type;

    /* EA decoder list and known internal FourCCs */
    switch(codec) {

        case 0x02:      /* "P6B0": PCM16BE [NBA Jam (Wii)] */
            vgmstream->coding_type = coding_PCM16_int;
            vgmstream->codec_endian = 1;
            vgmstream->layout_type = layout_blocked_ea_sns;
            break;

#ifdef VGM_USE_FFMPEG
        case 0x03: {    /* "EXm0": EA-XMA [Dante's Inferno (X360)] */
            uint8_t buf[0x100];
            int bytes, block_size, block_count;
            size_t stream_size, virtual_size;
            ffmpeg_custom_config cfg = {0};

            stream_size = get_streamfile_size(streamData) - start_offset;
            virtual_size = ffmpeg_get_eaxma_virtual_size(vgmstream->channels, start_offset,stream_size, streamData);
            block_size = 0x10000; /* todo unused and not correctly done by the parser */
            block_count = stream_size / block_size + (stream_size % block_size ? 1 : 0);

            bytes = ffmpeg_make_riff_xma2(buf, 0x100, vgmstream->num_samples, virtual_size, vgmstream->channels, vgmstream->sample_rate, block_count, block_size);
            if (bytes <= 0) goto fail;

            cfg.type = FFMPEG_EA_XMA;
            cfg.virtual_size = virtual_size;
            cfg.channels = vgmstream->channels;

            vgmstream->codec_data = init_ffmpeg_config(streamData, buf,bytes, start_offset,stream_size, &cfg);
            if (!vgmstream->codec_data) goto fail;

            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif

        case 0x04:      /* "Xas1": EA-XAS [Dead Space (PC/PS3)] */
            vgmstream->coding_type = coding_EA_XAS;
            vgmstream->layout_type = layout_blocked_ea_sns;
            break;

#ifdef VGM_USE_MPEG
        case 0x05:      /* "EL31": EALayer3 v1 [Need for Speed: Hot Pursuit (PS3)] */
        case 0x06:      /* "L32P": EALayer3 v2 "PCM" [Battlefield 1943 (PS3)] */
        case 0x07: {    /* "L32S": EALayer3 v2 "Spike" [Dante's Inferno (PS3)] */
            mpeg_custom_config cfg = {0};
            mpeg_custom_t type = (codec == 0x05 ? MPEG_EAL31b : (codec == 0x06) ? MPEG_EAL32P : MPEG_EAL32S);

            /* remove blocks on reads for some edge cases in L32P and to properly apply discard modes
             * (otherwise, and removing discards, it'd work with layout_blocked_ea_sns) */
            temp_streamFile = setup_eaac_streamfile(streamData, version, codec, start_offset, 0);
            if (!temp_streamFile) goto fail;

            start_offset = 0x00; /* must point to the custom streamfile's beginning */

            /* layout is still blocks, but should work fine with the custom mpeg decoder */
            vgmstream->codec_data = init_mpeg_custom(temp_streamFile, start_offset, &vgmstream->coding_type, vgmstream->channels, type, &cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->layout_type = layout_none;

            break;
        }
#endif

        case 0x08:      /* "Gca0"?: DSP [Need for Speed: Nitro sfx (Wii)] */
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_blocked_ea_sns;
            /* DSP coefs are read in the blocks */
            break;

#ifdef VGM_USE_ATRAC9
        case 0x0a: {    /* EATrax */
            atrac9_config cfg = {0};
            size_t total_size;

            cfg.channels = vgmstream->channels;
            cfg.config_data = read_32bitBE(header_offset + 0x08,streamHead);
            /* 0x10: frame size? (same as config data?) */
            total_size = read_32bitLE(header_offset + 0x0c,streamHead); /* actual data size without blocks, LE b/c why make sense */

            vgmstream->codec_data = init_atrac9(&cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_ATRAC9;
            vgmstream->layout_type = layout_none;

            /* EATrax is "buffered" ATRAC9, uses custom IO since it's kind of complex to add to the decoder */
            temp_streamFile = setup_eaac_streamfile(streamData, version, codec, start_offset, total_size);
            if (!temp_streamFile) goto fail;

            start_offset = 0x00; /* must point to the custom streamfile's beginning */
            break;
        }
#endif

        case 0x00: /* "NONE" (internal 'codec not set' flag) */
        case 0x01: /* not used/reserved? /MP30/P6L0/P2B0/P2L0/P8S0/P8U0/PFN0? */
        case 0x09: /* EASpeex (libspeex variant, base versions vary: 1.0.5, 1.2beta3) */
        case 0x0b: /* ? */
        case 0x0c: /* EAOpus (inside each SNS/SPS block is 16b frame size + standard? Opus packet) */
        case 0x0d: /* ? */
        case 0x0e: /* ? */
        case 0x0f: /* ? */
        default:
            VGM_LOG("EA SNS/SPS: unknown codec 0x%02x\n", codec);
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream,temp_streamFile ? temp_streamFile : streamData,start_offset))
        goto fail;

    if (vgmstream->layout_type == layout_blocked_ea_sns)
        block_update_ea_sns(start_offset, vgmstream);

    close_streamfile(temp_streamFile);
    return vgmstream;

fail:
    close_streamfile(temp_streamFile);
    close_vgmstream(vgmstream);
    return NULL;
}
