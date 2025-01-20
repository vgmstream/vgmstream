#include "meta.h" 
#include "../coding/coding.h" 
#include "../layout/layout.h" 
 
static snd_gcw_str_blocked_layout_data* build_blocked_snd_gcw_str(int32_t num_samples, int channels, size_t block_size); 
 
/* SND+GCW/STR - from Treyarch games */ 
VGMSTREAM* init_vgmstream_snd_gcw_str(STREAMFILE* sf) { 
    VGMSTREAM* vgmstream = NULL; 
    STREAMFILE* sf_body = NULL; 
    int16_t major_version, minor_version; 
    int32_t total_subsongs; 
    int target_subsong = sf->stream_index; 
    int32_t offset; 
    int32_t name_offset, name_size; 
    int32_t id; 
    int32_t flags, stream_offset, num_samples, sample_rate, stream_size; 
    int channels, loop_flag; 
 
    /* checks */ 
    if (!check_extensions(sf, "snd")) 
        goto fail; 
    if (!is_id32be(0x00, sf, "SOND")) 
        goto fail; 
 
    major_version = read_s16be(4, sf); 
    minor_version = read_s16be(6, sf); 
 
    if (major_version != 3) 
        goto fail; 
 
    bool valid_minor_version = false; 
    switch (minor_version) 
    { 
    case 0: // [Spider-Man 2002 (GC)] 
    case 3: // [Kelly Slater's Pro Surfer (GC)], [Minority Report: Everybody Runs (GC)], [NHL 2K3 (GC)] 
        valid_minor_version = true; 
        break; 
    default: 
        break; 
    } 
    if (!valid_minor_version) 
        goto fail; 
 
    char gcw_file_name[32]; 
    read_string(gcw_file_name, sizeof(gcw_file_name), 8, sf); 
 
    total_subsongs = read_s32be(0x28, sf); 
    if (target_subsong == 0) target_subsong = 1; 
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail; 
 
    int32_t ch1_coef_offset; 
    switch (minor_version) 
    { 
    case 0: 
        offset = 0x40 + (target_subsong - 1) * 0x88; 
        // v03.00 has names. 
        name_offset = offset + 0x00; 
        name_size = 0x40; 
        offset += 0x40; 
 
        flags = read_s32be(offset + 0x00, sf); 
        stream_offset = read_s32be(offset + 0x04, sf); 
        num_samples = read_s32be(offset + 0x08, sf); 
        sample_rate = read_s32be(offset + 0x0c, sf); 
        // 0x10 - suspected stream_type 
        // 0x14 - float (usually 1.0) 
 
        ch1_coef_offset = offset + 0x18; 
 
        offset += 0x48; 
        break; 
 
    case 3: 
        offset = 0x40 + (target_subsong - 1) * 0x100; 
        // v03.03 snd has both IDs and names instead of just names like before. 
        id = read_s32be(offset + 0x00, sf); 
 
        name_offset = offset + 0x04; 
        name_size = 0xb4; 
        offset += 0xb8; 
 
        flags = read_s32be(offset + 0x00, sf); 
        stream_offset = read_s32be(offset + 0x04, sf); 
        num_samples = read_s32be(offset + 0x08, sf); 
        sample_rate = read_s32be(offset + 0x0c, sf); 
        // 0x10 - suspected stream_type 
        // 0x14 - float (usually 1.0) 
 
        // grabbing DSP coefs is a matter of whether or not a given sound is streamed. 
        // (indicated by (flags & 0x01) == 0x01 if so, (flags & 0x01) == 0x00 if not) 
        // if sound requires gcw (and is mono), coef data is stored in the snd. 
        // if sound requires a bigfile however, coef data is left blank on the snd; 
        // instead said data is stored on said bigfile regardless of if sound is mono or stereo. 
 
        // however ch1_coef_offset is for reading mono sound coef data from the snd. 
        ch1_coef_offset = offset + 0x18; 
 
        offset += 0x48; 
        break; 
 
    default: 
        // (todo) why churn out errors *twice* when minor_version turns out to be anything other than 0 and 3? 
        goto fail; 
    } 
 
    if ((stream_offset == -1) && (num_samples == -1) && (sample_rate == -1)) { 
        // just pass on some silence to compensate for dummy data. 
        vgmstream = init_vgmstream_silence_container(total_subsongs); 
        if (!vgmstream) goto fail; 
 
        close_streamfile(sf_body); 
        return vgmstream; 
    } 
 
    // snd format does not support any other channel setup beyond "mono" (1ch) and "stereo" (2ch). 
    channels = (flags & 0x02) ? 2 : 1; 
    // with (flags & 0x04) set, entire sound will loop from start-to-finish. no exceptions. 
    loop_flag = (flags & 0x04) ? 1 : 0; 
 
    bool needs_ngc_dtk = (minor_version == 0) && 
        (((flags & 0x02) == 0x02) && ((flags & 0x01) == 0x01)); 
    // ^ only possible with a combination of minor version number and a combination of certain set flags. 
    bool dsp_is_in_bigfile = (minor_version == 3) && ((flags & 0x01) == 0x01); // signals bigfile-adjacent dsp. 
    bool adp_file_loaded = false; 
    bool is_ngc_dtk = false; 
    bool common_str_loaded = false; 
    char filename[256]; 
    int load_i; 
    if (needs_ngc_dtk) 
    { 
        char adp_name[64]; 
        // stream is a named .adp, with stream_offset set to 0. 
        read_string(adp_name, name_size, name_offset, sf); 
        for (load_i = 0; load_i < 2; load_i++) 
        { 
            if (adp_file_loaded) 
                break; 
 
            switch (load_i) 
            { 
            case 0: // if adp file is found outside snd+gcw folder. 
                snprintf(filename, sizeof(filename), "..\\%s.adp", adp_name); 
                break; 
            case 1: // if adp file is found in snd+gcw folder. 
                snprintf(filename, sizeof(filename), "%s.adp", adp_name); 
                break; 
            default: 
                break; 
            } 
 
            sf_body = open_streamfile_by_filename(sf, filename); 
            if (sf_body && !adp_file_loaded) 
                adp_file_loaded = true; 
        } 
 
        if (!adp_file_loaded) 
        { 
            vgm_logi("SND+GCW: external adp file '%s' not found (outside or into snd+gcw folder).\n", adp_name); 
            goto fail; 
        } 
 
        // stream is a headerless Nintendo GameCube DTK file, 
        // can be played elsewhere but snd has stream metadata so we won't be calling another vgmstream. 
        is_ngc_dtk = true; 
    } 
    else if (dsp_is_in_bigfile) 
    { 
        for (load_i = 0; load_i < 2; load_i++) 
        { 
            if (common_str_loaded) 
                break; 
 
            switch (load_i) 
            { 
            case 0: // if bigfile is found outside snd+gcw folder. 
                snprintf(filename, sizeof(filename), "..\\%s", "COMMON.STR"); 
                break; 
            case 1: // if bigfile is found in snd+gcw folder. 
                snprintf(filename, sizeof(filename), "%s", "COMMON.STR"); 
                break; 
            default: 
                break; 
            } 
 
            sf_body = open_streamfile_by_filename(sf, filename); 
            if (sf_body && !common_str_loaded) 
                common_str_loaded = true; 
        } 
 
        if (!common_str_loaded) 
        { 
            vgm_logi("SND+GCW: external file '%s' not found (outside or into snd+gcw folder).\n", filename); 
            goto fail; 
        } 
    } 
    else { 
        sf_body = open_streamfile_by_filename(sf, gcw_file_name); 
        if (!sf_body) 
        { 
            VGM_LOG("SND+GCW: Can't find GCW"); 
            goto fail; 
        } 
    } 
 
    // conjure up potential file sizes for both dsp and dtk. 
    // in dsp's case, it's simply a matter of multiplying size_calc to what a dsp size would look like in bytes. 
    int32_t size_calc, size_modulus; 
    int32_t temp_size, bigfile_dsp_block_size, bigfile_dsp_block_mod; 
    int32_t dsp_size, dtk_size; 
    if (!is_ngc_dtk) 
    { 
        // re-calc num_samples into a dividend of 14. 
        size_calc = (num_samples) / 14; 
        size_modulus = 14 - ((num_samples) % 14); 
        if (size_modulus != 14) 
            size_calc++; 
 
        dsp_size = size_calc * (8 * channels); 
        if (dsp_is_in_bigfile) 
        { 
            // bigfile-adjacent dsp data is divided into per-channel (individual) blocks and are all stored into a certain block size; 
            // other words, think of it as the interleave layout that you see elsewhere in vgmstream code. 
            // the block size itself is in a constant flux and heavily reliant on how long or how short a given sound can get,  
            // however, said size is hard locked to certain values, and is always a multiple of 2048. 
            // for smaller sounds, it cannot go below 2048 bytes (totaling 3584 samples per channel). 
            // for larger sounds, it cannot go beyond 32768 bytes (totaling 57344 samples per channel). 
            temp_size = (size_calc * 8) + 0x40; 
            bigfile_dsp_block_size = temp_size / 0x800; 
            bigfile_dsp_block_mod = 0x800 - (temp_size % 0x800); 
            if (bigfile_dsp_block_mod != 0) bigfile_dsp_block_size++; 
            bigfile_dsp_block_size *= 0x800; 
            if (bigfile_dsp_block_size > 0x8000) 
                bigfile_dsp_block_size = 0x8000; 
        } 
    } else { 
        // get size of loaded adp and calc num_samples from there. 
        dtk_size = get_streamfile_size(sf_body); 
        num_samples = dtk_size / 32 * 28; 
        // num_samples already exist for dtk sounds as listed in the snd but they're all unused. 
        // so this new calc is just taking over a var that's already read from a physical file. 
    } 
    // stream_size calc is done *after* loading sf_body; 
    // this is to compensate for adp being loaded afterwards. 
    stream_size = (!is_ngc_dtk) ? dsp_size : dtk_size; 
 
    vgmstream = allocate_vgmstream(channels, loop_flag); 
    if (!vgmstream) goto fail; 
 
    vgmstream->meta_type = meta_SND_GCW_STR; 
    vgmstream->sample_rate = sample_rate; 
    vgmstream->num_streams = total_subsongs; 
    vgmstream->stream_size = stream_size; 
    vgmstream->num_samples = num_samples; 
 
    if (flags & 0x04) 
    { 
        vgmstream->loop_start_sample = 0; 
        vgmstream->loop_end_sample = vgmstream->num_samples; 
    } 
 
    if (!is_ngc_dtk) 
    { 
        if (minor_version == 0 && ((flags & 0x02) == 0x02)) { 
            // [Spider-Man (GC)] snd do not support stereo dsp. 
            vgm_logi("SND+GCW: stereo dsp not supported.\n"); 
            goto fail; 
        } 
 
        if (minor_version == 3 
            &&  
            (((flags & 0x02) == 0x02) && ((flags & 0x01) == 0))) 
        { 
            // stereo sound only seen on bigfiles, not on gcw [Minority Report: Everybody Runs (GC)] 
            vgm_logi("SND+GCW: stereo sound not on an external bigfile.\n"); 
            goto fail; 
        } 
 
        vgmstream->coding_type = coding_NGC_DSP; 
        vgmstream->layout_type = (dsp_is_in_bigfile) ? layout_blocked_snd_gcw_str : layout_none; 
        // layout_blocked_snd_gcw_str is new; relies on heap-allocated data to do most of the work. 
        if (!dsp_is_in_bigfile) 
            // as said before, ch1 coef data is either stored on snd, or on external bigfile. 
            // however, we'll do this differently. instead, we'll only parse said data if it's in the snd. 
            dsp_read_coefs_separately_be(vgmstream, sf, ch1_coef_offset, 0); 
        else 
        { 
            // otherwise, just allocate blocked layout data. 
            vgmstream->layout_data = build_blocked_snd_gcw_str(num_samples, channels, (size_t)(bigfile_dsp_block_size)); 
            if (!vgmstream->layout_data) goto fail; 
        } 
    } 
    else { 
        vgmstream->coding_type = coding_NGC_DTK; 
        vgmstream->layout_type = layout_none; 
    } 
 
    read_string(vgmstream->stream_name, name_size, name_offset, sf); 
 
    if (!vgmstream_open_stream(vgmstream, sf_body, stream_offset)) 
        goto fail; 
    close_streamfile(sf_body); 
    return vgmstream; 
fail: 
    close_streamfile(sf_body); 
    close_vgmstream(vgmstream); 
    return NULL; 
} 
 
static snd_gcw_str_blocked_layout_data* build_blocked_snd_gcw_str(int32_t num_samples, int channels, size_t block_size) 
{ 
    snd_gcw_str_blocked_layout_data* data = NULL; 
 
    // init layout. 
    data = init_snd_gcw_str_blocked_layout(num_samples, channels, block_size); 
    if (!data) 
        goto fail; 
 
    // coef_info per-channel pointers are necessary for the blocked layout to actually work; 
    // bigfile-adjacent dsp files have their first per-channel block 
    // start up with a "header" consisting of coef data followed by blank space. 
    for (int i = 0; i < channels; i++) 
    { 
        data->info[i] = calloc(1, sizeof(snd_gcw_str_first_block_header_info)); 
        if (!data->info[i]) 
            goto fail; 
 
        data->info[i]->exists = true; 
    } 
 
    return data; 
fail: 
    free_snd_gcw_str_blocked_layout(data); 
    return NULL; 
} 
