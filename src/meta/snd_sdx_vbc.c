#include "meta.h"
#include "../coding/coding.h"
#include "../util/text_reader.h"
#include "../util/companion_files.h"

typedef enum {
    NONE = 0, REALSIZE = 1, SIZE = 2, OFFSET = 3, FREQ = 4, LOOP = 10,
    REVERB = 11, VOLUME_L = 5, VOLUME_R = 6, VOLUME = 7, STEREO = 8, MONO = 9,
    SPU = 12, COLLECTION = 14, CD = 13, SOUND_SOURCE = 17, STRING = 16, INT = 15,
    SOUND_LANGUAGE = 18
} treyarch_ps2snd_cmds;

/*
 * this is how the above enums would be set in gas.irx code, from [Spider-Man (2002) (PS2)]:
 * - SIZE = 1, OFFSET = 2, FREQ = 3, LOOP = 9,
 * - VOLL = 4, VOLR = 5, VOLUME = 6, STEREO = 7, MONO = 8,
 * - SPU = 10, COLLECTION = 12, CD = 11,
 * - SOUND_SOURCE = 15, STRING = 14, INT = 13
 * 
 * REALSIZE (set to 1) and REVERB (set to 11) were added to gas.irx/gasB.irx code in the following games:
 * - [Kelly Slater's Pro Surfer (PS2)] - uses gas.irx
 * - [NHL 2K3 (PS2)] - uses gasB.irx
 * consequently, the rest of the enum values had to be changed to accomodate these new additions.
 * 
 * [Minority Report: Everybody Runs (PS2)] uses gas.irx but do not use command enums at all.
 * 
 * SOUND_LANGUAGE is new to and used in this code (and only here) for practical reasons 
 * and otherwise does not exist in any Treyarch PS2 game whatsoever.
 */

typedef struct
{
    treyarch_ps2snd_cmds cmds;
    char stream_name[48]; /* sound_name */
    int32_t id; /* sound_id */
    int32_t realsize; /* real_sound_data_size */
    // ^ didn't exist in Spider-Man PS2 snd format.
    int32_t size; /* sound_data_size */
    int32_t offset; /* sound_offset */
    int32_t freq; /* sample_rate */
    // ^ present in snd format, but sdx format just ditched it.
    uint16_t pitch; /* sound_sample_pitch */
    // ^ in snd format, pitch was a result of freq being right-shifted to 12, then divided by 48000.
    // sdx format already has pitch value built-in.
    uint16_t volume_l; /* sound_left_volume */
    // ^ in snd format, volume_l was a result of volume value being multiplied by 16383, then divided by 100.
    // sdx format already has volume_l value built-in.
    uint16_t volume_r; /* sound_right_volume */
    // ^ in snd format, volume_r was a result of volume value being multiplied by 16383, then divided by 100.
    // sdx format already has volume_r value built-in.
    int32_t source; /* sound_source */
    // ^ set to one of the following numbers:
    // 0 - SFX, 1 - AMBIENCE, 2 - MUSIC, 3 - VOICE, 4 - MOVIE, 5 - USER1, 6 - USER2
    uint32_t flags; /* sound_flags */
    // sdx format introduced two vars, here they are for reference sake.
    uint32_t unk0x48;
    // ^ alternates between 0x5000 and a literal zero, with no in-between.
    int32_t lang; /* sound_language */
    /*
    uint32_t unk0x54;
    // ^ *always* zero.
    */
} treyarch_ps2snd_entry;

typedef struct {
    char basename[10];
    STREAMFILE* sf_header;
    char vbc_name[48];
    // ^ absolute file path of an spu vbc file, can be found in the "COLLECTION" text line from snd.
    // said path starts with the "ps2sound" folder.
    STREAMFILE* sf_vbc;

    bool subsong_set;
    bool sound_has_defined_name_or_id;
    bool sound_has_name_rather_than_id;
    bool enable_collection;

    int32_t start_offset;

    int total_subsongs; /* must be counted either per text line or per binary struct. */
    int expected_subsong;
    int target_subsong;

    char* line;
    treyarch_ps2snd_entry entry;
    treyarch_ps2snd_entry selected_entry;

    size_t header_size;
    int8_t sdx_block_info_size;
} treyarch_ps2snd;

static int parse_treyarch_ps2_snd(treyarch_ps2snd* ps2snd);
static int parse_sdx(treyarch_ps2snd* ps2snd);

/* SND+VBC - from Treyarch games [Spider-Man (2002) (PS2), Kelly Slater's Pro Surfer (PS2), NHL 2K3 (PS2)] */
VGMSTREAM* init_vgmstream_snd_vbc(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_body = NULL;
    treyarch_ps2snd ps2snd = { 0 };
    int channels, loop_flag;

    /* checks */
    if (!check_extensions(sf, "snd"))
        goto fail;

    /* check file size too. */
    if (get_streamfile_size(sf) > 0x17000)
        goto fail;

    /* snd files always start with an semicolon(;), meant to denote a "comment".
     * but GAS.IRX has code for skipping this symbol while reading such files
     * so let's check some numbers and alphabet letters also. */
    if (read_s8(0, sf) != 0x3b &&
        ((read_s8(0, sf) >= '0') && (read_s8(0, sf) >= '9')) &&
        ((read_s8(0, sf) >= 'A') && (read_s8(0, sf) >= 'Z')) &&
        ((read_s8(0, sf) >= 'a') && (read_s8(0, sf) >= 'z')))
        goto fail;

    /* get basename of snd file */
    get_streamfile_basename(sf, ps2snd.basename, sizeof(ps2snd.basename));

    /* snd format - auto-generated text file from Treyarch PS2 games released in 2002. */
    /* no "number of entries" number available so must be obtained per-line. */

    ps2snd.sf_header = sf;
    ps2snd.subsong_set = false;
    ps2snd.target_subsong = sf->stream_index;

    if (!parse_treyarch_ps2_snd(&ps2snd)) goto fail;

    channels = (ps2snd.selected_entry.flags & 0x01) ? 2 : 1;
    loop_flag = (ps2snd.selected_entry.flags & 0x02) ? 1 : 0;
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SND_VBC;
    vgmstream->sample_rate = ps2snd.selected_entry.freq;
    vgmstream->num_streams = ps2snd.total_subsongs;
    vgmstream->stream_size = ps2snd.selected_entry.size;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x800;
    vgmstream->num_samples = ps_bytes_to_samples(ps2snd.selected_entry.size, channels);
    vgmstream->loop_start_sample = 0;
    vgmstream->loop_end_sample = vgmstream->num_samples;

    sf_body = ps2snd.sf_vbc;
    if (!sf_body) goto fail;

    if (ps2snd.sound_has_defined_name_or_id)
    {
        if (!ps2snd.sound_has_name_rather_than_id)
        {
            snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%08x", ps2snd.selected_entry.id);
        }
        else {
            strcpy(vgmstream->stream_name, ps2snd.selected_entry.stream_name);
        }
    }

    if (!vgmstream_open_stream(vgmstream, sf_body, ps2snd.selected_entry.offset))
        goto fail;

    close_streamfile(ps2snd.sf_vbc);
    return vgmstream;
fail:
    close_streamfile(ps2snd.sf_vbc);
    close_vgmstream(vgmstream);
    return NULL;
}

/* SDX+VBC - used in only one Treyarch PS2 game and it's this -> [Minority Report: Everybody Runs (PS2)] */
VGMSTREAM* init_vgmstream_sdx_vbc(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_body = NULL;
    treyarch_ps2snd ps2snd = { 0 };
    int channels, loop_flag;

    /* checks */
    if (!check_extensions(sf, "sdx")) return NULL;

    ps2snd.sf_header = sf;
    ps2snd.subsong_set = false;
    ps2snd.target_subsong = sf->stream_index;
    ps2snd.sdx_block_info_size = 0x58;

    /* get basename of sdx file */
    get_streamfile_basename(sf, ps2snd.basename, sizeof(ps2snd.basename));

    /* sdx file - basically xsh+xsd/xss format from Treyarch Xbox games if first three fields (version number, zero, number of sounds) never existed. */

    if (!parse_sdx(&ps2snd))
        goto fail;

    channels = (ps2snd.selected_entry.flags & 0x01) ? 2 : 1;
    loop_flag = (ps2snd.selected_entry.flags & 0x02) ? 1 : 0;
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SDX_VBC;
    vgmstream->sample_rate = round10((48000 * ps2snd.selected_entry.pitch) / 4096); /* alternatively, (ps2snd.pitch * 48000) << 12 */
    vgmstream->num_streams = ps2snd.total_subsongs;
    vgmstream->stream_size = ps2snd.selected_entry.size;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x800;
    vgmstream->num_samples = ps_bytes_to_samples(ps2snd.selected_entry.size, channels);
    vgmstream->loop_start_sample = 0;
    vgmstream->loop_end_sample = vgmstream->num_samples;

    sf_body = ps2snd.sf_vbc;
    if (!sf_body) goto fail;

    strcpy(vgmstream->stream_name, ps2snd.selected_entry.stream_name);

    if (!vgmstream_open_stream(vgmstream, sf_body, ps2snd.selected_entry.offset))
        goto fail;

    close_streamfile(ps2snd.sf_vbc);
    return vgmstream;
fail:
    close_streamfile(ps2snd.sf_vbc);
    close_vgmstream(vgmstream);
    return NULL;
}

typedef struct {
    int line_pos;
    int line_size;

    bool got_token;
    char token_str[48];
    treyarch_ps2snd_cmds token_type;
    int32_t token_num;
} line_reader_state;

static int parse_line_commands(STREAMFILE* sf, treyarch_ps2snd* ps2snd);
static int parse_only_the_collection_command(STREAMFILE* sf, treyarch_ps2snd* ps2snd);
static treyarch_ps2snd_cmds get_string_cmd_key(line_reader_state* cfg, const char* val, treyarch_ps2snd_cmds* out_val1, int32_t* out_val2, char* out_val3);
static void clear_cmd_key(line_reader_state* cfg);
static void load_collection_file(treyarch_ps2snd* ps2snd);

// commonly used across snd text files.
static const char* const treyarch_ps2_sound_sources[8] = { "SFX", "AMBIENT", "MUSIC", "VOICE", "MOVIE", "USER1", "USER2", "N/A" };
// only seen on stream.snd, otherwise unused pretty much everywhere else from Treyarch PS2 games that have "snd" files.
// gas.irx/gasB.irx doesn't even have code for handling this, it's literally just a leftover array.
static const char* const treyarch_ps2_sound_languages[13] = { "NONE", "ENGLISH", "FRENCH", "GERMAN", "SPANISH", "JAPANESE", "CANTONESE", "MANDARIN", "GGENG", "GGFRE", "GGGER", "GGSPA", "N/A" };
// on a general note, "N\A" is spotted at least three times in gas.irx/gasB.irx code, except Minority Report which doesn't even *have* "N\A"

static int parse_treyarch_ps2_snd(treyarch_ps2snd* ps2snd) {
    /* set offset to zero and get size of whole snd. */
    ps2snd->start_offset = 0;
    ps2snd->header_size = get_streamfile_size(ps2snd->sf_header);

    /* set up some text reader stuff. */
    text_reader_t tr;
    uint8_t buf[255 + 1];
    int line_len;
    if (!text_reader_init(&tr, buf, sizeof(buf), ps2snd->sf_header, ps2snd->start_offset, 0))
        goto fail;

    /* set up some subsong info. */
    ps2snd->total_subsongs = 0;
    if (ps2snd->target_subsong == 0) ps2snd->target_subsong = 1;
    ps2snd->expected_subsong = 1;

    // read whole file while parsing only the "COLLECTION" line.
    // said line arrives dead *last* in an snd text file; mulitple "sound entry" lines are all stored above it.
    do
    {
        line_len = text_reader_get_line(&tr, &ps2snd->line);

        if (line_len < 0)
            goto fail;
        if (ps2snd->line == NULL)
            break;
        if (line_len == 0)
            continue;

        if (!parse_only_the_collection_command(ps2snd->sf_header, ps2snd))
            break;

    } while (line_len >= 0);

    /* we got what we needed so reset text reader. */
    if (!text_reader_init(&tr, buf, sizeof(buf), ps2snd->sf_header, ps2snd->start_offset, 0))
        goto fail;

    /* now read whole file while parsing literally everything else. */
    do
    {
        line_len = text_reader_get_line(&tr, &ps2snd->line);

        if (line_len < 0)
            goto fail;
        if (ps2snd->line == NULL)
            break;
        if (line_len == 0)
            continue;

        if (!parse_line_commands(ps2snd->sf_header, ps2snd))
            goto fail;

        if (ps2snd->subsong_set)
            ps2snd->selected_entry = ps2snd->entry;

    } while (line_len >= 0);

    return 1;
fail:
    return 0; 
}

static int parse_line_commands(STREAMFILE* sf, treyarch_ps2snd* ps2snd) {
    line_reader_state state = { 0 };
    int32_t int_value = 0;
    //char char_array[128];
    char char_array_small[48];
    char vbc_main_load_name[48];
    ps2snd->sound_has_defined_name_or_id = true; /* line always starts with a name or a number. */
    ps2snd->subsong_set = false;
    int backup = 0;

    if (ps2snd->line[0] == 0x3b /* ';' */)
        goto ignore;

    /* get line_size, check if line_pos is beyond line_size, and if it is, just fail. */
    state.line_size = strlen(ps2snd->line);
    if (state.line_pos >= state.line_size)
        goto fail;

    /* Treyarch PS2 games handled snd files by way of binary serialization, into memory.
     * They were literally just text files consisting of sound info being spelled out in plain-text, per line.
     *
     * such a line would start with either:
     * - the name of the sound supposedly being played (ex: SM_LANDING, CITY_MAIN).
     * - or the assigned number ID of said sound (ex: 119, 112).
     *
     * following that were various variables put in place, all of which were needed to play a particular sound in-game.
     * these could range from "realsize", "offset", "freq", "volume", just to name a few.
     *
     * said variables would take on this format: "VARIABLE VALUE".
     * - where "VARIABLE" is the assigned name, and "VALUE" is the assigned value that represents a variable.
     * - ex: "volume 100" <- value "100" is set to variable "volume".
     *
     * next up would be actual commands,
     * command format is just "COMMAND", a single word that would define how to actually play a sound.
     * - "COMMAND" is just the assigned name of how exactly:
     * -- is this sound supposed to be played from, how is it going to be played, how is it associated with, and where.
     * - ex1: "loop" represents a looping sound.
     * - ex2: "cd" represents a sound that needs to be played on a raw bigfile from the disk (STREAM.VBC).
     * - ex3: "USER1" represents a supposed sound source where sound itself is associated with.
     * - and so on and so forth.
     *
     * anyway, a line that would contain such vars and cmds would have to be serialized into a binary struct,
     * with vars made to actual vars in a struct with extra processing depending on the var, and cmds stored to bit-flags in a specialized "flags" var.
     * sound names and/or sound IDs (sound info could only start with one of the two) would be uploaded into such struct this way too,
     * with "sound name" being stored into a multi-array char var, and "sound ID" stored into a 32-bit int. */

     /* string token stuff taken from gas.irx and gasB.irx, respectively. */

    bool defined_sound_name_or_id = true;
    treyarch_ps2snd_cmds command = NONE;
    treyarch_ps2snd_cmds alt_command = NONE;
    while (ps2snd->entry.cmds = get_string_cmd_key(&state, ps2snd->line, &command, &int_value, &char_array_small), ps2snd->entry.cmds != 0)
    {
        if (defined_sound_name_or_id)
        {
            alt_command = ps2snd->entry.cmds - INT;
            if (alt_command >= 0 && alt_command <= 1)
            {
                switch (alt_command)
                {
                case 0: // INT
                    if (ps2snd->sound_has_defined_name_or_id)
                    {
                        ps2snd->total_subsongs++;
                        ps2snd->entry.id = (int32_t)(int_value);
                        ps2snd->sound_has_name_rather_than_id = false;
                    }
                    break;
                case 1: // STRING
                    if (ps2snd->sound_has_defined_name_or_id)
                    {
                        ps2snd->total_subsongs++;
                        strcpy(ps2snd->entry.stream_name, char_array_small);
                        ps2snd->sound_has_name_rather_than_id = true;
                    }
                    break;
                }

                /* set them flags. */
                ps2snd->entry.flags &= 0xfffffff4;
                ps2snd->entry.flags &= 0xfffffffd;
                ps2snd->entry.flags &= 0xfffffffe;
                ps2snd->entry.flags |= 0x10;
                //volume_result = percentile_to_volume(100);
                //ps2snd->entry.volume_l = volume_result;
                //ps2snd->entry.volume_r = volume_result;
                //ps2snd->entry.realsize = 0;
            }
        }

        /* serialize all info from a text line, even if we don't need much of it.*/
        switch (ps2snd->entry.cmds)
        {
        case NONE:
            break;
        case COLLECTION: /* COLLECTION file definition; has absolute file path of the vbc file. */
            if (get_string_cmd_key(&state, ps2snd->line, &command, &int_value, &char_array_small) == STRING)
            {
                if (!ps2snd->enable_collection)
                {
                    // technically speaking, it could be possible that "COLLECTION" line was placed as the actual first line,
                    // but instead it ends up dead last in an snd text file.
                    strcpy(ps2snd->vbc_name, char_array_small);
                    ps2snd->enable_collection = true;
                }
            }
            break;
        case REALSIZE: /* file size of a sound entry minus the padding at the end of said sound. */
            if (get_string_cmd_key(&state, ps2snd->line, &command, &int_value, &char_array_small) == INT) {
                ps2snd->entry.realsize = int_value;
            }
            else {
                clear_cmd_key(&state);
            }
            break;
        case SIZE: /* whole file size of a sound entry. */
            if (get_string_cmd_key(&state, ps2snd->line, &command, &int_value, &char_array_small) == INT) {
                ps2snd->entry.size = int_value;
            }
            else {
                clear_cmd_key(&state);
            }
            break;
        case OFFSET: /* file offset, denotes where sound is in a given vbc. */
            if (get_string_cmd_key(&state, ps2snd->line, &command, &int_value, &char_array_small) == INT) {
                ps2snd->entry.offset = int_value;
            }
            else {
                clear_cmd_key(&state);
            }
            break;
        case FREQ: /* sample rate, made into PS2 sound pitch format also. */
            if (get_string_cmd_key(&state, ps2snd->line, &command, &int_value, &char_array_small) == INT) {
                ps2snd->entry.freq = int_value;
                ps2snd->entry.pitch = ((int_value << 12) / 48000) & 0xffff;
            }
            else {
                clear_cmd_key(&state);
            }
            break;
        case VOLUME_L: /* volume for left-channel sound. */
            if (get_string_cmd_key(&state, ps2snd->line, &command, &int_value, &char_array_small) == INT) {
                ps2snd->entry.volume_l = ((int_value * 16383) / 100) & 0xffff;
            }
            else {
                clear_cmd_key(&state);
            }
            break;
        case VOLUME_R: /* volume for right-channel sound. */
            if (get_string_cmd_key(&state, ps2snd->line, &command, &int_value, &char_array_small) == INT) {
                ps2snd->entry.volume_r = ((int_value * 16383) / 100) & 0xffff;
            }
            else {
                clear_cmd_key(&state);
            }
            break;
        case VOLUME: /* volume for stereo sound. */
            if (get_string_cmd_key(&state, ps2snd->line, &command, &int_value, &char_array_small) == INT) {
                ps2snd->entry.volume_l = ((int_value * 16383) / 100) & 0xffff;
                ps2snd->entry.volume_r = ((int_value * 16383) / 100) & 0xffff;
            }
            else {
                clear_cmd_key(&state);
            }
            break;
        case STEREO: /* stereo flag */
            ps2snd->entry.flags |= 1;
            break;
        case MONO: /* mono flag */
            ps2snd->entry.flags &= 0xfffffffe;
            break;
        case LOOP: /* loop flag */
            ps2snd->entry.flags |= 2;
            break;
        case REVERB: /* reverb flag */
            ps2snd->entry.flags |= 8;
            break;
        case SPU: /* spu vbc file flag */
            ps2snd->entry.flags &= 0xffffffef;
            break;
        case CD: /* STREAM.VBC flag */
            ps2snd->entry.flags |= 0x10;
            break;
        case INT: /* int number. */
            if (!defined_sound_name_or_id)
                printf("Error, what's this number (%d) doing here?\n", int_value);
            if (defined_sound_name_or_id)
                defined_sound_name_or_id = false;
            break;
        case STRING: /* string. */
            if (!defined_sound_name_or_id)
                printf("Error, what's this string ('%s') doing here?\n", char_array_small);
            if (defined_sound_name_or_id)
                defined_sound_name_or_id = false;
            break;
        case SOUND_SOURCE: /* assigned sound source. */
            ps2snd->entry.source = int_value;
            break;
        case SOUND_LANGUAGE:
            ps2snd->entry.lang = int_value;
            break;
        default:
            break;
        }
    }

    ps2snd->entry.flags &= 0xfffffffb;
    if ((ps2snd->entry.flags >> 4 & 1) == 0)
        ps2snd->entry.unk0x48 = 0x5000;

    if (ps2snd->target_subsong < 0 || (ps2snd->target_subsong > ps2snd->total_subsongs && ps2snd->target_subsong > 1000) || ps2snd->total_subsongs < 1) goto fail;
    /* in this case, there are now a total of four outcomes (two of which can happen either way)
     * that are all valid for failing to fetch the sound info from a text line:
     * - if target_subsong is below 0;
     * - if target_subsong goes beyond either total_subsongs OR 1000 (arbritary number, observed max is 960);
     * - if total_subsongs is below 1; */

    if (ps2snd->sound_has_defined_name_or_id) {
        if (ps2snd->expected_subsong == ps2snd->target_subsong)
            ps2snd->subsong_set = true;

        if (ps2snd->subsong_set)
        {
            bool vbc_load_success = false;

            for (int i = 0; i < 5; i++)
            {
                if (vbc_load_success) break;

                if ((ps2snd->entry.flags & 0x10) == 0)
                {
                    switch (i)
                    {
                    case 0:
                        // set vbc load name to be vbc from the spu folder.
                        snprintf(vbc_main_load_name, sizeof(vbc_main_load_name), "spu\\%s.vbc", ps2snd->basename);
                        break;
                    case 1:
                        // set vbc load name to be vbc from the same folder as the snd itself (unlikely).
                        snprintf(vbc_main_load_name, sizeof(vbc_main_load_name), "%s.vbc", ps2snd->basename);
                        break;
                    case 2:
                        // load specified vbc file (complete with a full path name consisting of about four folders and a file name)
                        // from the "COLLECTION" field.
                        load_collection_file(ps2snd);
                        break;
                    case 3:
                        // open vbc through an txtm file.
                        // vbc file shares the same basename as snd file does shouldn't be too hard to find the former file.
                        // (MENU.VBC has MENU.SND also, for one)
                        ps2snd->sf_vbc = read_filemap_file(sf, 0);
                        // (todo) untested.
                        break;
                    default:
                        break;
                    }

                    if (i >= 0 && i <= 1)
                    {
                        // finally load vbc from already-built load name.
                        ps2snd->sf_vbc = open_streamfile_by_filename(sf, vbc_main_load_name);
                    }
                }
                else if ((ps2snd->entry.flags & 0x10) == 0x10)
                {
                    switch (i)
                    {
                    case 0:
                        // open STREAM.VBC from two folders back and into the stream folder.
                        ps2snd->sf_vbc = open_streamfile_by_filename(sf, "..\\..\\stream\\stream.vbc");
                        break;
                    case 1:
                        // open STREAM.VBC from one folder back and into the stream folder.
                        ps2snd->sf_vbc = open_streamfile_by_filename(sf, "..\\stream\\stream.vbc");
                        break;
                    case 2:
                        // open STREAM.VBC from the stream folder if said folder goes alongside snd and/or vbc files (unlikely).
                        ps2snd->sf_vbc = open_streamfile_by_filename(sf, "stream\\stream.vbc");
                        break;
                    case 3:
                        // open STREAM.VBC from the same folder as where the snd and/or vbc files are (unlikely).
                        ps2snd->sf_vbc = open_streamfile_by_filename(sf, "stream.vbc");
                        break;
                    case 4:
                        // open STREAM.VBC through an txtm file.
                        ps2snd->sf_vbc = read_filemap_file(sf, 1);
                        // (todo) untested.
                        break;
                    default:
                        break;
                    }
                }

                if (ps2snd->sf_vbc && !vbc_load_success)
                    vbc_load_success = true;
            }
        }

        ps2snd->expected_subsong++;
    }

    return 1;
ignore:
    return 2;
fail:
    return 0;
}

// basically parse_line_commands except this time we only need to parse the "COLLECTION" line.
static int parse_only_the_collection_command(STREAMFILE* sf, treyarch_ps2snd* ps2snd)
{
    line_reader_state state = { 0 };

    treyarch_ps2snd_cmds command = NONE;
    treyarch_ps2snd_cmds is_string = NONE;
    int32_t int_value = 0;
    char char_array[48];
    ps2snd->sound_has_defined_name_or_id = true; /* line always starts with a name or a number. */
    ps2snd->subsong_set = false;

    if (ps2snd->line[0] == 0x3b /* ';' */)
        goto ignore;

    /* get line_size, check if line_pos is beyond line_size, and if it is, just fail. */
    state.line_size = strlen(ps2snd->line);
    if (state.line_pos >= state.line_size)
        goto fail;

    // now parse only the "COLLECTION" line.
    get_string_cmd_key(&state, ps2snd->line, &command, &int_value, &char_array);
    if ((command == COLLECTION) /* COLLECTION file definition; has absolute file path of the vbc file. */ &&
        (is_string = get_string_cmd_key(&state, ps2snd->line, &command, &int_value, &char_array),
            is_string == STRING))
    {
        if (!ps2snd->enable_collection)
        {
            // technically speaking, it could be possible that "COLLECTION" line was placed as the actual first line,
            // but instead it ends up dead last in an snd text file.
            strcpy(ps2snd->vbc_name, char_array);
            ps2snd->enable_collection = true;
        }
    }
    else {
        goto ignore;
    }

    return 1;
ignore:
    return 2;
fail:
    return 0;
}

static treyarch_ps2snd_cmds get_string_cmd_key(line_reader_state* cfg, const char* val, treyarch_ps2snd_cmds* out_val1, int32_t* out_val2, char* out_val3) {
    treyarch_ps2snd_cmds return_val;
    bool found_key_spot = false;
    int has_numbers = 0;
    int has_letters = 0;
    bool stream_name_is_not_all_numbers = false;
    int tmp_pos1 = 0;
    char val_chr = 0;
    int tmp_pos2 = 0;
    int32_t int_val;

    if (cfg->line_pos >= cfg->line_size) goto fail;
    if (cfg->line_pos < 0) goto fail;

    if (!cfg->got_token)
    {
        /* token isn't occupied, let's do business. */

        /* find an non-empty char so we can start work on identifying token. */
        while (true)
        {
            if (cfg->line_pos >= cfg->line_size) break;
            val_chr = val[cfg->line_pos];
            if (val_chr != ' ') {
                found_key_spot = true;
                break;
            }
            if (val_chr == '\0') break;
            cfg->line_pos++;
        }

        if (!found_key_spot)
        {
            /* do nothing if "key spot" is nowhere to be seen. */
            out_val3 = 0;
            return_val = NONE;
        }
        else {
            /* copy whole token to char array. */
            while (true)
            {
                if (tmp_pos1 >= cfg->line_size) break;
                val_chr = val[cfg->line_pos];
                if (val_chr == ' ') break;
                if (val_chr == '\0') break;
                out_val3[tmp_pos1] = val_chr;
                tmp_pos1++;
                cfg->line_pos++;
            }

            /* put a zero in there (string is already read in its entirety anyway). */
            out_val3[tmp_pos1] = '\0';
            tmp_pos1 = 0;

            /* convert token to uppercase (by way of bitmask checks against lowercase chars) into another char array. */
            while (true)
            {
                val_chr = out_val3[tmp_pos1];
                if (val_chr == '\0') break;
                if ((val_chr & 0x40) == 0x40) { if ((val_chr & 0x20) == 0x20) { val_chr ^= 0x20; } }
                cfg->token_str[tmp_pos2] = val_chr;
                tmp_pos1++;
                tmp_pos2++;
            }

            cfg->token_str[tmp_pos2] = '\0';
            tmp_pos2 = 0;

            tmp_pos1 = 0;

            /* now weigh in the options;
             * if current char has letters (A-Z range only), count them to a separate var.
             * or if current char has numbers (0-9 range only), count them to another var. */
            while (true)
            {
                val_chr = cfg->token_str[tmp_pos1];
                if (val_chr == '\0') break;
                if ((val_chr & 0x80) == 0) {
                    if ((val_chr & 0x40) == 0x40) { has_letters++; }
                    else if ((val_chr & 0x20) == 0x20) { if ((val_chr & 0x10) == 0x10) { has_numbers++; } }
                }
                tmp_pos1++;
            }

            *out_val1 = 0;

            /* then, actually weigh in on whether or not the token we got is nothing but numbers, letters, or a mix of the two. */
            if (has_letters == 0 && has_numbers != 0) { stream_name_is_not_all_numbers = false; }
            else if (has_letters != 0 && has_numbers == 0) { stream_name_is_not_all_numbers = true; }
            else if (has_letters != 0 && has_numbers != 0) { stream_name_is_not_all_numbers = true; }

            if (stream_name_is_not_all_numbers)
            {
                /* identify if already-obtained token is based on existing tokens an do some work with them. */
                if (strcmp(cfg->token_str, "SIZE") == 0) { *out_val1 = SIZE; }
                else if (strcmp(cfg->token_str, "REALSIZE") == 0) { *out_val1 = REALSIZE; }
                else if (strcmp(cfg->token_str, "OFFSET") == 0) { *out_val1 = OFFSET; }
                else if (strcmp(cfg->token_str, "FREQ") == 0) { *out_val1 = FREQ; }
                else if (strcmp(cfg->token_str, "LOOP") == 0) { *out_val1 = LOOP; }
                else if (strcmp(cfg->token_str, "REVERB") == 0) { *out_val1 = REVERB; }
                else if (strcmp(cfg->token_str, "VOLL") == 0) { *out_val1 = VOLUME_L; }
                else if (strcmp(cfg->token_str, "VOLR") == 0) { *out_val1 = VOLUME_R; }
                else if (strcmp(cfg->token_str, "VOLUME") == 0) { *out_val1 = VOLUME; }
                else if (strcmp(cfg->token_str, "STEREO") == 0) { *out_val1 = STEREO; }
                else if (strcmp(cfg->token_str, "MONO") == 0) { *out_val1 = MONO; }
                else if (strcmp(cfg->token_str, "SPU") == 0) { *out_val1 = SPU; }
                else if (strcmp(cfg->token_str, "COLLECTION") == 0) { *out_val1 = COLLECTION; }
                else if (strcmp(cfg->token_str, "CD") == 0) { *out_val1 = CD; }
                else {
                    int strcmp_return = 0;
                    int snd_lang_i = 0;
                    int snd_src_i = 0;
                    bool is_sound_lang = false;
                    bool is_sound_src = false;

                    while (strcmp_return = strcmp(treyarch_ps2_sound_languages[snd_lang_i], "N\\A"), strcmp_return != 0)
                    {
                        if (!strcmp(cfg->token_str, treyarch_ps2_sound_languages[snd_lang_i]))
                        {
                            // sound language identified.
                            *out_val1 = SOUND_LANGUAGE;
                            *out_val2 = (int32_t)(snd_lang_i);
                            is_sound_lang = true;
                            break;
                        }

                        snd_lang_i++;
                        if (snd_lang_i >= 13) break;
                    }

                    if (is_sound_lang) goto new_token;

                    while (strcmp_return = strcmp(treyarch_ps2_sound_sources[snd_src_i], "N\\A"), strcmp_return != 0)
                    {
                        if (!strcmp(cfg->token_str, treyarch_ps2_sound_sources[snd_src_i]))
                        {
                            // sound source identified.
                            *out_val1 = SOUND_SOURCE;
                            *out_val2 = (int32_t)(snd_src_i);
                            is_sound_src = true;
                            break;
                        }

                        snd_src_i++;
                        if (snd_src_i >= 8) break;
                    }

                    if (is_sound_src) goto new_token;

                    if (!is_sound_lang && !is_sound_src) {
                        /* token is alone as a string. */
                        *out_val1 = STRING;
                    }
                }
            }
            else {
                /* token is alone as a number. */
                *out_val1 = INT;
                int_val = strtol(cfg->token_str, '\0', 10);
                *out_val2 = int_val;
            }
            new_token:
            /* get token info from here. */
            cfg->token_type = *out_val1;
            cfg->token_num = *out_val2;
            return_val = *out_val1;
        }
    }
    else {
        /* if token already exists, just copy info from token and reset everything. */
        *out_val1 = cfg->token_type;
        *out_val2 = cfg->token_num;
        out_val3 = cfg->token_str;
        cfg->got_token = false;
        return_val = *out_val1;
    }

    return return_val;
fail:
    return 0;
}

static void clear_cmd_key(line_reader_state* cfg) {
    cfg->got_token = true;
}

// load specified vbc file from the "COLLECTION" line.
static void load_collection_file(treyarch_ps2snd* ps2snd) {
    char pathname[PATH_LIMIT];
    char vbc_path[48];

    if (ps2snd->enable_collection && !ps2snd->sf_vbc)
    {
        // count how many folders reside in an specified vbc name.
        // it's all absolute paths here.
        int vbc_name_pos = 0;
        int how_many_folders_from_vbc = 0;
        int vbc_pos_after_first_folder = 0;
        int vbc_last_folder_pos = 0;
        while (ps2snd->vbc_name[vbc_name_pos] != 0)
        {
            if (ps2snd->vbc_name[vbc_name_pos] == '\\' || ps2snd->vbc_name[vbc_name_pos] == '/')
            {
                how_many_folders_from_vbc++;
                vbc_last_folder_pos = vbc_name_pos + 1;
                if (how_many_folders_from_vbc == 1) vbc_pos_after_first_folder = vbc_last_folder_pos;
            }
            vbc_name_pos++;
        }

        // copy only the folder names, and while at it punch in a null delimiter after the fact.
        strncpy(vbc_path, ps2snd->vbc_name, vbc_last_folder_pos);
        vbc_path[vbc_last_folder_pos] = 0;

        // grab path name from an existing STREAMFILE.
        get_streamfile_path(ps2snd->sf_header, pathname, PATH_LIMIT);

        // and now, do some backwards "magic" to compare the newly-obtained path with the vbc path.
        int pathname_size = strlen(pathname);
        int vbc_path_size = vbc_last_folder_pos + 1;
        if (pathname_size >= vbc_path_size)
        {
            bool found_exact_match = false;
            int pathname_pos = pathname_size - 1;
            int pathname_pos_size = pathname_size - pathname_pos;
            int backup_pathname_pos = pathname_pos;
            vbc_name_pos = 0;
            int exact_match_count = 0;
            char char_comp1 = 0;
            char char_comp2 = 0;

            while (true)
            {
                if (pathname_pos_size < 0) break;

                if (pathname_pos_size < vbc_pos_after_first_folder)
                {
                    // make path name position "step back" if there's not enough space
                    // to compare chars from one path (path from STREAMFILE) to another (vbc path)
                    pathname_pos--;
                    pathname_pos_size++;
                }
                else {
                    // compare chars regardless of char sensitivity and folders.
                    for (int i = 0; i < vbc_pos_after_first_folder; i++)
                    {
                        char_comp1 = vbc_path[i + vbc_name_pos];
                        char_comp2 = pathname[i + pathname_pos];
                        if ((vbc_path[i + vbc_name_pos] != '\\' && vbc_path[i + vbc_name_pos] != '/')
                            ||
                            (pathname[i + pathname_pos] != '\\' && pathname[i + pathname_pos] != '/'))
                        {
                            if (((char_comp1 & 0x80) == 0) && ((char_comp1 & 0x40) == 0x40) && ((char_comp1 & 0x20) == 0x20)) char_comp1 ^= 0x20;
                            if (((char_comp2 & 0x80) == 0) && ((char_comp2 & 0x40) == 0x40) && ((char_comp2 & 0x20) == 0x20)) char_comp2 ^= 0x20;
                        }

                        if (char_comp1 == char_comp2)
                            exact_match_count++;
                    }

                    if (exact_match_count != vbc_pos_after_first_folder)
                    {
                        // numbers don't match? make path name position "take a step back" and try again.
                        exact_match_count = 0;
                        vbc_name_pos = 0;
                        backup_pathname_pos = pathname_pos;
                        pathname_pos--;
                    }
                    else if (exact_match_count == vbc_pos_after_first_folder) {
                        // we finally got a match!
                        found_exact_match = true;
                        backup_pathname_pos = pathname_pos;
                        break;
                    }
                }
            }

            // do some more magic *again* until we get to load the vbc.
            pathname_pos = backup_pathname_pos;
            if (found_exact_match)
            {
                // clean path name from last char to where the first vbc folder is (specified from "COLLECTION")
                while (pathname[pathname_pos] != 0)
                {
                    pathname[pathname_pos] = 0;
                    pathname_pos++;
                }

                // then put in the path name with the same specified vbc file from "COLLECTION".
                pathname_pos = backup_pathname_pos;
                vbc_name_pos = 0;
                while (ps2snd->vbc_name[vbc_name_pos] != 0)
                {
                    pathname[pathname_pos] = ps2snd->vbc_name[vbc_name_pos];
                    pathname_pos++;
                    vbc_name_pos++;
                }
                pathname[pathname_pos] = 0;

                // and finally, open the vbc.
                ps2snd->sf_vbc = open_streamfile(ps2snd->sf_header, pathname);
            }
        }
    }
}

typedef struct {
    int bin_pos;
} sdx_bin_reader_state;

static int parse_sdx_binary_struct(STREAMFILE* sf, treyarch_ps2snd* ps2snd, sdx_bin_reader_state* cfg);

static int parse_sdx(treyarch_ps2snd* ps2snd) {
    ps2snd->start_offset = 0;
    ps2snd->sdx_block_info_size = 0x58;
    ps2snd->header_size = get_streamfile_size(ps2snd->sf_header);

    /* additional checks, we need to know if this sdx is the real deal. */
    /* step 1 - do not allow sdx files weighing less than 88 bytes (block size of an entire binary struct). */
    if (ps2snd->header_size < ps2snd->sdx_block_info_size) goto fail;

    /* step 2 - if one value isn't zero and other value isn't "small enough"(*), might as well just fail. */
    if ((read_u32le(0x30, ps2snd->sf_header)) != 0 && (read_u32le(0x34, ps2snd->sf_header)) > 0x10000) goto fail;
    // (*) - sdx files have a complete listing of all the sounds as defined by that sdx.
    // said sounds are sorted by duration in descending order,
    // meaning the shortest possible sound (foley SFX, menu SFX, etc. lasting anything below a literal second) gets to be listed first
    // while the longest possible sound, be it banger music (lasting 4~5m *max*) or ambiance, is listed last.

    if (ps2snd->target_subsong == 0) ps2snd->target_subsong = 1;
    ps2snd->total_subsongs = 0;
    ps2snd->expected_subsong = 1;

    sdx_bin_reader_state state = { 0 };
    int return_state = 0;
    while (return_state = parse_sdx_binary_struct(ps2snd->sf_header, ps2snd, &state), return_state != 2) {
        if (return_state == 0) goto fail;

        if (ps2snd->subsong_set)
            ps2snd->selected_entry = ps2snd->entry;

    }

    return 1;
fail:
    return 0;
}

static int parse_sdx_binary_struct(STREAMFILE* sf, treyarch_ps2snd* ps2snd, sdx_bin_reader_state* cfg) {
    char vbc_main_load_name[48];
    ps2snd->subsong_set = false;

    if (cfg->bin_pos < 0) goto fail;
    if (cfg->bin_pos >= ps2snd->header_size) return 2;

    /*
     * Treyarch SDX format owes a lot to the SND format;
     * - most of the text file structure has now been replaced with a binary struct weighing 88 bytes apiece.
     * -- "apiece" in this case means "as many sounds as SDX permits".
     * - to account for this, SDX file now consists of multiple blocks of pre-serialized binary structs,
     *   with one block representing an entire sound and all its surrounding information.
     * -- "sound name" is first stored into a 48-byte char field, which is then followed by three 32-bit-wide fields representing certain values, and so on and so forth.
     * 
     * One upside to all this is that reading sound info is now much easier;
     * eliminates the need to parse "variables", "commands", names and numbers into a serialized binary struct from a single text line.
     */

    read_string(ps2snd->entry.stream_name, 0x30, cfg->bin_pos + 0, sf);
    ps2snd->entry.offset = read_s32le(cfg->bin_pos + 0x30, sf);
    ps2snd->entry.realsize = read_s32le(cfg->bin_pos + 0x34, sf);
    ps2snd->entry.size = read_s32le(cfg->bin_pos + 0x38, sf);
    ps2snd->entry.pitch = read_u16le(cfg->bin_pos + 0x3c, sf);
    ps2snd->entry.volume_l = read_u16le(cfg->bin_pos + 0x3e, sf);
    ps2snd->entry.volume_r = read_u16le(cfg->bin_pos + 0x40, sf);
    ps2snd->entry.unk0x48 = read_u32le(cfg->bin_pos + 0x48, sf);
    ps2snd->entry.source = read_s32le(cfg->bin_pos + 0x4c, sf);
    ps2snd->entry.flags = read_u32le(cfg->bin_pos + 0x50, sf);
    /*
    ps2snd->unk0x54 = read_u32le(cfg->bin_pos + 0x54, sf);
    */
    cfg->bin_pos += ps2snd->sdx_block_info_size;
    if (cfg->bin_pos <= ps2snd->header_size) ps2snd->total_subsongs++;

    if (ps2snd->target_subsong < 0 || (ps2snd->target_subsong > ps2snd->total_subsongs && ps2snd->target_subsong > 900) || ps2snd->total_subsongs < 1) goto fail;
    /* in this case, there are now a total of four outcomes (two of which can happen either way)
     * that are all valid for failing to fetch the sound info from a text line:
     * - if target_subsong is below 0;
     * - if target_subsong goes beyond either total_subsongs OR 900 (arbritary number, observed max is 809);
     * - if total_subsongs is below 1; */

    if (ps2snd->expected_subsong == ps2snd->target_subsong)
        ps2snd->subsong_set = true;

    if (ps2snd->subsong_set)
    {
        bool vbc_load_success = false;

        for (int i = 0; i < 5; i++)
        {
            if (vbc_load_success) break;

            if ((ps2snd->entry.flags & 0x10) == 0)
            {
                switch (i)
                {
                case 0:
                    // set vbc load name to be vbc from the spu folder.
                    snprintf(vbc_main_load_name, sizeof(vbc_main_load_name), "spu\\%s.vbc", ps2snd->basename);
                    break;
                case 1:
                    // set vbc load name to be vbc from the same folder as the snd itself (unlikely).
                    snprintf(vbc_main_load_name, sizeof(vbc_main_load_name), "%s.vbc", ps2snd->basename);
                    break;
                case 2:
                    // open vbc through an txtm file.
                    // vbc file shares the same basename as snd file does shouldn't be too hard to find the former file.
                    // (MENU.VBC has MENU.SND also, for one)
                    ps2snd->sf_vbc = read_filemap_file(sf, 0);
                    // (todo) untested.
                    break;
                default:
                    break;
                }
                if (i >= 0 && i <= 1)
                {
                    // finally load vbc from already-built load name.
                    ps2snd->sf_vbc = open_streamfile_by_filename(sf, vbc_main_load_name);
                }
            }
            else if ((ps2snd->entry.flags & 0x10) == 0x10)
            {
                switch (i)
                {
                case 0:
                    // open STREAM.VBC from two folders back and into the stream folder.
                    ps2snd->sf_vbc = open_streamfile_by_filename(sf, "..\\..\\stream\\stream.vbc");
                    break;
                case 1:
                    // open STREAM.VBC from one folder back and into the stream folder.
                    ps2snd->sf_vbc = open_streamfile_by_filename(sf, "..\\stream\\stream.vbc");
                    break;
                case 2:
                    // open STREAM.VBC from the stream folder if said folder goes alongside snd and/or vbc files (unlikely).
                    ps2snd->sf_vbc = open_streamfile_by_filename(sf, "stream\\stream.vbc");
                    break;
                case 3:
                    // open STREAM.VBC from the same folder as where the snd and/or vbc files are (unlikely).
                    ps2snd->sf_vbc = open_streamfile_by_filename(sf, "stream.vbc");
                    break;
                case 4:
                    // open STREAM.VBC through an txtm file.
                    ps2snd->sf_vbc = read_filemap_file(sf, 1);
                    // (todo) untested.
                    break;
                default:
                    break;
                }
            }

            if (ps2snd->sf_vbc && !vbc_load_success)
                vbc_load_success = true;
        }
    }

    ps2snd->expected_subsong++;

    return 1;
fail:
    return 0;
}
