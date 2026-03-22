#include "meta.h"
#include "../coding/coding.h"
#include "../util/cri_utf.h"
#include "../util/cri_keys.h"
#include "../util/companion_files.h"
#include "crid_streamfile.h"

static uint32_t get_sfa_header_offset(STREAMFILE* sf, int chno);

/* CRID - CRI .usm videos */
VGMSTREAM* init_vgmstream_crid(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    utf_context* utf = NULL;
    utf_context* utf_sfa = NULL;


    /* checks */
    if (!is_id32be(0x00,sf, "CRID"))
        return NULL;
    if (!check_extensions(sf, "usm"))
        return NULL;

    /* .USM info:
     * - starts with a 'CRID' audio header and UTF header at 0x20 (with info about used streams)
     * - has xN @SFA (audio) and @SFV (video) chunks with regular payload (hca, adx, m2v, etc)
     * - chunks can be of types: data=0, header=1, comment=2, seek=3
     * - chunks may be partially encrypted (internal .hca may be encrypted instead as well)
     * - chunks have an associated stream (chno)
     * - header chunks also has an UTF header with basic stream info (codec, channels, etc)
     * (see crid_streamfile info)
     */

    char stream_name[STREAM_NAME_SIZE] = {0};
    int total_subsongs, target_subsong = sf->stream_index;
    uint64_t keycode = 0;
    uint8_t audio_codec = 0;

    {
        int crid_rows, sfa_rows;
        const char* name;
        const char* sfa_name;
        uint32_t sfa_offset = 0;
        uint16_t header_size = read_u16be(0x08,sf) + 0x08; // always 0x20 in practice

        // base table
        utf = utf_open(sf, header_size, &crid_rows, &name);
        if (!utf) goto fail;

        if (strcmp(name, "CRIUSF_DIR_STREAM") != 0)
            goto fail;

        total_subsongs = 0;
        if (target_subsong == 0) target_subsong = 1;

        for (int i = 0; i < crid_rows; i++) {
            const char* row_name;
            uint32_t stmid = 0;
            uint16_t chno = 0;

            if (!utf_query_u32(utf, i, "stmid", &stmid))
                goto fail;
            if (!utf_query_u16(utf, i, "chno", &chno))
                goto fail;
            if (utf_query_string(utf, i, "filename", &row_name) && chno + 1 == target_subsong) {
                snprintf(stream_name, sizeof(stream_name) - 1, "%s", row_name);
            }

            if (stmid == get_id32be("@SFA")) {
                total_subsongs++;
            }
        }

        if (total_subsongs == 0) {
            vgm_logi("CRID: file has no audio (ignore)\n");
            goto fail;
        }
        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

        // find target header and get more info
        sfa_offset = get_sfa_header_offset(sf, target_subsong - 1);
        header_size = read_u16be(sfa_offset + 0x08,sf) + 0x08; 

        utf_sfa = utf_open(sf, sfa_offset + header_size, &sfa_rows, &sfa_name);
        if (!utf_sfa) goto fail;

        if (strcmp(sfa_name, "AUDIO_HDRINFO") != 0 || sfa_rows != 1)
            goto fail;

        if (!utf_query_u8(utf_sfa, 0, "audio_codec", &audio_codec))
            goto fail;
        // possibly useful? "channel_config" "ambisonic"
    }

    /* find decryption key */
    {
        uint8_t keybuf[20+1] = {0}; // max keystring 20, +1 extra null
        size_t key_size;

        key_size = read_key_file(keybuf, sizeof(keybuf) - 1, sf);

        bool is_keystring = cri_key9_valid_keystring(keybuf, key_size);

        if (is_keystring) { // number
            keycode = cri_keystring_to_keycode(keybuf);
        }
        else if (key_size == 0x08) { // hex
            keycode = get_u64be(keybuf+0x00);
        }
        else {
            //find_usm_key(xxx, &keycode);
        }
    }

    {
        VGMSTREAM* (*init_vgmstream)(STREAMFILE* sf);
        const char* ext = NULL;

        // could also get first data chunk
        switch(audio_codec) {
            case 2:
                init_vgmstream = init_vgmstream_adx;
                ext = "adx";
                break;

            case 4:
                init_vgmstream = init_vgmstream_hca;
                ext = "hca";
                break;

            default:
                VGM_LOG("USM: unknown audio_codec %i\n", audio_codec);
                goto fail;
        }

        /* dechunk and optional encryption */
        temp_sf = setup_crid_streamfile(sf, ext, keycode, target_subsong);
        if (!temp_sf) goto fail;

        vgmstream = init_vgmstream(temp_sf);
        if (!vgmstream) goto fail;

        vgmstream->num_streams = total_subsongs;
        snprintf(vgmstream->stream_name, STREAM_NAME_SIZE-1, "%s", stream_name);

        //TODO: mch HCA seem to be encoded like L R SL SR FC LFE
    }


    utf_close(utf);
    utf_close(utf_sfa);
    close_streamfile(temp_sf);
    return vgmstream;

fail:
    utf_close(utf);
    utf_close(utf_sfa);
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}

static uint32_t get_sfa_header_offset(STREAMFILE* sf, int chno) {
    uint32_t offset = 0;
    uint32_t max_offset = get_streamfile_size(sf);
    while (offset < max_offset) {
        uint32_t chunk_id   = read_u32be(offset + 0x00, sf);
        uint32_t chunk_size = read_u32be(offset + 0x04, sf);
        uint8_t  chunk_chno = read_u8   (offset + 0x0c, sf);
        uint16_t chunk_type = read_u16be(offset + 0x0e, sf);

        if (chunk_id != get_id32be("@SFA") || chunk_chno != chno) { //header 
            offset += 0x08 + chunk_size;
            continue;
        }

        // typical order: header=1, comment=2, seek=3, data=0
        if (chunk_type == 0)
            return 0;

        return offset;
    }

    return 0;
}
