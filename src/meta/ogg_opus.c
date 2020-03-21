#include "meta.h"
#include "../coding/coding.h"


static int get_ogg_page_size(STREAMFILE *streamFile, off_t page_offset, off_t *out_data_offset, size_t *out_page_size);
static int ogg_get_num_samples(STREAMFILE *streamFile, off_t start_offset);

/* Ogg Opus - standard Opus with optional looping comments [The Pillars of Earth (PC), Monster Boy and the Cursed Kingdom (Switch)] */
VGMSTREAM * init_vgmstream_ogg_opus(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, data_offset = 0;
    size_t page_size = 0;
    int loop_flag, channel_count, original_rate;
    int loop_start = 0, loop_end = 0;


    /* checks */
    /* .opus: standard, .lopus: fake extension for plugins
     * .ogg: less common, .logg: same
     * .bgm: Utawarerumono: Mask of Truth (PC) */
    if (!check_extensions(streamFile, "opus,lopus,ogg,logg,bgm"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x4F676753) /* "OggS" */
        goto fail;
    /* see: https://tools.ietf.org/html/rfc7845.html */

    start_offset = 0x00;

    /* parse 1st page: opus head */
    if (!get_ogg_page_size(streamFile, start_offset, &data_offset, &page_size))
        goto fail;
    if (read_32bitBE(data_offset+0x00,streamFile) != 0x4F707573 &&  /* "Opus" */
        read_32bitBE(data_offset+0x04,streamFile) != 0x48656164)    /* "Head" */
        goto fail;
    /* 0x01: version 1, fixed */
    channel_count = read_8bit(data_offset+0x09,streamFile);
    /* 0x0A: skip samples */
    original_rate = read_32bitLE(data_offset+0x0c,streamFile);
    /* 0x10: gain */
    /* 0x12: mapping family */

    /* parse 2nd page: opus tags (also mandatory) */
    if (!get_ogg_page_size(streamFile, start_offset+page_size, &data_offset, &page_size))
        goto fail;
    if (read_32bitBE(data_offset+0x00,streamFile) != 0x4F707573 &&  /* "Opus" */
        read_32bitBE(data_offset+0x04,streamFile) != 0x54616773)    /* "Tags" */
        goto fail;

    loop_flag = 0;
    {
        char user_comment[1024+1];
        off_t offset;
        int vendor_size, comment_count, user_comment_size, user_comment_max;
        int i;
        int has_encoder_options = 0, has_title = 0;

        vendor_size   = read_32bitLE(data_offset+0x08,streamFile);
        comment_count = read_32bitLE(data_offset+0x0c+vendor_size,streamFile);

        /* parse comments */
        offset = data_offset + 0x0c + vendor_size + 0x04;
        for (i = 0; i < comment_count; i++) {
            user_comment_size = read_32bitLE(offset+0x00,streamFile);
            user_comment_max = user_comment_size > 1024 ? 1024 : user_comment_size;
            read_string(user_comment,user_comment_max+1, offset+0x04,streamFile);


            /* parse loop strings */
            if (strstr(user_comment,"LOOP_START=")==user_comment) { /* Monster Boy and the Cursed Kingdom (Switch) */
                loop_start = atol(strrchr(user_comment,'=')+1);
                loop_flag = (loop_start >= 0);
            }
            else if (strstr(user_comment,"LOOP_END=")==user_comment) { /* LOOP_START pair */
                loop_end = atol(strrchr(user_comment,'=')+1);
            }
            else if (strstr(user_comment,"ENCODER_OPTIONS=")==user_comment) { /* for detection */
                has_encoder_options = 1;
            }
            else if (strstr(user_comment,"TITLE=")==user_comment) { /* for detection */
                has_title = 1;
            }
            else if (strstr(user_comment,"LoopStart=")==user_comment) { /* Utawarerumono: Mask of Truth (PC) */
                loop_start= atol(strrchr(user_comment,'=')+1);
                loop_flag = (loop_start >= 0);
            }
            else if (strstr(user_comment,"LoopEnd=")==user_comment) { /* LoopStart pair */
                loop_end = atol(strrchr(user_comment,'=')+1);
            }
            else if (strstr(user_comment, "loops=") == user_comment) { /* The Legend of Heroes: Trails of Cold Steel III (Switch) */
                sscanf(strrchr(user_comment, '=') + 1, "%d-%d", &loop_start, &loop_end);
                loop_flag = 1;
            }

            //;VGM_LOG("OggOpus: user_comment=%s\n", user_comment);
            offset += 0x04 + user_comment_size;
        }


        /* Monster Boy has loop points for 44100hz (what), but Opus is resampled so
         * they must be adjusted (with extra checks just in case). */
        if (loop_flag && original_rate < 48000 && has_encoder_options && has_title) {
            float modifier = 48000.0f / (float)original_rate;
            loop_start = loop_start * modifier;
            loop_end = loop_end * modifier;
        }
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_OGG_OPUS;
    vgmstream->sample_rate = 48000; /* Opus always resamples to this */
    vgmstream->num_samples = ogg_get_num_samples(streamFile, 0x00);
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

#ifdef VGM_USE_FFMPEG
    {
        vgmstream->codec_data = init_ffmpeg_offset(streamFile, start_offset, get_streamfile_size(streamFile));
        if (!vgmstream->codec_data) goto fail;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;
        vgmstream->channel_layout = ffmpeg_get_channel_layout(vgmstream->codec_data);
        /* FFmpeg+libopus handles skip samples ok, FFmpeg+opus doesn't */
    }
#else
    goto fail;
#endif
    
    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


/* parse OggS's bizarre segment table */
static int get_ogg_page_size(STREAMFILE *streamFile, off_t page_offset, off_t *out_data_offset, size_t *out_page_size) {
    uint8_t segments;
    size_t page_size = 0;
    int i;

    if (read_32bitBE(page_offset+0x00,streamFile) != 0x4F676753) /* "OggS" */
        goto fail;

    /* read all segment sizes */
    segments = (uint8_t)read_8bit(page_offset+0x1a, streamFile);
    for (i = 0; i < segments; i++) {
        page_size += (uint8_t)read_8bit(page_offset + 0x1b + i, streamFile);
    }
    page_size += 0x1b + segments;

    if (out_data_offset) *out_data_offset = page_offset + 0x1b + segments;
    if (out_page_size) *out_page_size = page_size;
    return 1;
fail:
    return 0;
}

/* Ogg doesn't have num_samples info, must manually seek+read last granule
 * (Xiph is insistent this is the One True Way). */
static int ogg_get_num_samples(STREAMFILE *streamFile, off_t start_offset) {
    uint32_t expected_id = 0x4F676753;
    off_t offset = get_streamfile_size(streamFile) - 0x04-0x01-0x01-0x08-0x04-0x04-0x04;

    //todo better buffer reads (Ogg page max is 0xFFFF)
    //lame way to force buffer, assuming it's around that
    read_32bitBE(offset - 0x4000, streamFile);

    while (offset >= start_offset) {
        uint32_t current_id = read_32bitBE(offset, streamFile);
        if (current_id == expected_id) { /* if more checks are needed last page starts with 0x0004 */
            return read_32bitLE(offset+0x04+0x01+0x01, streamFile); /* get last granule = total samples (64b but whatevs) */
        }

        offset--;
    }

    return 0;
}
