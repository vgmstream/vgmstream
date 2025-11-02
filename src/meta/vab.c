#include "meta.h"
#include "../coding/coding.h"
#include "../util/spu_utils.h"


#define VAB_MIN(x,y) ((x)<(y)?(x):(y))
#define VAB_MAX(x,y) ((x)>(y)?(x):(y))
#define VAB_CLAMP(x,min,max) VAB_MIN(VAB_MAX(x,min),max)

static bool read_vabcfg_file(STREAMFILE* sf, int program, int tone, int* note, int* fine, int* uselimits) {
    char filename[PATH_LIMIT];
    off_t txt_offset, file_size;
    STREAMFILE* sf_cfg = NULL;
    size_t file_len, key_len;

    sf_cfg = open_streamfile_by_filename(sf, ".vab_config");
    if (!sf_cfg) return false;

    get_streamfile_filename(sf, filename, sizeof(filename));

    txt_offset = read_bom(sf_cfg);
    file_size = get_streamfile_size(sf_cfg);
    file_len = strlen(filename);

    /* read lines and find target filename, format is (filename): value1, ... valueN */
    while (txt_offset < file_size) {
        char line[0x2000];
        char key[PATH_LIMIT] = { 0 }, val[0x2000] = { 0 };
        int ok, bytes_read, line_ok;
        int cfg_program, cfg_tone, cfg_note, cfg_fine, cfg_limits;

        bytes_read = read_line(line, sizeof(line), txt_offset, sf_cfg, &line_ok);
        if (!line_ok) goto fail;

        txt_offset += bytes_read;

        /* get key/val (ignores lead/trailing spaces, stops at comment/separator) */
        ok = sscanf(line, " %[^\t#:] : %[^\t#\r\n] ", key, val);
        if (ok != 2) /* ignore line if no key=val (comment or garbage) */
            continue;

        if (key[0] == '*') {
            key_len = strlen(key);
            if (file_len < key_len)
                continue;

            if (strcmp(filename + (file_len - key_len + 1), key + 1) != 0)
                continue;
        } else {
            if (strcmp(filename, key) != 0)
                continue;
        }

        ok = sscanf(val, "%d , %d , %d , %d , %d", &cfg_program, &cfg_tone, &cfg_note, &cfg_fine, &cfg_limits);
        if (ok != 5)
            continue;

        if (cfg_program >= 0 && program != cfg_program)
            continue;

        if (cfg_tone >= 0 && tone != cfg_tone)
            continue;

        *note = cfg_note;
        *fine = cfg_fine;
        *uselimits = cfg_limits;

        close_streamfile(sf_cfg);
        return true;
    }

fail:
    close_streamfile(sf_cfg);
    return false;
}

/* .VAB - standard PS1 bank format */
VGMSTREAM* init_vgmstream_vab(STREAMFILE* sf) {
    uint16_t programs, wave_num, pitch;
    uint8_t center, shift, min_note, max_note;
    off_t programs_off, tones_off, waves_off, entry_off, data_offset;
    size_t data_size;
    bool is_vh = false;
    int target_subsong = sf->stream_index, program_num, tone_num, total_subsongs,
        note, fine, uselimits,
        channels, loop_flag, loop_start = 0, loop_end = 0;
    STREAMFILE* sf_data = NULL;
    VGMSTREAM* vgmstream = NULL;

    /* this format is intended for storing samples for sequenced music but
     * some games use it for storing SFX as a hack */

    /* checks */
    if (!is_id32le(0x00, sf, "VABp"))
        return NULL;

    if (check_extensions(sf, "vh")) {
        is_vh = true;
        sf_data = open_streamfile_by_ext(sf, "vb");
        if (!sf_data) return NULL;
    } else if (check_extensions(sf, "vab")) {
        is_vh = false;
        sf_data = sf;
    } else {
        return NULL;
    }

    programs = read_u16le(0x12, sf);
    //tones = read_u16le(0x14, sf);
    //waves = read_u16le(0x16, sf);

    programs_off = 0x20;
    tones_off = programs_off + 128 * 0x10;
    waves_off = tones_off + programs * 16 * 0x20;

    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0)
        goto fail;

    total_subsongs = 0;
    program_num = -1;
    tone_num = -1;
    for (int i = 0; i < programs; i++) {
        uint8_t program_tones;
        int local_target;

        local_target = target_subsong - total_subsongs - 1;
        entry_off = programs_off + i * 0x10;
        program_tones = read_u8(entry_off + 0x00, sf);
        total_subsongs += program_tones;

        if (local_target >= 0 && local_target < program_tones) {
            program_num = i;
            tone_num = local_target;
        }
    }

    if (program_num == -1)
        goto fail;

    entry_off = tones_off + program_num * 16 * 0x20 + tone_num * 0x20;
    center = read_u8(entry_off + 0x04, sf);
    shift = read_u8(entry_off + 0x05, sf);
    min_note = read_u8(entry_off + 0x06, sf); /* these two may contain garbage */
    max_note = read_u8(entry_off + 0x07, sf);
    wave_num = read_u16le(entry_off + 0x16, sf);

    if (read_vabcfg_file(sf, program_num, tone_num, &note, &fine, &uselimits)) {
        if (note == -1)
            note = center;
        if (fine == -1)
            fine = shift;
        if (uselimits)
            note = VAB_CLAMP(note, min_note, max_note);
    } else {
        /* play default note */
        note = 60;
        fine = 0;
    }

    pitch = spu1_note_to_pitch(note, fine, center, shift);

    data_offset = is_vh ? 0x00 : (waves_off + 256 * 0x02);
    for (int i = 0; i < wave_num; i++) {
        data_offset += read_u16le(waves_off + i * 0x02, sf) << 3;
    }

    data_size = read_u16le(waves_off + wave_num * 0x02, sf) << 3;

    if (data_size == 0 /*&& center == 0 && shift == 0*/) {
        // hack for empty sounds in rare cases (may set center/shift to 0 as well) [Critical Depth]
        vgmstream = init_vgmstream_silence(1, 44100, 44100);
        if (!vgmstream) goto fail;

        vgmstream->meta_type = meta_VAB;
        vgmstream->num_streams = total_subsongs;
        snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%02d/%02d (empty)", program_num, tone_num);

        if (is_vh) close_streamfile(sf_data);
        return vgmstream;
    }

    channels = 1;
    loop_flag = ps_find_loop_offsets(sf_data, data_offset, data_size, channels, 0, &loop_start, &loop_end);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_VAB;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_none;
    vgmstream->sample_rate = spu1_pitch_to_sample_rate(pitch); // FIXME: Maybe use actual pitching if implemented.
    vgmstream->num_samples = ps_bytes_to_samples(data_size, channels);
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;
    vgmstream->stream_size = data_size;
    vgmstream->num_streams = total_subsongs;
    snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%02d/%02d", program_num, tone_num);

    if (!vgmstream_open_stream(vgmstream, sf_data, data_offset))
        goto fail;

    if (is_vh) close_streamfile(sf_data);
    return vgmstream;

fail:
    if (is_vh) close_streamfile(sf_data);
    close_vgmstream(vgmstream);
    return NULL;
}
