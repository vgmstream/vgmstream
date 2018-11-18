#include "meta.h"
#include "../coding/coding.h"

/* P3D - from Radical's Prototype 1/2 (PC/PS3/X360) */
VGMSTREAM * init_vgmstream_p3d(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, parse_offset, name_offset = 0;
    size_t header_size, file_size, data_size;
    int loop_flag = 0, channel_count, sample_rate, codec;
    int i, name_count, text_len, block_size = 0, block_count = 0, num_samples;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;


    /* checks */
    if (!check_extensions(streamFile,"p3d"))
        goto fail;
    if (read_32bitBE(0x0,streamFile) != 0x503344FF &&  /* "P3D"\FF (LE: PC) */
        read_32bitBE(0x0,streamFile) != 0xFF443350)    /* \FF"D3P" (BE: PS3, X360) */
        goto fail;

    read_32bit = read_32bitBE(0x0,streamFile) == 0xFF443350 ? read_32bitBE : read_32bitLE;
    file_size = get_streamfile_size(streamFile);

    /* base header */
    header_size = read_32bit(0x4,streamFile);
    if (0x0C != header_size) goto fail;
    if (read_32bit(0x08,streamFile) != file_size) goto fail;
    if (read_32bit(0x0C,streamFile) != 0xFE000000) goto fail; /* fixed */
    if (read_32bit(0x10,streamFile) + header_size != file_size) goto fail;
    if (read_32bit(0x14,streamFile) + header_size != file_size) goto fail; /* body size again */
    if (read_32bit(0x18,streamFile) != 0x0000000A) goto fail; /* fixed */

    /* header text */
    parse_offset = 0x1C;
    text_len = read_32bit(parse_offset,streamFile);
    if (9 != text_len) goto fail;
    parse_offset += 4;

    /* check the type as P3D is just a generic container used in Radical's games */
    if (read_32bitBE(parse_offset+0x00,streamFile) != 0x41756469 ||
        read_32bitBE(parse_offset+0x04,streamFile) != 0x6F46696C ||
        read_16bitBE(parse_offset+0x08,streamFile) != 0x6500) goto fail; /* "AudioFile\0" */
    parse_offset += text_len + 1;

    /* file names: always 2 (repeated); but if it's 3 there is an extra string later */
    name_count = read_32bit(parse_offset,streamFile);
    if (name_count != 2 && name_count != 3) goto fail; /* 2: Prototype1, 3: Prototype2 */
    parse_offset += 4;

    /* skip names */
    for (i = 0; i < 2; i++) {
        if (!name_offset)
            name_offset = parse_offset + 4;
        text_len = read_32bit(parse_offset,streamFile) + 1; /* null-terminated */
        parse_offset += 4 + text_len;
    }

    /* info count? */
    if (0x01 != read_32bit(parse_offset,streamFile)) goto fail;
    parse_offset += 4;

    /* next string can be used as a codec id */
    text_len = read_32bit(parse_offset,streamFile);
    codec = read_32bitBE(parse_offset+4,streamFile);
    parse_offset += 4 + text_len + 1;

    /* extra "Music" string in Prototype 2 */
    if (name_count == 3) {
        text_len = read_32bit(parse_offset,streamFile) + 1; /* null-terminated */
        parse_offset += 4 + text_len;
    }


    /* sub-header per codec */
    switch(codec) {
        case 0x72616470:    /* "radp" (PC) */
            if (read_32bitBE(parse_offset,streamFile) != 0x52414450) goto fail; /* "RADP" */
            parse_offset += 0x04;
            channel_count = read_32bit(parse_offset+0x00,streamFile);
            sample_rate   = read_32bit(parse_offset+0x04,streamFile);
            /* 0x08: ? (0x0F) */
            data_size     = read_32bit(parse_offset+0x0c,streamFile);
            block_size    = 0x14;
            num_samples   = data_size / block_size / channel_count * 32;
            start_offset  = parse_offset+0x10;
            break;

        case 0x6D703300:    /* "mp3\0" (PS3) */
            if ((read_32bitBE(parse_offset,streamFile) & 0xFFFFFF00) != 0x6D703300) goto fail; /* "mp3" */
            parse_offset += 0x03;
            /* all fields LE even though the prev parts were BE */
            sample_rate   = read_32bitLE(parse_offset+0x00,streamFile);
            /* 0x04: mp3 sample rate (ex. @0x00 is 47999 and @0x04 is 48000) */
            num_samples   = read_32bitLE(parse_offset+0x08,streamFile);
            data_size     = read_32bitLE(parse_offset+0x0c,streamFile);
            channel_count = read_32bitLE(parse_offset+0x10,streamFile);
            block_size    = read_32bitLE(parse_offset+0x14,streamFile);
            num_samples   = num_samples / channel_count; /* total samples */
            start_offset  = parse_offset+0x18;
            break;

        case 0x786D6100:    /* "xma\0" (X360) */
            if (read_32bitBE(parse_offset,streamFile) != 0x584D4132) goto fail; /* "XMA2" */
            parse_offset += 0x04;
            /* 0x00: subheader size? (0x2c),  0x04: seek table size */
            data_size     = read_32bitBE(parse_offset+0x08,streamFile);
            /* 0x0c: ?,  0x10: ?, 0x14/18: 0x0 */
            sample_rate   = read_32bitBE(parse_offset+0x1c,streamFile);
            /* 0x20: XMA decoder params,  0x24: abr */
            block_size    = read_32bitBE(parse_offset+0x28,streamFile);
            num_samples   = read_32bitBE(parse_offset+0x2c,streamFile);
            /* 0x30: original file's samples */
            block_count   = read_32bitBE(parse_offset+0x34,streamFile);
            channel_count =    read_8bit(parse_offset+0x38,streamFile);
            /* 0x39: channel related? (stream config? channel layout?) */
            start_offset  = parse_offset + 0x3c + read_32bitBE(parse_offset+0x04,streamFile);
            break;

        default:
            VGM_LOG("P3D: unknown codec 0x%04x\n", codec);
            goto fail;
    }

    if (start_offset + data_size != file_size) goto fail;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->meta_type = meta_P3D;
    if (name_offset)
        read_string(vgmstream->stream_name,STREAM_NAME_SIZE, name_offset,streamFile);

    /* codec init */
    switch(codec) {
        case 0x72616470:    /* "radp" (PC) */
            vgmstream->coding_type = coding_RAD_IMA_mono;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = block_size;
            break;

#ifdef VGM_USE_MPEG
        case 0x6D703300: {  /* "mp3\0" (PS3) */
            mpeg_custom_config cfg = {0};

            cfg.interleave = 0x400;
            cfg.data_size = data_size;
            /* block_size * 3 = frame size (0x60*3=0x120 or 0x40*3=0xC0) but doesn't seem to have any significance) */

            vgmstream->codec_data = init_mpeg_custom(streamFile, start_offset, &vgmstream->coding_type, vgmstream->channels, MPEG_P3D, &cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif

#ifdef VGM_USE_FFMPEG
        case 0x786D6100: {  /* "xma\0" (X360) */
            uint8_t buf[0x100];
            size_t bytes;

            bytes = ffmpeg_make_riff_xma2(buf,0x100, vgmstream->num_samples, data_size, vgmstream->channels, vgmstream->sample_rate, block_count, block_size);
            vgmstream->codec_data = init_ffmpeg_header_offset(streamFile, buf,bytes, start_offset,data_size);
            if ( !vgmstream->codec_data ) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            xma_fix_raw_samples(vgmstream, streamFile, start_offset, data_size, 0, 1,1); /* samples needs adjustment */
            break;
        }
#endif

        default:
            VGM_LOG("P3D: unknown codec 0x%04x\n", codec);
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
