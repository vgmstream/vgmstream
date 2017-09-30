#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"


/* .SNU - from EA Redwood Shores/Visceral games (Dead Space, Dante's Inferno, The Godfather 2) */
VGMSTREAM * init_vgmstream_ea_snu(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    int channel_count, loop_flag = 0, channel_config, codec, sample_rate, flags;
    uint32_t num_samples, loop_start = 0, loop_end = 0;
    off_t start_offset;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;


    /* check extension, case insensitive */
    if (!check_extensions(streamFile,"snu"))
        goto fail;

    /* check header (the first 0x10 are BE/LE depending on platform) */
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

    start_offset = read_32bit(0x08,streamFile);

    codec = read_8bit(0x10,streamFile);
    channel_config = read_8bit(0x11,streamFile);
    sample_rate = (uint16_t)read_16bitBE(0x12,streamFile);
    flags = (uint8_t)read_8bit(0x14,streamFile); /* upper nibble only? */
    num_samples = (uint32_t)read_32bitBE(0x14,streamFile) & 0x00FFFFFF;
    /* 0x18: null?, 0x1c: null? */

    if (flags != 0x60 && flags != 0x40) {
        VGM_LOG("EA SNS: unknown flag\n");
        goto fail;
    }

#if 0
    //todo not working ok with blocks in XAS
    //todo check if EA-XMA loops (Dante's Inferno doesn't)
    if (flags & 0x60) { /* full loop, seen in ambient tracks */
        loop_flag = 1;
        loop_start = 0;
        loop_end = num_samples;
    }
#endif

    //channel_count = (channel_config >> 2) + 1; //todo test
    /* 01/02/03 = 1 ch?, 05/06/07 = 2/3 ch?, 0d/0e/0f = 4/5 ch?, 15/16/17 = 6/7 ch?, 1d/1e/1f = 8 ch? */
    switch(channel_config) {
        case 0x00: channel_count = 1; break;
        case 0x04: channel_count = 2; break;
        case 0x0c: channel_count = 4; break;
        case 0x14: channel_count = 6; break;
        case 0x1c: channel_count = 8; break;
        default:
            VGM_LOG("EA SNU: unknown channel config 0x%02x\n", channel_config);
            goto fail;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

    vgmstream->meta_type = meta_EA_SNU;

    switch(codec) {
        case 0x04:      /* "Xas1": EA-XAS (Dead Space PC/PS3) */
            vgmstream->coding_type = coding_EA_XAS;
            vgmstream->layout_type = layout_ea_sns_blocked;
            break;

#if 0
#ifdef VGM_USE_MPEG
        case 0x07: {    /* "EL32S": EALayer3 v2 "S" (Dante's Inferno PS3) */
            mpeg_custom_config cfg;
            off_t mpeg_start_offset = start_offset + 0x08;

            memset(&cfg, 0, sizeof(mpeg_custom_config));

            /* layout is still blocks, but should work fine with the custom mpeg decoder */
            vgmstream->codec_data = init_mpeg_custom_codec_data(streamFile, mpeg_start_offset, &vgmstream->coding_type, vgmstream->channels, MPEG_EAL32S, &cfg);
            if (!vgmstream->codec_data) goto fail;

            vgmstream->layout_type = layout_ea_sns_blocked;
            break;
        }
#endif
#endif

#ifdef VGM_USE_FFMPEG
        case 0x03: {    /* "EXm0": EA-XMA (Dante's Inferno X360) */
            uint8_t buf[0x100];
            int bytes, block_size, block_count;
            size_t stream_size, virtual_size;
            ffmpeg_custom_config cfg;

            stream_size = get_streamfile_size(streamFile) - start_offset;
            virtual_size = ffmpeg_get_eaxma_virtual_size(vgmstream->channels, start_offset,stream_size, streamFile);
            block_size = 0x10000; /* todo unused and not correctly done by the parser */
            block_count = stream_size / block_size + (stream_size % block_size ? 1 : 0);

            bytes = ffmpeg_make_riff_xma2(buf, 0x100, vgmstream->num_samples, virtual_size, vgmstream->channels, vgmstream->sample_rate, block_count, block_size);
            if (bytes <= 0) goto fail;

            memset(&cfg, 0, sizeof(ffmpeg_custom_config));
            cfg.type = FFMPEG_EA_XMA;
            cfg.virtual_size = virtual_size;
            cfg.channels = vgmstream->channels;

            vgmstream->codec_data = init_ffmpeg_config(streamFile, buf,bytes, start_offset,stream_size, &cfg);
            if (!vgmstream->codec_data) goto fail;

            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif
        case 0x00: /* "NONE" */
        case 0x01: /* not used? */
        case 0x02: /* "P6B0": PCM16BE */

        case 0x05: /* "EL31": EALayer3 v1 b (with PCM blocks in normal EA-frames?) */
        case 0x06: /* "EL32P": EALayer3 v2 "P" */
        case 0x09: /* EASpeex? */
        case 0x0c: /* EAOpus? */
        case 0x0e: /* XAS variant? */
        case 0x0f: /* EALayer3 variant? */
        /* also 0x1n variations, used in other headers */
        default:
            VGM_LOG("EA SNU: unknown codec 0x%02x\n", codec);
            goto fail;
    }


    /* open the file for reading by each channel */
    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    if (vgmstream->layout_type == layout_ea_sns_blocked)
        ea_sns_block_update(start_offset, vgmstream);

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
