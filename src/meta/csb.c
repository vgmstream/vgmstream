#include "meta.h"
#include "../coding/coding.h"
#include "../util/cri_utf.h"


/* CSB (Cue Sheet Binary?) - CRI container of memory audio, often together with a .cpk wave bank */
VGMSTREAM* init_vgmstream_csb(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    uint32_t subfile_offset, subfile_size;
    utf_context* utf = NULL;
    utf_context* utf_sdl = NULL;
    int total_subsongs, target_subsong = sf->stream_index;
    uint8_t fmt = 0;
    const char* stream_name = NULL;


    /* checks */
    if (!is_id32be(0x00,sf, "@UTF"))
        goto fail;
    if (!check_extensions(sf, "csb"))
        goto fail;

    /* .csb is an early, simpler version of .acb+awk (see acb.c) used until ~2013?
     * Can stream from .cpk but this only loads memory data. */
    {
        int rows, sdl_rows, sdl_row, i;
        const char *name;
        const char *row_name;
        const char *sdl_name;
        uint32_t sdl_offset, sdl_size, offset, size;
        uint32_t table_offset = 0x00;
        uint8_t ttype;
        int found = 0;


        utf = utf_open(sf, table_offset, &rows, &name);
        if (!utf) goto fail;

        if (strcmp(name, "TBLCSB") != 0)
            goto fail;

        /* each TBLCSB row has a name and subtable with actual things:
         * - INFO (TBL_INFO): table type/version, rarely ommited [Nights: Journey of Dreams (Wii)-some csb]
         * - CUE (TBLCUE): base cues
         * - SYNTH (TBLSYN): cue configs
         * - SOUND_ELEMENT (TBLSDL): audio info/data (usually AAX)
         * - ISAAC (TBLISC): 3D config
         * - VOICE_LIMIT_GROUP (TBLVLG): system info?
         * Subtable can be empty but still appear (0 rows).
         */
        sdl_row = -1;
        for (i = 0; i < rows; i++) {
            if (!utf_query_string(utf, i, "name", &row_name))
                goto fail;
            if (strcmp(row_name, "SOUND_ELEMENT") == 0) {
                sdl_row = i; /* usually 2 or 3 */
                break;
            }
        }
        if (sdl_row < 0)
            goto fail;


        /* read SOUND_ELEMENT table */
        if (!utf_query_u8(utf, sdl_row, "ttype", &ttype) || ttype != 4)
            goto fail;
        if (!utf_query_data(utf, sdl_row, "utf", &sdl_offset, &sdl_size))
            goto fail;

        utf_sdl = utf_open(sf, sdl_offset, &sdl_rows, &sdl_name);
        if (!utf_sdl) goto fail;

        if (strcmp(sdl_name, "TBLSDL") != 0)
            goto fail;

        total_subsongs = 0;
        if (target_subsong == 0) target_subsong = 1;

        /* get target subsong */
        for (i = 0; i < sdl_rows; i++) {
            uint8_t stream_flag;

            if (!utf_query_u8(utf_sdl, i, "stmflg", &stream_flag))
                goto fail;

            /* only internal data for now (when 1 this refers to a .cpk subfile probably using "name", has size 0) */
            if (stream_flag)
                continue;

            total_subsongs++;
            if (total_subsongs == target_subsong && !found) {

                if (!utf_query_string(utf_sdl, i, "name", &stream_name))
                    goto fail;
                if (!utf_query_data(utf_sdl, i, "data", &offset, &size))
                    goto fail;
                if (!utf_query_u8(utf_sdl, i, "fmt", &fmt))
                    goto fail;
                /* also nch/sfreq/nsmpl info */

                found = 1;
            }
        }

        if (!found) goto fail;
        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

        subfile_offset = /*sdl_offset +*/ offset;
        subfile_size = size;

        /* column exists but can be empty */
        if (subfile_size == 0)
            goto fail;
    }

    //;VGM_LOG("CSB: subfile offset=%x + %x\n", subfile_offset, subfile_size);
    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, "aax");
    if (!temp_sf) goto fail;

    switch(fmt) {
        case 0: /* AAX */
        case 6: /* HCA */
            vgmstream = init_vgmstream_aax(temp_sf);
            if (!vgmstream) goto fail;
            break;

        case 2: /* AHX */
            vgmstream = init_vgmstream_utf_ahx(temp_sf);
            if (!vgmstream) goto fail;
            break;

        case 4: /* ADPCM_WII */
            vgmstream = init_vgmstream_utf_dsp(temp_sf);
            if (!vgmstream) goto fail;
            break;

        default:
            VGM_LOG("CSB: unknown format %i\n", fmt);
            goto fail;
    }

    vgmstream->num_streams = total_subsongs;
    strncpy(vgmstream->stream_name, stream_name, STREAM_NAME_SIZE-1);

    utf_close(utf);
    utf_close(utf_sdl);
    close_streamfile(temp_sf);
    return vgmstream;

fail:
    utf_close(utf);
    utf_close(utf_sdl);
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}


/* CRI's UTF wrapper around DSP [Sonic Colors (Wii)-sfx, NiGHTS: Journey of Dreams (Wii)-sfx] */
VGMSTREAM* init_vgmstream_utf_dsp(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    uint8_t loop_flag = 0, channels;
    uint32_t sample_rate, num_samples, loop_start, loop_end, interleave;
    uint32_t data_offset, data_size,  header_offset, header_size;
    utf_context* utf = NULL;


    /* checks */
    if (!is_id32be(0x00,sf, "@UTF"))
        goto fail;

    /* .aax: assumed
     * (extensionless): extracted names inside csb/cpk often don't have extensions */
    if (!check_extensions(sf, "aax,"))
        goto fail;

    /* contains a simple UTF table with one row and various columns being header info */
    {
        int rows;
        const char* name;
        uint32_t table_offset = 0x00;


        utf = utf_open(sf, table_offset, &rows, &name);
        if (!utf) goto fail;

        if (strcmp(name, "ADPCM_WII") != 0)
            goto fail;

        if (rows != 1)
            goto fail;

        if (!utf_query_u32(utf, 0, "sfreq", &sample_rate))
            goto fail;
        if (!utf_query_u32(utf, 0, "nsmpl", &num_samples))
            goto fail;
        if (!utf_query_u8(utf, 0, "nch", &channels))
            goto fail;
        if (!utf_query_u8(utf, 0, "lpflg", &loop_flag)) /* full loops */
            goto fail;
        /* for some reason data is stored before header */
        if (!utf_query_data(utf, 0, "data", &data_offset, &data_size))
            goto fail;
        if (!utf_query_data(utf, 0, "header", &header_offset, &header_size))
            goto fail;

        if (channels < 1 || channels > 2)
            goto fail;
        if (header_size != channels * 0x60)
            goto fail;

        start_offset = data_offset;
        interleave = (data_size+7) / 8 * 8 / channels;

        loop_start = read_32bitBE(header_offset + 0x10, sf);
        loop_end   = read_32bitBE(header_offset + 0x14, sf);
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(loop_start);
    vgmstream->loop_end_sample   = dsp_nibbles_to_samples(loop_end) + 1;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;
    vgmstream->meta_type = meta_UTF_DSP;

    dsp_read_coefs_be(vgmstream, sf, header_offset+0x1c, 0x60);

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    utf_close(utf);
    return vgmstream;

fail:
    utf_close(utf);
    close_vgmstream(vgmstream);
    return NULL;
}

/* CRI's UTF wrapper around AHX [Yakuza: Dead Souls (PS3)-voices] */
VGMSTREAM* init_vgmstream_utf_ahx(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    uint32_t subfile_offset, subfile_size;
    utf_context* utf = NULL;


    /* checks */
    if (!is_id32be(0x00,sf, "@UTF"))
        goto fail;

    /* .aax: assumed
     * (extensionless): extracted names inside csb/cpk often don't have extensions */
    if (!check_extensions(sf, "aax,"))
        goto fail;

    /* contains a simple UTF table with one row and offset+size info */
    {
        int rows;
        const char* name;
        uint32_t table_offset = 0x00;

        utf = utf_open(sf, table_offset, &rows, &name);
        if (!utf) goto fail;

        if (strcmp(name, "AHX") != 0)
            goto fail;

        if (rows != 1)
            goto fail;

        if (!utf_query_data(utf, 0, "data", &subfile_offset, &subfile_size))
            goto fail;
    }

    //;VGM_LOG("UTF_AHX: subfile offset=%x + %x\n", subfile_offset, subfile_size);
    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, "ahx");
    if (!temp_sf) goto fail;

    vgmstream = init_vgmstream_ahx(temp_sf);
    if (!vgmstream) goto fail;

    utf_close(utf);
    close_streamfile(temp_sf);
    return vgmstream;

fail:
    utf_close(utf);
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
