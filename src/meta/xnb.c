#include "meta.h"
#include "../coding/coding.h"
#include "xnb_streamfile.h"


/* XNB - Microsoft XNA Game Studio 4.0 format */
VGMSTREAM* init_vgmstream_xnb(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_h = NULL;
    off_t start_offset, offset, xma_chunk_offset = 0;
    int loop_flag = 0, channel_count, num_samples = 0, loop_start = 0, loop_end = 0;
    int big_endian, flags, codec, sample_rate, block_align, bps;
    size_t data_size;
    char platform;
    int is_sound = 0, is_ogg = 0, is_at9 = 0, is_song = 0;
    char song_name[255+1];


    /* checks */
    if ((read_u32be(0x00, sf) & 0xFFFFFF00) != get_id32be("XNB\0"))
        goto fail;
    if (!check_extensions(sf,"xnb"))
        goto fail;

    /* XNA Studio platforms: 'w' = Windows, 'm' = Windows Phone 7, 'x' = X360
     * MonoGame extensions: 'i' = iOS, 'a' = Android, 'X' = MacOSX, 'P' = PS4, 'S' = Switch, etc */
    platform = read_u8(0x03, sf);
    big_endian = (platform == 'x');

    if (read_u8(0x04,sf) != 0x04 &&   /* XNA 3.0? found on Scare Me (XBLIG), no notable diffs */
        read_u8(0x04,sf) != 0x05)     /* XNA 4.0 version */
        goto fail;

    flags = read_u8(0x05, sf);
  //if (flags & 0x01) goto fail; /* "HiDef profile" content (no actual difference) */

    /* full size */
    if (read_u32le(0x06, sf) != get_streamfile_size(sf)) {
        goto fail;
    }

    /* handle XNB compression as a normal non-compressed stream, normally for graphics
     * and other formats, but confused devs use it with already-compressed audio like Ogg/ATRAC9 */
    if ((flags & 0x80) ||    /* LZX/XMemCompress (not used with audio?) */
        (flags & 0x40)) {    /* LZ4 (MonoGame extension) [Square Heroes (PS4), Little Savior (PC)] */
        size_t compression_start = 0x0e;
        size_t compression_size = read_u32le(0x0a, sf);
        sf_h = setup_xnb_streamfile(sf, flags, compression_start, compression_size);
        if (!sf_h) goto fail;

        //dump_streamfile(sf_h, 0);
        offset = 0x0e; /* refers to decompressed stream */
    }
    else {
        sf_h = sf;
        offset = 0x0a;
    }


    /* XNB contains "type reader" class references to parse "shared resource" data (can be any implemented filetype) */
    {
        char reader_name[255+1];
        size_t string_len;
        uint8_t type_count;
        static const char* type_sound =  "Microsoft.Xna.Framework.Content.SoundEffectReader"; /* partial "fmt" chunk or XMA */
        static const char* type_ogg = "SoundEffectFromOggReader"; /* has extra text info after base part */
        static const char* type_song = "Microsoft.Xna.Framework.Content.SongReader"; /* references a companion .wma */
        static const char* type_int32 = "Microsoft.Xna.Framework.Content.Int32Reader"; /* extra crap */

        type_count = read_u8(offset++, sf_h);

        /* check type reader string */
        string_len = read_u8(offset++, sf_h); /* doesn't count null */
        if (read_string(reader_name, string_len+1, offset, sf_h) != string_len)
            goto fail;

        if (strcmp(reader_name, type_sound) == 0) {
            if (type_count != 1) goto fail;
            is_sound = 1;
        }
        else if (strncmp(reader_name, type_ogg, strlen(type_ogg)) == 0) { /* has extra info after base string */
            if (type_count != 1) goto fail;
            is_ogg = 1;
        }
        else if (strcmp(reader_name, type_song) == 0) {
            if (type_count != 2) goto fail;
            is_song = 1;
        }
        else {
            goto fail;
        }

        offset += string_len + 1;

        if (is_song) {
            offset += 3;

            string_len = read_u8(offset++, sf_h);
            if (read_string(reader_name, string_len+1, offset, sf_h) != string_len)
                goto fail;

            if (strcmp(reader_name, type_int32) != 0)
                goto fail;

            offset += string_len + 1;
        }

        offset += 0x04; /* reader version, 0 */

        /* shared resource number 1 */
        if (read_u8(offset++, sf_h) != 1)
            goto fail;

        /* read shared resource */
        if (is_sound || is_ogg) {
            /* partial "fmt" chunk */
            uint32_t (*read_u32)(off_t,STREAMFILE*) = big_endian ? read_u32be : read_u32le;
            uint16_t (*read_u16)(off_t,STREAMFILE*) = big_endian ? read_u16be : read_u16le;
            uint32_t fmt_chunk_size;

            fmt_chunk_size = read_u32le(offset, sf_h);
            offset += 0x04;

            codec         = read_u16(offset+0x00, sf_h);
            channel_count = read_u16(offset+0x02, sf_h);
            sample_rate   = read_u32(offset+0x04, sf_h);
            /* 0x08: byte rate */
            block_align   = read_u16(offset+0x0c, sf_h);
            bps           = read_u16(offset+0x0e, sf_h);

            if (codec == 0x0002) {
                if (!msadpcm_check_coefs(sf_h, offset + 0x14))
                    goto fail;
            }

            if (codec == 0x0166) {
                xma2_parse_fmt_chunk_extra(sf_h, offset, &loop_flag, &num_samples, &loop_start, &loop_end, big_endian);
                xma_chunk_offset = offset;
            }

            if (codec == 0xFFFF) {
                if (platform != 'S') goto fail;
                sample_rate = read_u32(offset+fmt_chunk_size+0x04+0x08, sf_h);
            }

            /* mini-fmt has AT9 stuff then a regular RIFF [Square Heroes (PS4)] */
            if (codec == 0xFFFE) {
                is_at9 = 1;
            }

            /* Ogg (with loop tags) poses as PCM [Little Savior (PC)] */

            offset += fmt_chunk_size;

            data_size = read_u32le(offset, sf_h);
            offset += 0x04;

            start_offset = offset;
        }
        else if (is_song) {
            /* filename (typically same as .xnb but .wma) */
            string_len = read_u8(offset++, sf_h);

            if (read_string(song_name, string_len+1, offset, sf_h) != string_len + 1)
                goto fail;

            start_offset = 0;
            data_size = 0;
            /* after name is shared resource number 1 + 32b int (durationMs?) */
        }
        else {
            goto fail;
        }
    }

    /* container handling */
    if (is_ogg || is_at9) {
        STREAMFILE* temp_sf = NULL;
        const char* fake_ext = is_ogg ? "ogg" : "at9";

        /* after data_size is loop start + loop length and offset? (same as loop tags), 0 if not enabled */

        temp_sf = setup_subfile_streamfile(sf_h, start_offset, data_size, fake_ext);
        if (!temp_sf) goto fail;

        if (is_ogg) {
            vgmstream = init_vgmstream_ogg_vorbis(temp_sf);
        }
        else {
            vgmstream = init_vgmstream_riff(temp_sf);
        }
        close_streamfile(temp_sf);
        if (!vgmstream) goto fail;

        vgmstream->meta_type = meta_XNB;
        if (sf_h != sf) close_streamfile(sf_h);
        return vgmstream;
    }
    else if (is_song) {
        STREAMFILE* sf_body = open_streamfile_by_filename(sf, song_name);
        if (!sf_body) goto fail;

        if (read_u32be(0x00, sf_body) == 0x01000080) {
            STREAMFILE* temp_sf = setup_subfile_streamfile(sf_body, 0x00, get_streamfile_size(sf_body), "opus");
            if (!temp_sf) goto fail;

            /* MonoGame with NXOpus [Clan N (Switch)] */
            vgmstream = init_vgmstream_opus_std(temp_sf);
            close_streamfile(temp_sf);
        }
        else {
#ifdef VGM_USE_FFMPEG
            /* XNA with WMA [Guncraft: Blocked and Loaded (X360)] */
            vgmstream = init_vgmstream_ffmpeg(sf_body);
#endif
        }
        close_streamfile(sf_body);
        if (!vgmstream) goto fail;

        vgmstream->meta_type = meta_XNB;
        if (sf_h != sf) close_streamfile(sf_h);
        return vgmstream;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->meta_type = meta_XNB;

    switch (codec) {
        case 0x01: /* Dragon's Blade (Android) */
            if (!block_align) /* null in Metagalactic Blitz (PC) */
                block_align = (bps == 8 ? 0x01 : 0x02) * channel_count;

            vgmstream->coding_type = bps == 8 ? coding_PCM8_U : coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = block_align / channel_count;
            vgmstream->num_samples = pcm_bytes_to_samples(data_size, channel_count, bps);
            break;

        case 0x02: /* White Noise Online (PC) */
            if (!block_align) goto fail;
            vgmstream->coding_type = coding_MSADPCM;
            vgmstream->layout_type = layout_none;
            vgmstream->frame_size = block_align;
            vgmstream->num_samples = msadpcm_bytes_to_samples(data_size, block_align, channel_count);
            break;

        case 0x11:
            if (!block_align) goto fail;
            vgmstream->coding_type = coding_MS_IMA;
            vgmstream->layout_type = layout_none;
            vgmstream->interleave_block_size = block_align;
            vgmstream->num_samples = ms_ima_bytes_to_samples(data_size, block_align, channel_count);
            break;

#ifdef VGM_USE_FFMPEG
        case 0x166: { /* Terraria (X360) */
            int block_size = 0x10000; /* guessed */

            vgmstream->codec_data = init_ffmpeg_xma2_raw(sf_h, start_offset, data_size, num_samples, vgmstream->channels, vgmstream->sample_rate, block_size, 0);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = num_samples;
            vgmstream->loop_start_sample = loop_start;
            vgmstream->loop_end_sample = loop_end;

            xma_fix_raw_samples(vgmstream, sf_h, start_offset,data_size, xma_chunk_offset, 1,1);
            break;
        }
#endif

        case 0xFFFF: /* Eagle Island (Switch) */
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = data_size / channel_count;
            vgmstream->num_samples = read_s32le(start_offset + 0x00, sf_h);
            //vgmstream->num_samples = dsp_bytes_to_samples(data_size - 0x60*channel_count, channel_count);

            dsp_read_coefs(vgmstream, sf_h, start_offset + 0x1c, vgmstream->interleave_block_size, big_endian);
            dsp_read_hist (vgmstream, sf_h, start_offset + 0x3c, vgmstream->interleave_block_size, big_endian);
            break;

        default:
            VGM_LOG("XNB: unknown codec 0x%x\n", codec);
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sf_h, start_offset))
        goto fail;

    if (sf_h != sf) close_streamfile(sf_h);
    return vgmstream;

fail:
    if (sf_h != sf) close_streamfile(sf_h);
    close_vgmstream(vgmstream);
    return NULL;
}
