#include "meta.h"
#include "../coding/coding.h"
#include "idtech_streamfile.h"
#include <string.h>


/* mzrt - id Tech 4.5 audio found in .resource bigfiles (w/ internal filenames) [Doom 3 BFG edition (PC/PS3/X360)] */
VGMSTREAM* init_vgmstream_mzrt_v0(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag = 0, channels, codec, sample_rate, block_size = 0, bps = 0, num_samples;
    STREAMFILE* temp_sf = NULL;


    /* checks */
    if (!is_id32be(0x00,sf, "mzrt"))
        goto fail;
    if (read_u32be(0x04, sf) != 0) /* version */
        goto fail;

    if (!check_extensions(sf, "idwav,idmsf,idxma"))
        goto fail;


    /* this format is bizarrely mis-aligned (and mis-designed too) */

    num_samples = read_s32be(0x11,sf);
    codec = read_u16le(0x15,sf);
    switch(codec) {
        case 0x0001:
        case 0x0002:
        case 0x0166:
            channels = read_u16le(0x17,sf);
            sample_rate = read_u32le(0x19, sf);
            block_size = read_u16le(0x21, sf);
            bps = read_u16le(0x23,sf);

            start_offset = 0x25;
            break;

        case 0x0000:
            sample_rate = read_u32be(0x1D, sf);
            channels = read_u32be(0x21, sf);

            start_offset = 0x29;
            break;

        default:
            goto fail;
    }

    /* skip MSADPCM data */
    if (codec == 0x0002) {
        if (!msadpcm_check_coefs(sf, start_offset + 0x02 + 0x02))
            goto fail;

        start_offset += 0x02 + read_u16le(start_offset, sf);
    }

    /* skip extra data */
    if (codec == 0x0166) {
        start_offset += 0x02 + read_u16le(start_offset, sf);
    }

    /* skip unknown table */
    if (codec == 0x0000) {
        start_offset += 0x04 + read_u32be(start_offset, sf) * 0x04;
    }

    /* skip unknown table */
    start_offset += 0x04 + read_u32be(start_offset, sf);

    /* skip block info */
    if (codec != 0x0000) {
        /* 0x00: de-blocked size
         * 0x04: block count*/
        start_offset += 0x08;

        /* idwav only uses 1 super-block though */
        temp_sf = setup_mzrt_streamfile(sf, start_offset);
        if (!temp_sf) goto fail;
    }
    else {
        /* 0x00: de-blocked size */
        start_offset += 0x04;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_MZRT;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;

    switch(codec) {
        case 0x0001:
            if (bps != 16) goto fail;
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = block_size / channels;
            break;

        case 0x0002:
            if (bps != 4) goto fail;
            vgmstream->coding_type = coding_MSADPCM;
            vgmstream->layout_type = layout_none;
            vgmstream->frame_size = block_size;
            break;

#ifdef VGM_USE_FFMPEG
        case 0x0166: {
            size_t stream_size = get_streamfile_size(temp_sf);

            vgmstream->codec_data = init_ffmpeg_xma_chunk_split(sf, temp_sf, 0x00, stream_size, 0x15, 0x34);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            xma_fix_raw_samples_hb(vgmstream, sf, temp_sf, 0x00, stream_size, 0x15, 0,0);
            break;
        }
#endif

#ifdef VGM_USE_MPEG
        case 0x0000: {
            mpeg_custom_config cfg = {0};

            cfg.skip_samples = 576; /* assumed */

            vgmstream->codec_data = init_mpeg_custom(sf, start_offset, &vgmstream->coding_type, vgmstream->channels, MPEG_STANDARD, &cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif

        default:
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream, temp_sf == NULL ? sf : temp_sf, temp_sf == NULL ? start_offset : 0x00))
        goto fail;
    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}

/* mzrt - id Tech 5 audio [Rage (PS3), The Evil Within (PS3)] */
VGMSTREAM* init_vgmstream_mzrt_v1(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, offset;
    size_t stream_size;
    int loop_flag, channels, codec, type, sample_rate, block_size = 0, bps = 0;
    int32_t num_samples, loop_start = 0;
    STREAMFILE* sb = NULL;
    const char* extension = NULL;


    /* checks */
    if (!is_id32be(0x00,sf, "mzrt"))
        goto fail;
    if (read_u32be(0x04, sf) != 1) /* version */
        goto fail;

    if (!check_extensions(sf, "idmsf")) //idmsa: untested
        goto fail;

    type = read_s32be(0x09,sf);
    if (type == 0) { /* Rage */
        /* 0x0d: null */
        /* 0x11: crc? */
        /* 0x15: flag? */
        offset = 0x19;
    }
    else { /* TEW */
        offset = 0x0D;
    }

    stream_size = read_u32be(offset + 0x00,sf);
    offset = read_u32be(offset + 0x04,sf); /* absolute but typically right after this */

    /* 0x00: crc? */
    codec = read_u8(offset + 0x04,sf); /* assumed */
    switch(codec) {
        case 0x00: {
            /* 0x05: null? */
            num_samples = read_s32be(offset + 0x09,sf);
            /* 0x0D: null? */
            /* 0x11: loop related? */
            /* 0x1d: stream size? */
            /* others: ? */

            /* fmt at 0x31 (codec, avg bitrate, etc) */
            channels = read_u16le(offset + 0x33, sf);
            sample_rate = read_u32le(offset + 0x35, sf);
            block_size = read_u16le(offset + 0x3d, sf);
            bps = read_u16le(offset + 0x3f, sf);

            /* 0x41: MSADPCM fmt extra */
            if (!msadpcm_check_coefs(sf, offset + 0x41 + 0x02 + 0x02))
                goto fail;

            extension = "msadpcm";
            break;
        }

        case 0x01: {
            uint32_t table_entries;

            /* 0x05: stream size? */
            num_samples = read_s32be(offset + 0x09,sf);
            /* 0x0D: 0x40? */
            /* 0x11: loop related? */
            loop_start = read_s32be(offset + 0x15,sf);
            /* 0x19: null */
            table_entries = read_u32be(offset + 0x1d,sf);

            /* skip seek table, format: frame size (16b) + frame samples (16b)
             * (first entry may be 0 then next entry x2 samples to mimic encoder latency?) */
            offset += 0x21 + table_entries * 0x04;

            sample_rate = read_u32be(offset + 0x00, sf);
            channels = read_u32be(offset + 0x04, sf);
            /* 0x0c: MSF codec */

            extension = type == 0 ? "msadpcm" : "msf";
            break;
        }
        default:
            goto fail;
    }

    sb = open_streamfile_by_ext(sf, extension);
    if (!sb) goto fail;
    
    if (stream_size != get_streamfile_size(sb))
        goto fail;


    loop_flag = (loop_start > 0);
    start_offset = 0x00;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_MZRT;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = num_samples;

    switch(codec) {
        case 0x0002:
            if (bps != 4) goto fail;
            vgmstream->coding_type = coding_MSADPCM;
            vgmstream->layout_type = layout_none;
            vgmstream->frame_size = block_size;
            break;

#ifdef VGM_USE_MPEG
        case 0x01: {
            mpeg_custom_config cfg = {0};

            cfg.skip_samples = 1152; /* seems ok */

            vgmstream->codec_data = init_mpeg_custom(sb, start_offset, &vgmstream->coding_type, vgmstream->channels, MPEG_STANDARD, &cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples -= cfg.skip_samples;
            vgmstream->loop_start_sample -= cfg.skip_samples;
            vgmstream->loop_end_sample -= cfg.skip_samples;
            break;
        }
#endif

        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sb, start_offset))
        goto fail;
    close_streamfile(sb);
    return vgmstream;

fail:
    close_streamfile(sb);
    close_vgmstream(vgmstream);
    return NULL;
}

/* bsnf - id Tech 5 audio [Wolfenstein: The New Order (multi), Wolfenstein: The Old Blood (PS4)] */
VGMSTREAM* init_vgmstream_bsnf(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, offset, extra_offset;
    size_t stream_size;
    int target_subsong = sf->stream_index, num_languages,
        loop_flag, channels, codec, sample_rate; //, block_size = 0, bps = 0;
    int32_t num_samples, loop_start = 0;
    char language[0x10];
    STREAMFILE* sb = NULL;


    /* checks */
    if (!is_id32be(0x00, sf, "bsnf"))
        goto fail;

    if (!check_extensions(sf, "bsnd"))
        goto fail;

    num_languages = read_u32be(0x04, sf);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > num_languages || num_languages < 1)
        goto fail;

    offset = 0x08 + (target_subsong - 1) * 0x18;

    read_string(language, 0x10, offset + 0x00, sf);
    stream_size = read_u32be(offset + 0x10, sf);
    offset = read_u32be(offset + 0x14, sf); /* absolute but typically right after this */

    /* 0x00: crc? */
    /* 0x04: CBR samples or 0 if VBR */
    num_samples = read_s32be(offset + 0x08, sf);
    loop_start  = read_s32be(offset + 0x0c, sf);
    /* 0x10: stream size? */
    
    codec       = read_u16le(offset + 0x14, sf);
    channels    = read_u16le(offset + 0x16, sf);
    sample_rate = read_u32le(offset + 0x18, sf);
  //block_size  = read_u16le(offset + 0x20, sf);
  //bps         = read_u16le(offset + 0x22, sf);

    extra_offset = offset + 0x24;

    /* extra data per codec */
    /* 0x0055 - msf */
    /*   0x00: table entries */
    /*   0x04: seek table, format: frame size (16b) + frame samples (16b) */
    /* 0x0166 - xma */
    /*   0x00: extra size */
    /*   0x02: xma config and block table */
    /* 0x674F - vorbis */
    /*   0x00: extra size */
    /*   0x02: num samples */
    /* 0x42D2 - at9 */
    /*   0x00: extra size */
    /*   0x02: encoder delay */
    /*   0x04: channel config */
    /*   0x08: ATRAC9 GUID */
    /*   0x1c: ATRAC9 config */

    {
        char filename[PATH_LIMIT];
        get_streamfile_basename(sf, filename, sizeof(filename));

        if (language[0] != '\0') {
            strcat(filename, "_");
            strcat(filename, language);
        }

        sb = open_streamfile_by_filename(sf, filename);
        if (!sb) {
            if (language[0] != '\0') {
                // fill missing languages with blanks
                vgmstream = init_vgmstream_silence(channels, sample_rate, num_samples);
                if (!vgmstream) goto fail;

                vgmstream->meta_type = meta_BSNF;
                vgmstream->num_streams = num_languages;
                snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%s (missing)", language);

                return vgmstream;
            }

            goto fail;
        }
    }

    if (stream_size != get_streamfile_size(sb))
        goto fail;

    loop_flag = (loop_start > 0); /* loops from 0 on some codecs aren't detectable though */
    start_offset = 0x00;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_BSNF;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = num_samples;
    vgmstream->num_streams = num_languages;
    strncpy(vgmstream->stream_name, language, STREAM_NAME_SIZE);

    /* for codecs with explicit encoder delay (mp3/at9/etc) num_samples includes it
     * ex. mus_c05_dream_doorloop_* does full loops; with some codecs loop start is encoder delay and num_samples
     * has extra delay samples compared to codecs with implicit delay (ex. mp3 1152 to 101152 vs ogg 0 to 100000),
     * but there is no header value for encoder delay, maybe engine hardcodes it? */

    switch (codec) {

#ifdef VGM_USE_MPEG
        case 0x0055: {
            mpeg_custom_config cfg = { 0 };

            //cfg.skip_samples = 1152; /* observed default */

            vgmstream->codec_data = init_mpeg_custom(sb, start_offset, &vgmstream->coding_type, vgmstream->channels, MPEG_STANDARD, &cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples -= cfg.skip_samples;
            vgmstream->loop_start_sample -= cfg.skip_samples;
            vgmstream->loop_end_sample -= cfg.skip_samples;
            break;
        }
#endif

#ifdef VGM_USE_FFMPEG
        case 0x0166: {
            int block_size = 0x800;

            vgmstream->codec_data = init_ffmpeg_xma2_raw(sb, start_offset, stream_size, num_samples, channels, sample_rate, block_size, 0);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            xma_fix_raw_samples(vgmstream, sb, start_offset, stream_size, 0x00, 1, 1);
            break;
        }
#endif

#ifdef VGM_USE_VORBIS
        case 0x674F: {
            vgmstream->codec_data = init_ogg_vorbis(sb, start_offset, stream_size, NULL);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_OGG_VORBIS;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif

#ifdef VGM_USE_ATRAC9
        case 0x42D2: {
            atrac9_config cfg = { 0 };

            /* extra offset: RIFF fmt extra data (extra size, frame size, GUID, etc), no fact samples/delay */

            cfg.channels = vgmstream->channels;
            //cfg.encoder_delay = read_u16le(extra_offset + 0x02, sf) / 4; /* seemingly one subframe = 256 */
            cfg.config_data = read_u32be(extra_offset + 0x1c, sf);

            vgmstream->codec_data = init_atrac9(&cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_ATRAC9;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples -= cfg.encoder_delay;
            vgmstream->loop_start_sample -= cfg.encoder_delay;
            vgmstream->loop_end_sample -= cfg.encoder_delay;
            break;
        }
#endif

        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sb, start_offset))
        goto fail;
    close_streamfile(sb);
    return vgmstream;

fail:
    close_streamfile(sb);
    close_vgmstream(vgmstream);
    return NULL;
}
