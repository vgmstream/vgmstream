#include "meta.h"
#include "../coding/coding.h"

static int get_ogg_page_size(STREAMFILE* sf, off_t page_offset, off_t *out_data_offset, size_t *out_page_size);
static int ogg_get_num_samples(STREAMFILE* sf, off_t start_offset);

typedef struct {
    int original_rate;
    int32_t loop_start;
    int32_t loop_end;
    bool loop_flag;
} ogg_opus_comment_info_t;

static bool read_comments(STREAMFILE* sf, uint32_t data_offset, ogg_opus_comment_info_t* info);

/* Ogg Opus - standard Opus with optional looping comments [The Pillars of Earth (PC), Monster Boy and the Cursed Kingdom (Switch)] */
VGMSTREAM* init_vgmstream_ogg_opus(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, data_offset = 0;


    /* checks */
    if (!is_id32be(0x00,sf, "OggS"))
        return NULL;
    /* .opus: standard, .lopus: fake extension for plugins
     * .ogg: less common, .logg: same
     * .bgm: Utawarerumono: Mask of Truth (PC)
     * .oga: niconico app (Switch) */
    if (!check_extensions(sf, "opus,lopus,ogg,logg,bgm,oga"))
        return NULL;
    /* see: https://tools.ietf.org/html/rfc7845.html */

    size_t page_size = 0;
    start_offset = 0x00;

    /* parse 1st page: opus head */
    if (!get_ogg_page_size(sf, start_offset, &data_offset, &page_size))
        return NULL;
    if (!is_id32be(data_offset+0x00,sf, "Opus") ||
        !is_id32be(data_offset+0x04,sf, "Head"))
        return NULL;
    /* 0x01: version 1, fixed */
    int channels = read_u8(data_offset+0x09,sf);
    /* 0x0A: skip samples */
    int original_rate = read_s32le(data_offset+0x0c,sf);
    /* 0x10: gain */
    /* 0x12: mapping family */

    /* parse 2nd page: opus tags (also mandatory) */
    if (!get_ogg_page_size(sf, start_offset+page_size, &data_offset, &page_size))
        return NULL;
    if (!is_id32be(data_offset+0x00,sf, "Opus") ||
        !is_id32be(data_offset+0x04,sf, "Tags"))
        return NULL;

    ogg_opus_comment_info_t info = {0};
    info.original_rate = original_rate;
    if (!read_comments(sf, data_offset, &info))
        return NULL;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, info.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_OGG_OPUS;
    vgmstream->sample_rate = 48000; /* Opus always resamples to this */
    vgmstream->num_samples = ogg_get_num_samples(sf, 0x00);
    vgmstream->loop_start_sample = info.loop_start;
    vgmstream->loop_end_sample = info.loop_end;

#ifdef VGM_USE_FFMPEG
    {
        vgmstream->codec_data = init_ffmpeg_offset(sf, start_offset, get_streamfile_size(sf));
        if (!vgmstream->codec_data) goto fail;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;
        vgmstream->channel_layout = ffmpeg_get_channel_layout(vgmstream->codec_data);
        /* FFmpeg+libopus handles skip samples ok, FFmpeg+opus doesn't */
    }
#else
    goto fail;
#endif
    
    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


/* parse OggS's bizarre segment table */
static int get_ogg_page_size(STREAMFILE* sf, off_t page_offset, off_t* p_data_offset, size_t* p_page_size) {

    if (!is_id32be(page_offset+0x00,sf, "OggS"))
        return false;

    /* read all segment sizes */
    size_t page_size = 0;
    uint8_t segments = read_u8(page_offset+0x1a, sf);
    for (int i = 0; i < segments; i++) {
        page_size += read_u8(page_offset + 0x1b + i, sf);
    }
    page_size += 0x1b + segments;

    if (p_data_offset) *p_data_offset = page_offset + 0x1b + segments;
    if (p_page_size) *p_page_size = page_size;
    return true;
}

/* Ogg doesn't have num_samples info, must manually seek+read last granule
 * (Xiph is insistent this is the One True Way). */
static int ogg_get_num_samples(STREAMFILE *sf, off_t start_offset) {
    uint32_t expected_id = get_id32be("OggS");
    off_t offset = get_streamfile_size(sf) - 0x04-0x01-0x01-0x08-0x04-0x04-0x04;

    //todo better buffer reads (Ogg page max is 0xFFFF)
    //lame way to force buffer, assuming it's around that
    read_u32be(offset - 0x4000, sf);

    while (offset >= start_offset) {
        uint32_t current_id = read_u32be(offset, sf);
        if (current_id == expected_id) { /* if more checks are needed last page starts with 0x0004 */
            return read_s32le(offset+0x04+0x01+0x01, sf); /* get last granule = total samples (64b but whatevs) */
        }

        offset--;
    }

    return 0;
}

static bool vstr_startswith(const char* str, const char* substr) {
    return strstr(str, substr) == str;
}

static bool read_comments(STREAMFILE* sf, uint32_t data_offset, ogg_opus_comment_info_t* info) {
    char user_comment[1024];
    bool has_encoder_options = false, has_title = false;

    uint32_t vendor_size = read_u32le(data_offset + 0x08,sf);
    if (vendor_size >= 0x400) // 32-bit???
        return false;
    uint32_t comment_count = read_u32le(data_offset + 0x0c + vendor_size, sf);
    if (comment_count >= 256) // did Opus really need million of comments?
        return false;

    /* parse comments */
    off_t offset = data_offset + 0x0c + vendor_size + 0x04;
    for (int i = 0; i < comment_count; i++) {
        uint32_t user_comment_size = read_u32le(offset + 0x00,sf);
        if (user_comment_size >= sizeof(user_comment) - 1) {
            offset += 0x04 + user_comment_size;
            continue; // skip rather than stop for lyrics/etc
        }

        // comments aren't null-terminated and need an extra byte
        read_string(user_comment, user_comment_size + 1, offset + 0x04,sf);
        offset += 0x04 + user_comment_size;

        /* parse loop strings */
        if (vstr_startswith(user_comment,"LOOP_START=")) { /* Monster Boy and the Cursed Kingdom (Switch) */
            info->loop_start = atol(strrchr(user_comment,'=')+1);
            info->loop_flag = (info->loop_start >= 0);
        }
        else if (vstr_startswith(user_comment,"LOOP_END=")) { /* LOOP_START pair */
            info->loop_end = atol(strrchr(user_comment,'=')+1);
        }
        else if (vstr_startswith(user_comment,"ENCODER_OPTIONS=")) { /* for detection */
            has_encoder_options = true;
        }
        else if (vstr_startswith(user_comment,"TITLE=")) { /* for detection */
            has_title = true;
        }
        else if (vstr_startswith(user_comment,"LoopStart=")) { /* Utawarerumono: Mask of Truth (PC) */
            info->loop_start= atol(strrchr(user_comment,'=')+1);
            info->loop_flag = (info->loop_start >= 0);
        }
        else if (vstr_startswith(user_comment,"LoopEnd=")) { /* LoopStart pair */
            info->loop_end = atol(strrchr(user_comment,'=')+1);
        }
        else if (vstr_startswith(user_comment, "loops=")) { /* The Legend of Heroes: Trails of Cold Steel III (Switch) */
            sscanf(strrchr(user_comment, '=') + 1, "%d-%d", &info->loop_start, &info->loop_end);
            info->loop_flag = true;
        }
        else if (vstr_startswith(user_comment,"loopstart=")) { /* The Legend of Heroes: Kuro no Kiseki (PC) */
            info->loop_start= atol(strrchr(user_comment,'=')+1);
            info->loop_flag = (info->loop_start >= 0);
        }
        else if (vstr_startswith(user_comment,"loopend=")) { /* LoopStart pair */
            info->loop_end = atol(strrchr(user_comment,'=')+1);
        }

        ;VGM_LOG("OggOpus: user_comment=%s\n", user_comment);
    }


    /* Monster Boy has loop points for 44100hz (what), but Opus is resampled so
        * they must be adjusted (with extra checks just in case). */
    if (info->loop_flag && info->original_rate < 48000 && has_encoder_options && has_title) {
        float modifier = 48000.0f / (float)info->original_rate;
        info->loop_start = info->loop_start * modifier;
        info->loop_end = info->loop_end * modifier;
    }

    return true;
}