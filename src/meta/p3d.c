#include "meta.h"
#include "../coding/coding.h"
#include "../util/endianness.h"


/* P3D - from Radical's Prototype 1/2 (PC/PS3/X360), Spider-Man 4 Beta (X360) */
VGMSTREAM* init_vgmstream_p3d(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset, offset, name_offset = 0;
    size_t header_size, file_size, data_size;
    uint32_t xma2_offset = 0, xma2_size = 0;
    int loop_flag, channels, sample_rate, codec;
    int i, name_count, unk_count, text_len, block_size = 0, num_samples;
    read_u32_t read_u32;


    /* checks */
    if (!is_id32be(0x00,sf, "P3D\xFF") &&   /* LE: PC */
        !is_id32le(0x00,sf, "P3D\xFF"))     /* BE: PS3, X360 */
        goto fail;
    if (!check_extensions(sf,"p3d"))
        goto fail;

    read_u32 = guess_endian32(0x04,sf) ? read_u32be : read_u32le;
    file_size = get_streamfile_size(sf);

    /* base header */
    header_size = read_u32(0x04,sf);
    if (header_size != 0x0c) goto fail;
    if (read_u32(0x08,sf) != file_size) goto fail;
    offset = 0x0c; 

    /* P3D is just a generic container used in Radical's games, so we only want "AudioFile".
     * Rarely some voice files start with a "AudioDialogueSubtitle" section, so skip that first */
    if (is_id64be(offset+0x14,sf, "AudioDia")) {
        offset += read_u32(offset + 0x04,sf); /* section size */
    }

    /* AudioFile section */
    if (read_u32(offset + 0x00,sf) != 0xFE000000) goto fail; /* section marker */
    if (read_u32(offset + 0x04,sf) + offset != file_size) goto fail; /* AudioFile size */
    if (read_u32(offset + 0x08,sf) + offset != file_size) goto fail; /* again */
    if (read_u32(offset + 0x0c,sf) != 0x0000000A) goto fail; /* fixed */

    text_len = read_u32(offset + 0x10,sf);
    if (text_len != 9) goto fail;
    offset += 0x14;

    if (!is_id64be(offset+0x00,sf, "AudioFil") || read_u16be(offset+0x08,sf) != 0x6500) /* "AudioFile\0" */
        goto fail;
    offset += text_len + 0x01;

    /* file names: 1 internal stream name + 1 external filename (usually repeated) + 1 an extra string later (P2) */
    name_count = read_u32(offset,sf);
    if (name_count != 2 && name_count != 3) goto fail; /* 2: Prototype1, 3: Prototype2 */
    offset += 0x04;

    /* skip names */
    for (i = 0; i < 2; i++) {
        if (!name_offset)
            name_offset = offset + 0x04;
        text_len = read_u32(offset,sf) + 1; /* null-terminated */
        offset += 0x04 + text_len;
    }

    /* 1=music, 0=dialogues */
    unk_count = read_u32(offset,sf);
    if (unk_count != 0x00 && unk_count != 0x01) goto fail;
    offset += 0x04;

    /* next string can be used as a codec id */
    text_len = read_u32(offset,sf);
    codec = read_u32be(offset+0x04,sf);
    offset += 0x04 + text_len + 0x01;

    /* extra "Music" or "Dialogue" string in Prototype 2 */
    if (name_count >= 3) {
        text_len = read_u32(offset,sf) + 1; /* null-terminated */
        offset += 0x04 + text_len;
    }

    loop_flag = 0;

    /* sub-header per codec */
    switch(codec) {
        case 0x72616470:    /* "radp" (PC) */
            if (!is_id32be(offset,sf, "RADP"))
                goto fail;
            offset += 0x04;

            channels        = read_u32(offset+0x00,sf);
            sample_rate     = read_u32(offset+0x04,sf);
            /* 0x08: ? (0x0F) */
            data_size       = read_u32(offset+0x0c,sf);
            block_size      = 0x14;

            num_samples     = data_size / block_size / channels * 32;
            start_offset    = offset + 0x10;
            break;

        case 0x6D703300:    /* "mp3\0" (PS3) */
            if ((read_u32be(offset,sf) & 0xFFFFFF00) != get_id32be("mp3\0"))
                goto fail;
            offset += 0x03;

            /* all fields LE even though the prev parts were BE */
            sample_rate     = read_s32le(offset+0x00,sf);
            /* 0x04: mp3 sample rate (ex. @0x00 is 47999 and @0x04 is 48000) */
            num_samples     = read_s32le(offset+0x08,sf);
            data_size       = read_u32le(offset+0x0c,sf);
            channels        = read_s32le(offset+0x10,sf);
            block_size      = read_u32le(offset+0x14,sf);

            num_samples     = num_samples / channels; /* total samples */
            start_offset    = offset + 0x18;
            break;

        case 0x786D6100: {  /* "xma\0" (X360) */
            uint32_t seek_size;

            if (!is_id32be(offset,sf, "XMA2"))
                goto fail;
            offset += 0x04;

            xma2_size       = read_u32be(offset+0x00,sf);
            seek_size       = read_u32be(offset+0x04,sf);
            data_size       = read_u32be(offset+0x08,sf);
            /* 0x0c: ? */
            xma2_offset = offset+0x10;
            if (!read_u8(xma2_offset+0x00, sf))  /* needs "xma2" chunk (Spider-Man 4 beta has multi-streams) */
                goto fail;

            /* loops never set */
            xma2_parse_xma2_chunk(sf, xma2_offset, &channels, &sample_rate, &loop_flag, &num_samples, NULL, NULL);

            start_offset  = offset + 0x10 + xma2_size + seek_size;
            break;
        }

        default:
            vgm_logi("P3D: unknown codec 0x%04x\n", codec);
            goto fail;
    }

    if (start_offset + data_size != file_size)
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_P3D;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    if (name_offset)
        read_string(vgmstream->stream_name,STREAM_NAME_SIZE, name_offset,sf);

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

            vgmstream->codec_data = init_mpeg_custom(sf, start_offset, &vgmstream->coding_type, vgmstream->channels, MPEG_P3D, &cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif

#ifdef VGM_USE_FFMPEG
        case 0x786D6100: {  /* "xma\0" (X360) */
            //TODO: some in Spider-Man 4 beta use 18ch but ffmpeg supports max 16ch XMA2
            vgmstream->codec_data = init_ffmpeg_xma_chunk(sf, start_offset, data_size, xma2_offset, xma2_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            xma_fix_raw_samples(vgmstream, sf, start_offset, data_size, 0, 1,1); /* samples needs adjustment */
            break;
        }
#endif

        default:
            vgm_logi("P3D: unknown codec 0x%04x\n", codec);
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
