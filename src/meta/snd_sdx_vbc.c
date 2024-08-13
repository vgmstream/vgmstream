#include "meta.h"
#include "../coding/coding.h"
#include "../util/text_reader.h"
#include "../util/companion_files.h"

typedef enum { NONE_ = 0, REALSIZE_ = 1, SIZE__ = 2, OFFSET_ = 3, FREQ_ = 4, LOOP_ = 10, 
               REVERB_ = 11, VOLL_ = 5, VOLR_ = 6, VOLUME_ = 7, STEREO_ = 8, MONO_ = 9, SPU_ = 12, 
               COLLECTION_ = 14, CD_ = 13, SOUND_SOURCE_ = 17, STRING___ = 16, INT___ = 15} treyarch_ps2snd_comms;

typedef struct {
    char basename[10];
    STREAMFILE* sf_snd;
    STREAMFILE* sf_sdx;
    STREAMFILE* sf_vbc;
    STREAMFILE* sf_stream_vbc;

    bool is_stream_snd; /* STREAM.SND, unused but present in some PS2 games. */
    bool snd_start_offset_set;
    bool sdx_start_offset_set;
    bool subsong_set_to_zero;
    bool text_reader_set;
    bool subsong_set;
    bool sound_has_defined_name_or_id;
    bool stream_has_name_rather_than_id;
    bool calc_total_subsongs_first;

    text_reader_t tr;
    uint8_t buf[255 + 1];
    int line_len;
    char* line;
    int32_t snd_start_offset;
    int32_t sdx_start_offset;

    int total_subsongs; /* must be counted either per text line or per binary struct. */
    int expected_subsong;
    int target_subsong;

    treyarch_ps2snd_comms comms;
    char vbc_name[64];
    char stream_name[64]; /* sound_name */
    int32_t id_; /* sound_id */
    int32_t realsize; /* real_sound_size */
    int32_t size; /* sound_data_size */
    int32_t offset; /* sound_offset */
    int32_t freq; /* sample_rate */
    uint16_t pitch; /* sound_sample_pitch */
    // ^ PS2 games needed sample-rate to pitch conversion to play any sound in-game, vgmstream code can only convert pitch to sample-rate so may be useful.
    uint16_t volume_l; /* sound_left_volume */
    // ^ PS2 games needed separate volume value conversion to play any sound in-game, vgmstream code leaves volume in the player's hands.
    uint16_t volume_r; /* sound_right_volume */
    // ^ ditto.
    int32_t source; /* sound_source */
    int32_t flags; /* sound_flags (snd format exclusive) */
    /* sdx format introduced three flag vars */
    int32_t flags1;
    int32_t flags2;
    int32_t flags3;

    size_t sdx_size;
    int8_t sdx_block_info_size;
} treyarch_ps2snd;

static int parse_treyarch_ps2_snd(treyarch_ps2snd* ps2snd);
static int parse_sdx(treyarch_ps2snd* ps2snd);

void close_treyarch_ps2snd_streamfiles(treyarch_ps2snd* ps2snd);

/* SND+VBC - from Treyarch games */
VGMSTREAM* init_vgmstream_snd_vbc(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_body = NULL;
    treyarch_ps2snd ps2snd = {0};
    char snd_name[PATH_LIMIT];
    int channels, loop_flag;

    /* checks */
    if (!check_extensions(sf,"snd"))
        goto fail;

    ps2snd.sf_snd = sf;
    ps2snd.subsong_set_to_zero = false;
    ps2snd.snd_start_offset_set = false;
    ps2snd.subsong_set = false;
    ps2snd.target_subsong = sf->stream_index;

    /* get basename of snd file */
    get_streamfile_basename(sf, ps2snd.basename,sizeof(ps2snd.basename));
    /* bool config for when STREAM.SND opens */
    if (strcmp(ps2snd.basename,"STREAM") == 0)
        ps2snd.is_stream_snd = true;

    /* (todo) STREAM.SND text lines have an extra field denoting which language this sound belongs to. 
     * for now, do nothing and report opening failures instead. */
    if (ps2snd.is_stream_snd)
        goto fail;

    if (!parse_treyarch_ps2_snd(&ps2snd))
        goto fail;

    /* snd format - auto-generated text file. */
    /* first 2-3 text lines are comments, avoid parsing them. */
    /* no "number of entries" number available so must be obtained per-line. */

    channels = (ps2snd.flags & 0x01) ? 2 : 1;
    loop_flag = (ps2snd.flags & 0x02) ? 1 : 0;
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SND_VBC;
    vgmstream->sample_rate = ps2snd.freq;
    vgmstream->num_streams = ps2snd.total_subsongs;
    vgmstream->stream_size = ps2snd.size;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x800;
    vgmstream->num_samples = ps_bytes_to_samples(ps2snd.size, channels);
    vgmstream->loop_start_sample = 0;
    vgmstream->loop_end_sample = vgmstream->num_samples;

    sf_body = (ps2snd.flags & 0x04) ? ps2snd.sf_stream_vbc : ps2snd.sf_vbc;
    if (!sf_body) goto fail;

    if (ps2snd.sound_has_defined_name_or_id == true)
    {
        if (ps2snd.stream_has_name_rather_than_id == false)
        {
            snprintf(vgmstream->stream_name, 256, "%08x",ps2snd.id_);
        }
        else {
            strcpy(vgmstream->stream_name, ps2snd.stream_name);
        }
    }

    if (!vgmstream_open_stream(vgmstream, sf_body, ps2snd.offset))
        goto fail;
    close_treyarch_ps2snd_streamfiles(&ps2snd);
    return vgmstream;
fail:
    close_treyarch_ps2snd_streamfiles(&ps2snd);
    close_vgmstream(vgmstream);
    return NULL;
}

/* SDX+VBC [Minority Report: Everybody Runs (PS2)] */
VGMSTREAM* init_vgmstream_sdx_vbc(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_body = NULL;
    treyarch_ps2snd ps2snd = { 0 };
    int channels, loop_flag;

    /* checks */
    if (!check_extensions(sf,"sdx")) return NULL;

    ps2snd.sf_sdx = sf;
    ps2snd.subsong_set_to_zero = false;
    ps2snd.sdx_start_offset_set = false;
    ps2snd.subsong_set = false;
    ps2snd.target_subsong = sf->stream_index;
    ps2snd.sdx_block_info_size = 0x58;

    /* get basename of sdx file */
    get_streamfile_basename(sf, ps2snd.basename, sizeof(ps2snd.basename));

    if (!parse_sdx(&ps2snd)) goto fail;

    /* sdx file - xsh+xsd/xss format from Treyarch Xbox games if first three fields (version number, zero, number of sounds) never existed. */

    channels = (ps2snd.flags3 & 0x01) ? 2 : 1;
    loop_flag = (ps2snd.flags3 & 0x02) ? 1 : 0;
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SDX_VBC;
    vgmstream->sample_rate = round10((48000 * ps2snd.pitch) / 4096); /* alternatively, (ps2snd.pitch * 48000) << 12 */
    vgmstream->num_streams = ps2snd.total_subsongs;
    vgmstream->stream_size = ps2snd.size;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x800;
    vgmstream->num_samples = ps_bytes_to_samples(ps2snd.size, channels);
    vgmstream->loop_start_sample = 0;
    vgmstream->loop_end_sample = vgmstream->num_samples;

    sf_body = (ps2snd.flags3 & 0x10) ? ps2snd.sf_stream_vbc : ps2snd.sf_vbc;
    if (!sf_body) goto fail;

    strcpy(vgmstream->stream_name, ps2snd.stream_name);

    if (!vgmstream_open_stream(vgmstream, sf_body, ps2snd.offset))
        goto fail;
    close_treyarch_ps2snd_streamfiles(&ps2snd);
    return vgmstream;
fail:
    close_treyarch_ps2snd_streamfiles(&ps2snd);
    close_vgmstream(vgmstream);
}

void close_treyarch_ps2snd_streamfiles(treyarch_ps2snd* ps2snd)
{
    if (ps2snd->sf_vbc) close_streamfile(ps2snd->sf_vbc);
    if (ps2snd->sf_stream_vbc) close_streamfile(ps2snd->sf_stream_vbc);
}

typedef struct {
    int line_pos;
    int bkup_pos;
    int line_size;

    bool got_token;
    char token_str[64];
    int32_t token_type;
    int32_t token_num;
} line_go_through;

static int parse_line_comms(STREAMFILE* sf, treyarch_ps2snd* ps2snd);
static bool line_is_comment(const char* val);
static treyarch_ps2snd_comms get_string_comm_key(line_go_through* cfg, const char* val, int32_t* out_val1, int32_t* out_val2, char* out_val3);
static void clear_comm_key(line_go_through* cfg);

static const char* const treyarch_ps2_stream_sources[8] = { "SFX", "AMBIENT", "MUSIC", "VOICE", "MOVIE", "USER1", "USER2", "N/A" };

static int parse_treyarch_ps2_snd(treyarch_ps2snd* ps2snd) {
    int i;

    if (ps2snd->snd_start_offset_set == false) {
        ps2snd->snd_start_offset = read_bom(ps2snd->sf_snd);
        ps2snd->snd_start_offset_set = true;
    }

    {
        for (i = 0; i < 2; i++)
        {
            ps2snd->text_reader_set = false;
            switch (i)
            {
            case 0:
                ps2snd->calc_total_subsongs_first = true;
                break;
            case 1:
                ps2snd->calc_total_subsongs_first = false;
                break;
            default:
                break;
            }

            if (ps2snd->text_reader_set == false)
            {
                if (!text_reader_init(&ps2snd->tr, ps2snd->buf, sizeof(ps2snd->buf), ps2snd->sf_snd, ps2snd->snd_start_offset, 0)) { goto fail; }
                else { ps2snd->text_reader_set = true; }
            }

            do
            {
                ps2snd->line_len = text_reader_get_line(&ps2snd->tr, &ps2snd->line);

                if (ps2snd->line_len < 0)
                    goto fail;
                if (ps2snd->line == NULL)
                    break;
                if (ps2snd->line_len == 0)
                    continue;

                if (!parse_line_comms(ps2snd->sf_snd, ps2snd))
                    goto fail;

                if (ps2snd->calc_total_subsongs_first == false) {
                    if (ps2snd->subsong_set == true) { break; }
                }

            } while (ps2snd->line_len >= 0);

            if (ps2snd->calc_total_subsongs_first == false) {
                if (ps2snd->target_subsong < 0 || ps2snd->target_subsong > ps2snd->total_subsongs || ps2snd->total_subsongs < 1) goto fail; }
        }
    }

    return 1;
fail:
    return 0;
}

static int parse_line_comms(STREAMFILE* sf, treyarch_ps2snd* ps2snd) {
    bool is_comment;
    line_go_through lgt_ = { 0 };

    int32_t temp_val2;
    int32_t temp_val3;
    char temp_val4[64];
    char temp_val5[64];
    char temp_val6[32];
    ps2snd->sound_has_defined_name_or_id = true; /* line always starts with a name or a number. */
    bool spu_vbc_success = false;
    bool stream_vbc_success = false;
    ps2snd->subsong_set = false;

    is_comment = line_is_comment(ps2snd->line);
    if (!is_comment)
    {
        /* get line_size and see if line_pos isn't ahead of the former in some way (unlikely). */
        lgt_.line_size = strlen(ps2snd->line);
        if (lgt_.line_pos >= lgt_.line_size) goto fail;

        /* set subsong stuff first. */
        if (ps2snd->subsong_set_to_zero == false) {
            if (ps2snd->target_subsong == 0) ps2snd->target_subsong = 1;
            ps2snd->total_subsongs = 0;
            ps2snd->expected_subsong = 1;
            ps2snd->subsong_set_to_zero = true;
        }

        /* meh. */
        ps2snd->flags &= 0xfffffff7;
        ps2snd->flags &= 0xfffffffd;
        ps2snd->flags &= 0xfffffffe;
        ps2snd->flags |= 0x10;
        ps2snd->realsize = 0;
        ps2snd->size = 0;
        ps2snd->offset = 0;
        ps2snd->freq = 0;
        ps2snd->pitch = 0;
        ps2snd->volume_l = 0;
        ps2snd->volume_r = 0;
        ps2snd->source = 0;

        /* Treyarch PS2 games handled snd files by way of binary serialization. */
        /* much info gathered from GAS.IRX */
        while (ps2snd->comms = get_string_comm_key(&lgt_, ps2snd->line, &temp_val2, &temp_val3, &temp_val4), ps2snd->comms != 0)
        {
            switch (ps2snd->comms)
            {
            case NONE_:
                break;
            case REALSIZE_:
                if (ps2snd->calc_total_subsongs_first == false)
                {
                    if (get_string_comm_key(&lgt_, ps2snd->line, &temp_val2, &temp_val3, &temp_val4) == INT___) {
                        ps2snd->realsize = temp_val3;
                    }
                    else { clear_comm_key(&lgt_); }
                }
                break;
            case SIZE__:
                if (ps2snd->calc_total_subsongs_first == false) {
                    if (get_string_comm_key(&lgt_, ps2snd->line, &temp_val2, &temp_val3, &temp_val4) == INT___) {
                        ps2snd->size = temp_val3;
                    } else { clear_comm_key(&lgt_); }
                }

                break;
            case OFFSET_:
                if (ps2snd->calc_total_subsongs_first == false) {
                    if (get_string_comm_key(&lgt_, ps2snd->line, &temp_val2, &temp_val3, &temp_val4) == INT___) {
                        ps2snd->offset = temp_val3;
                    }
                    else { clear_comm_key(&lgt_); }
                }
                break;
            case FREQ_:
                if (ps2snd->calc_total_subsongs_first == false) {
                    if (get_string_comm_key(&lgt_, ps2snd->line, &temp_val2, &temp_val3, &temp_val4) == INT___) {
                        ps2snd->freq = temp_val3;
                        ps2snd->pitch = ((temp_val3 << 12) / 48000) & 0xffff;
                    }
                    else { clear_comm_key(&lgt_); }
                }
                break;
            case VOLL_:
                if (ps2snd->calc_total_subsongs_first == false) {
                    if (get_string_comm_key(&lgt_, ps2snd->line, &temp_val2, &temp_val3, &temp_val4) == INT___) {
                        ps2snd->volume_l = ((temp_val3 * 16383) / 100) & 0xffff;
                    }
                    else { clear_comm_key(&lgt_); }
                }
                break;
            case VOLR_:
                if (ps2snd->calc_total_subsongs_first == false) {
                    if (get_string_comm_key(&lgt_, ps2snd->line, &temp_val2, &temp_val3, &temp_val4) == INT___) {
                        ps2snd->volume_r = ((temp_val3 * 16383) / 100) & 0xffff;
                    }
                    else { clear_comm_key(&lgt_); }
                }
                break;
            case VOLUME_:
                if (ps2snd->calc_total_subsongs_first == false) {
                    if (get_string_comm_key(&lgt_, ps2snd->line, &temp_val2, &temp_val3, &temp_val4) == INT___) {
                        ps2snd->volume_l = ((temp_val3 * 16383) / 100) & 0xffff;
                        ps2snd->volume_r = ((temp_val3 * 16383) / 100) & 0xffff;
                    }
                    else { clear_comm_key(&lgt_); }
                }
                break;
            case STEREO_: /* stereo flag */
                if (ps2snd->calc_total_subsongs_first == false) {
                    ps2snd->flags |= 1;
                }
                break;
            case MONO_: /* mono flag */
                if (ps2snd->calc_total_subsongs_first == false) {
                    ps2snd->flags &= 0xfffffffe;
                }
                break;
            case LOOP_: /* loop flag */
                if (ps2snd->calc_total_subsongs_first == false) {
                    ps2snd->flags |= 2;
                }
                break;
            case REVERB_: /* reverb flag */
                /* (todo) actually set up reverb info. */
                if (ps2snd->calc_total_subsongs_first == false) {
                    ps2snd->flags |= 8;
                }
                break;
            case SPU_: /* spu vbc file flag */
                if (ps2snd->calc_total_subsongs_first == false) {
                    ps2snd->flags &= 0xffffffef;
                }
                break;
            case CD_: /* STREAM.VBC flag */
                if (ps2snd->calc_total_subsongs_first == false) {
                    ps2snd->flags |= 0x10;
                }
                break;
            case COLLECTION_: /* COLLECTION file definition, full file path of where vbc file is. */
                ps2snd->sound_has_defined_name_or_id = false;
                if (ps2snd->calc_total_subsongs_first == false) {
                    if (get_string_comm_key(&lgt_, ps2snd->line, &temp_val2, &temp_val3, &temp_val4) == STRING___) {
                        strcpy(ps2snd->vbc_name, temp_val4);
                    }
                    else { clear_comm_key(&lgt_); }
                }
                break;
            case INT___: /* lone number. */
                if (ps2snd->calc_total_subsongs_first == false) {
                    if (ps2snd->sound_has_defined_name_or_id == true) {
                        ps2snd->stream_has_name_rather_than_id = false;
                        ps2snd->id_ = temp_val3;
                    }
                }
                break;
            case STRING___: /* lone string char. */
                if (ps2snd->calc_total_subsongs_first == false) {
                    if (ps2snd->sound_has_defined_name_or_id == true) {
                        ps2snd->stream_has_name_rather_than_id = true;
                        strcpy(ps2snd->stream_name, temp_val4);
                    }
                }
                break;
            case SOUND_SOURCE_:
                if (ps2snd->calc_total_subsongs_first == false) {
                    ps2snd->source = temp_val3;
                }
                break;
            default:
                break;
            }

            if (ps2snd->calc_total_subsongs_first == false && ps2snd->sound_has_defined_name_or_id == true)
            {
                if (ps2snd->comms == SPU_)
                {
                    while (!ps2snd->sf_vbc)
                    {
                        // open vbc from the spu folder.
                        snprintf(temp_val5, sizeof(temp_val5), "spu\\%s.vbc", ps2snd->basename);
                        if (spu_vbc_success == false) ps2snd->sf_vbc = open_streamfile_by_filename(sf, temp_val5);
                        if (ps2snd->sf_vbc && spu_vbc_success == false) spu_vbc_success = true;

                        // open vbc from the same folder as the snd itself (unlikely).
                        snprintf(temp_val6, sizeof(temp_val6), "%s.vbc", ps2snd->basename);
                        if (spu_vbc_success == false) ps2snd->sf_vbc = open_streamfile_by_filename(sf, temp_val6);
                        if (ps2snd->sf_vbc && spu_vbc_success == false) spu_vbc_success = true;

                        // open vbc through an txtm file.
                        // vbc file shares the same basename as snd file does shouldn't be too hard to find the former file.
                        // (MENU.VBC has MENU.SND also, for one.)
                        // (todo) have vgmstream inform the end-user on how to make a txtm file.
                        if (spu_vbc_success == false) ps2snd->sf_vbc = read_filemap_file(sf, 0);
                        if (ps2snd->sf_vbc && spu_vbc_success == false) spu_vbc_success = true;

                        if (spu_vbc_success == false) { 
                            // tried everything, no dice.
                            // (todo) fill in the silence in case of an unsuccessful load.
                            if (!ps2snd->sf_vbc) continue; }
                        else {
                            // AFAIK when spu vbc is loaded nothing is done to the flags var itself. (info needs double-checking)
                            continue;
                        }
                    }
                }

                if (ps2snd->comms == CD_)
                {
                    while (!ps2snd->sf_stream_vbc)
                    {
                        // open STREAM.VBC from two folders back and into the stream folder.
                        if (stream_vbc_success == false) ps2snd->sf_stream_vbc = open_streamfile_by_filename(sf, "..\\..\\stream\\stream.vbc");
                        if (ps2snd->sf_stream_vbc && stream_vbc_success == false) stream_vbc_success = true;

                        // open STREAM.VBC from the stream folder if said folder goes alongside snd and/or vbc files (unlikely).
                        if (stream_vbc_success == false) ps2snd->sf_stream_vbc = open_streamfile_by_filename(sf, "stream\\stream.vbc");
                        if (ps2snd->sf_stream_vbc && stream_vbc_success == false) stream_vbc_success = true;

                        // open STREAM.VBC from the same folder as where the snd and/or vbc files are (unlikely).
                        if (stream_vbc_success == false) ps2snd->sf_stream_vbc = open_streamfile_by_filename(sf, "stream.vbc");
                        if (ps2snd->sf_stream_vbc && stream_vbc_success == false) stream_vbc_success = true;

                        // open STREAM.VBC through an txtm file.
                        // (todo) have vgmstream inform the end-user on how to make a txtm file.
                        if (stream_vbc_success == false) ps2snd->sf_stream_vbc = read_filemap_file(sf, 1);
                        if (ps2snd->sf_stream_vbc && stream_vbc_success == false) stream_vbc_success = true;

                        if (stream_vbc_success == false) {
                            // tried everything, no dice.
                            // (todo) fill in the silence in case of an unsuccessful load.
                            if (!ps2snd->sf_stream_vbc) continue;
                        } else {
                            // put in a flag denoting STREAM.VBC being loaded.
                            ps2snd->flags |= 4;
                            continue;
                        }
                    }
                }
            }
        }

        if (ps2snd->calc_total_subsongs_first == true) {
            if (ps2snd->sound_has_defined_name_or_id == true) { ps2snd->total_subsongs++; }
        }
        else {
            if (ps2snd->sound_has_defined_name_or_id == true) {
                if (ps2snd->expected_subsong == ps2snd->target_subsong) { ps2snd->subsong_set = true; }
                ps2snd->expected_subsong++;
            }
        }
    }

    return 1;
fail:
    return 0;
}

static bool line_is_comment(const char* val) {
    char is_comment;

    is_comment = val[0];
    if (is_comment != 0x3b /* ';' */) goto fail;

    return true;
fail:
    return false;
}

static treyarch_ps2snd_comms get_string_comm_key(line_go_through* cfg, const char* val, int32_t* out_val1, int32_t* out_val2, char* out_val3) {
    int ret_val;
    bool found_key_spot = false;
    int has_numbers = 0;
    int has_letters = 0;
    bool stream_name_is_not_all_numbers = false;
    int tmp_pos = 0;
    char val_chr;
    int tmp2_pos = 0;
    bool tmp3_bool;
    int tmp3_i1 = 0;
    int tmp3_is_zero;
    int32_t tmp4;

    if (cfg->line_pos >= cfg->line_size) goto fail;
    if (cfg->line_pos < 0) goto fail;

    if (cfg->got_token == false)
    {
        /* token isn't occupied, let's do business. */
        cfg->bkup_pos = cfg->line_pos;

        /* find an non-empty char so we can start work on identifying token. */
        do {
            if (cfg->line_pos >= cfg->line_size) break;
            val_chr = val[cfg->line_pos];
            if (val_chr != ' ') {
                found_key_spot = true;
                cfg->bkup_pos = cfg->line_pos;
                break;
            }
            if (val_chr == '\0') break;
            cfg->line_pos++;
        } while (true);

        if (found_key_spot == false)
        {
            /* do nothing if "key spot" is nowhere to be seen. */
            out_val3 = 0;
            ret_val = 0;
        }
        else {
            /* copy whole token to char array. */
            do {
                if (tmp_pos >= cfg->line_size) break;
                val_chr = val[cfg->line_pos];
                if (val_chr == ' ') break;
                if (val_chr == '\0') break;
                out_val3[tmp_pos] = val_chr;
                tmp_pos++;
                cfg->line_pos++;
            } while (true);

            /* put a zero in there (string is already read in its entirety anyway). */
            out_val3[tmp_pos] = '\0';
            tmp_pos = 0;

            /* convert token to uppercase (by way of bitmask checks against lowercase chars) into another char array. */
            do {
                val_chr = out_val3[tmp_pos];
                if (val_chr == '\0') break;
                if ((val_chr & 0x40) == 0x40) { if ((val_chr & 0x20) == 0x20) { val_chr ^= 0x20; } }
                cfg->token_str[tmp2_pos] = val_chr;
                tmp_pos++;
                tmp2_pos++;
            } while (true);

            cfg->token_str[tmp2_pos] = '\0';
            tmp2_pos = 0;

            tmp_pos = 0;

            /* now weigh in the options; 
             * if current char has letters (A-Z range only), count them to a separate var.
             * or if current char has numbers (0-9 range only), count them to another var. */
            while (true)
            {
                val_chr = cfg->token_str[tmp_pos];
                if (val_chr == '\0') break;
                if ((val_chr & 0x80) == 0) {
                    if ((val_chr & 0x40) == 0x40) { has_letters++; }
                    else if ((val_chr & 0x20) == 0x20) { if ((val_chr & 0x10) == 0x10) { has_numbers++; } }
                }
                tmp_pos++;
            }

            *out_val1 = 0;

            /* then, actually weigh in on whether or not the token we got is nothing but numbers, letters, or a mix of the two. */
            if (has_letters == 0 && has_numbers != 0) { stream_name_is_not_all_numbers = false; }
            else if (has_letters != 0 && has_numbers == 0) { stream_name_is_not_all_numbers = true; }
            else if (has_letters != 0 && has_numbers != 0) { stream_name_is_not_all_numbers = true; }

            if (stream_name_is_not_all_numbers == true)
            {
                /* identify if already-obtained token is based on existing tokens an do some work with them. */
                if (strcmp(cfg->token_str, "SIZE") == 0) { *out_val1 = 2; } // <- set to 1 in [Spider-Man (2002) (PS2)]
                else if (strcmp(cfg->token_str, "REALSIZE") == 0) { *out_val1 = 1; } // <- present in [Kelly Slater's Pro Surfer (PS2)] and [NHL 2K3 (PS2)]
                else if (strcmp(cfg->token_str, "OFFSET") == 0) { *out_val1 = 3; } // <- set to 2 in [Spider-Man (2002) (PS2)]
                else if (strcmp(cfg->token_str, "FREQ") == 0) { *out_val1 = 4; } // <- set to 3 in [Spider-Man (2002) (PS2)]
                else if (strcmp(cfg->token_str, "LOOP") == 0) { *out_val1 = 10; } // <- set to 9 in [Spider-Man (2002) (PS2)]
                else if (strcmp(cfg->token_str, "REVERB") == 0) { *out_val1 = 11; } // <- present in [Kelly Slater's Pro Surfer (PS2)] and [NHL 2K3 (PS2)]
                else if (strcmp(cfg->token_str, "VOLL") == 0) { *out_val1 = 5; } // <- set to 4 in [Spider-Man (2002) (PS2)]
                else if (strcmp(cfg->token_str, "VOLR") == 0) { *out_val1 = 6; } // <- set to 5 in [Spider-Man (2002) (PS2)]
                else if (strcmp(cfg->token_str, "VOLUME") == 0) { *out_val1 = 7; } // <- set to 6 in [Spider-Man (2002) (PS2)]
                else if (strcmp(cfg->token_str, "STEREO") == 0) { *out_val1 = 8; } // <- set to 7 in [Spider-Man (2002) (PS2)]
                else if (strcmp(cfg->token_str, "MONO") == 0) { *out_val1 = 9; } // <- set to 8 in [Spider-Man (2002) (PS2)]
                else if (strcmp(cfg->token_str, "SPU") == 0) { *out_val1 = 12; } // <- set to 10 in [Spider-Man (2002) (PS2)]
                else if (strcmp(cfg->token_str, "COLLECTION") == 0) { *out_val1 = 14; } // <- set to 12 in [Spider-Man (2002) (PS2)]
                else if (strcmp(cfg->token_str, "CD") == 0) { *out_val1 = 13; } // <- set to 11 in [Spider-Man (2002) (PS2)]
                else {
                    tmp3_bool = false;
                    while (true)
                    {
                        /* identify if already-obtained token is based on existing tokens, this time sound sources. */
                        tmp3_is_zero = strcmp(cfg->token_str, treyarch_ps2_stream_sources[tmp3_i1]);
                        if (tmp3_is_zero == 0)
                        {
                            /* sound source identified. */
                            *out_val1 = 17; // <- set to 15 in [Spider-Man (2002) (PS2)]
                            *out_val2 = tmp3_i1;
                            tmp3_bool = true;
                            break;
                        }
                        tmp3_i1++;
                        if (tmp3_i1 >= 8) break;
                    }
                    if (!tmp3_bool) {
                        /* token is alone as a string. */
                        *out_val1 = 16; // <- set to 14 in [Spider-Man (2002) (PS2)]
                    }
                }
            }
            else {
                /* token is alone as a number. */
                *out_val1 = 15; // <- set to 13 in [Spider-Man (2002) (PS2)]
                tmp4 = strtol(cfg->token_str, '\0', 10);
                *out_val2 = tmp4;
            }
            /* get token info from here. */
            cfg->token_type = *out_val1;
            cfg->token_num = *out_val2;
            ret_val = *out_val1;
        }
    }
    else {
        /* if token already exists, just copy info from token and reset everything. */
        *out_val1 = cfg->token_type;
        *out_val2 = cfg->token_num;
        out_val3 = cfg->token_str;
        cfg->got_token = false;
        ret_val = *out_val1;
    }

    return ret_val;
fail:
    return 0;
}

static void clear_comm_key(line_go_through* cfg) {
    cfg->got_token = true;
}

typedef struct {
    int bin_pos;
    int bkup_pos;
} sdx_bin_go_through;

static int parse_sdx_binary_struct(STREAMFILE* sf, treyarch_ps2snd* ps2snd, sdx_bin_go_through* sbgt_);

static int parse_sdx(treyarch_ps2snd* ps2snd) {
    int i;
    uint32_t sdx_test1, sdx_test2, sdx_test3;
    bool valid_read = false;

    /* additional checks, we need to know if this sdx is the real deal. */
    sdx_test1 = read_u32be(0, ps2snd->sf_sdx); /* sdx format starts with a literal string weighing 0x30 bytes. */
    if ((sdx_test1 & 0x80808080) == 0) {
        if (sdx_test1 & 0x40404040) {
            sdx_test2 = read_u32le(0x30, ps2snd->sf_sdx); /* then starts with a literal zero even tho it's not a requirement. */
            if (sdx_test2 == 0) {
                sdx_test3 = read_u32le(0x34, ps2snd->sf_sdx); /* then starts with a fairly small non-zero number even tho it's also not required. */
                if (sdx_test3 < 0x10000) { valid_read = true; }
            }
        }
    }

    if (valid_read == false) {
        goto fail;
    }
    else {
        if (ps2snd->sdx_start_offset_set == false) {
            ps2snd->sdx_start_offset = 0;
            ps2snd->sdx_size = get_streamfile_size(ps2snd->sf_sdx);
            ps2snd->sdx_block_info_size = 0x58;
            ps2snd->sdx_start_offset_set = true;
        }

        for (i = 0; i < 2; i++)
        {
            sdx_bin_go_through sbgt_ = { 0 };
            switch (i)
            {
            case 0:
                ps2snd->calc_total_subsongs_first = true;
                break;
            case 1:
                ps2snd->calc_total_subsongs_first = false;
                break;
            default:
                break;
            }

            while (true) {
                if (!parse_sdx_binary_struct(ps2snd->sf_sdx, ps2snd, &sbgt_)) break;
                if (ps2snd->calc_total_subsongs_first == false) {
                    if (ps2snd->subsong_set == true) { break; }
                }
            }
        }
    }

    return 1;
fail:
    return 0;
}

static int parse_sdx_binary_struct(STREAMFILE* sf, treyarch_ps2snd* ps2snd, sdx_bin_go_through* sbgt_) {
    bool nothing_left_to_read = false;
    char temp_val5[64];
    char temp_val6[32];
    bool spu_vbc_success = false;
    bool stream_vbc_success = false;

    if (sbgt_->bin_pos >= ps2snd->sdx_size) { nothing_left_to_read = true; }
    if (nothing_left_to_read == true) { goto fail; }

    if (ps2snd->subsong_set_to_zero == false) {
        if (ps2snd->target_subsong == 0) ps2snd->target_subsong = 1;
        ps2snd->total_subsongs = 0;
        ps2snd->expected_subsong = 1;
        ps2snd->subsong_set_to_zero = true;
    }

    if (ps2snd->calc_total_subsongs_first == false) {
        if (ps2snd->target_subsong < 0 || ps2snd->target_subsong > ps2snd->total_subsongs || ps2snd->total_subsongs < 1) goto fail;
    }

    sbgt_->bkup_pos = sbgt_->bin_pos;
    read_string(ps2snd->stream_name, 0x30, sbgt_->bin_pos + 0, sf);
    ps2snd->offset = read_s32le(sbgt_->bin_pos + 0x30, sf);
    ps2snd->realsize = read_s32le(sbgt_->bin_pos + 0x34, sf);
    ps2snd->size = read_s32le(sbgt_->bin_pos + 0x38, sf);
    ps2snd->pitch = read_u16le(sbgt_->bin_pos + 0x3c, sf);
    ps2snd->volume_l = read_u16le(sbgt_->bin_pos + 0x3e, sf);
    ps2snd->volume_r = read_u16le(sbgt_->bin_pos + 0x40, sf);
    ps2snd->flags1 = read_u16le(sbgt_->bin_pos + 0x48, sf);
    ps2snd->flags2 = read_u16le(sbgt_->bin_pos + 0x4c, sf);
    ps2snd->flags3 = read_u16le(sbgt_->bin_pos + 0x50, sf);
    sbgt_->bin_pos += ps2snd->sdx_block_info_size;

    if ((ps2snd->flags3 & 0x10) == 0) {
        // recycled spu vbc loading code from parse_line_comms func.
        while (!ps2snd->sf_vbc)
        {
            // open vbc from the spu folder.
            snprintf(temp_val5, sizeof(temp_val5), "spu\\%s.vbc", ps2snd->basename);
            if (spu_vbc_success == false) ps2snd->sf_vbc = open_streamfile_by_filename(sf, temp_val5);
            if (ps2snd->sf_vbc && spu_vbc_success == false) spu_vbc_success = true;

            // open vbc from the same folder as the sdx itself (unlikely).
            snprintf(temp_val6, sizeof(temp_val6), "%s.vbc", ps2snd->basename);
            if (spu_vbc_success == false) ps2snd->sf_vbc = open_streamfile_by_filename(sf, temp_val6);
            if (ps2snd->sf_vbc && spu_vbc_success == false) spu_vbc_success = true;

            // open vbc through an txtm file.
            // vbc file shares the same basename as sdx file does shouldn't be too hard to find the former file.
            // (TRAINING.VBC has TRAINING.SDX also, for one.)
            // (todo) have vgmstream inform the end-user on how to make a txtm file.
            if (spu_vbc_success == false) ps2snd->sf_vbc = read_filemap_file(sf, 0);
            if (ps2snd->sf_vbc && spu_vbc_success == false) spu_vbc_success = true;

            /*
            // (todo) proper handling for empty vbc streamfile object.
            if (spu_vbc_success == false) {
                // tried everything, no dice.
                // (todo) fill in the silence in case of an unsuccessful load.
                if (!ps2snd->sf_vbc) continue;
            }
            else {
                // AFAIK when spu vbc is loaded nothing is done to the flags var itself. (info needs double-checking)
                continue;
            }
            */
        }
    } else if ((ps2snd->flags3 & 0x10) == 0x10) {
        // recycled STREAM.VBC loading code from parse_line_comms func.
        while (!ps2snd->sf_stream_vbc)
        {
            // open STREAM.VBC from two folders back and into the stream folder.
            if (stream_vbc_success == false) ps2snd->sf_stream_vbc = open_streamfile_by_filename(sf, "..\\..\\stream\\stream.vbc");
            if (ps2snd->sf_stream_vbc && stream_vbc_success == false) stream_vbc_success = true;

            // open STREAM.VBC from the stream folder if said folder goes alongside snd and/or vbc files (unlikely).
            if (stream_vbc_success == false) ps2snd->sf_stream_vbc = open_streamfile_by_filename(sf, "stream\\stream.vbc");
            if (ps2snd->sf_stream_vbc && stream_vbc_success == false) stream_vbc_success = true;

            // open STREAM.VBC from the same folder as where the snd and/or vbc files are (unlikely).
            if (stream_vbc_success == false) ps2snd->sf_stream_vbc = open_streamfile_by_filename(sf, "stream.vbc");
            if (ps2snd->sf_stream_vbc && stream_vbc_success == false) stream_vbc_success = true;

            // open STREAM.VBC through an txtm file.
            // (todo) have vgmstream inform the end-user on how to make a txtm file.
            if (stream_vbc_success == false) ps2snd->sf_stream_vbc = read_filemap_file(sf, 1);
            if (ps2snd->sf_stream_vbc && stream_vbc_success == false) stream_vbc_success = true;

            /*
            // (todo) proper handling for empty vbc streamfile object.
            if (stream_vbc_success == false) {
                // tried everything, no dice.
                // (todo) fill in the silence in case of an unsuccessful load.
                if (!ps2snd->sf_stream_vbc) continue;
            }
            else {
                // put in a flag denoting STREAM.VBC being loaded.
                ps2snd->flags |= 4;
                continue;
            }
            */
        }
    }

    if (ps2snd->calc_total_subsongs_first == true) { ps2snd->total_subsongs++; }
    else {
        if (ps2snd->expected_subsong == ps2snd->target_subsong) { ps2snd->subsong_set = true; }
        ps2snd->expected_subsong++;
        if (ps2snd->target_subsong < 0 || ps2snd->target_subsong > ps2snd->total_subsongs || ps2snd->total_subsongs < 1) goto fail;
    }

    return 1;
fail:
    return 0;
}
