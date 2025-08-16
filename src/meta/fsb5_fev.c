#include "meta.h"
#include "../coding/coding.h"
#include "../util/chunks.h"


static int get_subsongs(STREAMFILE* sf, uint32_t fsb5_offset, uint32_t fsb5_size);

/* FEV+FSB5 container [Just Cause 3 (PC), Shantae: Half-Genie Hero (Switch)] */
VGMSTREAM* init_vgmstream_fsb5_fev_bank(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    uint32_t version = 0;
    int fsb5_pos, fsb5_subsong;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    if (!is_id32be(0x00,sf, "RIFF"))
        return NULL;
    if (!is_id32be(0x08,sf, "FEV "))
        return NULL;

    if (!check_extensions(sf, "bank"))
        return NULL;

    version = read_u32le(0x14,sf); /* newer FEV have some kind of sub-version at 0x18 */

    /* .fev is an event format referencing various external .fsb, but FMOD can bake .fev and .fsb to
     * form a .bank, which is the format we support here (regular .fev is complex and not very interesting).
     * Format is RIFF with FMT (main), LIST (config) and SND (FSB5 data), we want the FSB5 offset inside LIST */

    off_t chunk_offset;
    if (!find_chunk_le(sf, get_id32be("LIST"), 0x0c, 0, &chunk_offset,NULL))
        return NULL;
    if (read_u32be(chunk_offset+0x00,sf) != get_id32be("PROJ") ||
        read_u32be(chunk_offset+0x04,sf) != get_id32be("BNKI"))
        return NULL; /* event .fev has "OBCT" instead of "BNKI" (which can also be empty) */


    /* inside BNKI is a bunch of LISTs each with event subchunks and somewhere the FSB5 offsets */
    uint32_t sndh_offset = 0, sndh_size = 0;
    //uint32_t pefx_offset = 0, pefx_size = 0;
    uint32_t offset = chunk_offset + 0x04;
    while (sndh_offset == 0 && offset < get_streamfile_size(sf)) {
        uint32_t chunk_type = read_u32be(offset + 0x00,sf);
        uint32_t chunk_size = read_u32le(offset + 0x04,sf);
        offset += 0x08;

        if (chunk_type == 0xFFFFFFFF || chunk_size == 0xFFFFFFFF)
            return NULL;

        //;VGM_LOG("FSB5 FEV: chunk=%x, offset=%x, size=%x\n", chunk_type, offset - 0x08, chunk_size);
        switch(chunk_type) {
            case 0x4C495354: /* "LIST" */
                // "SNDH" (usually v0x28 but also in Fall Guys's BNK_Music_RoundReveal)
                if (read_u32be(offset + 0x04, sf) == get_id32be("SNDH")) {
                    sndh_offset = offset + 0x0c;
                    sndh_size = read_s32le(offset + 0x08,sf);
                }
#if 0
                // multiple CrankcaseAudio REV ADM3 [Rennsport (PC)]
                // inside PEFX there are multiple LIST w/ LCNT + PEFF + PEFB > ADM3 (each with multiple subsongs).
                // can be set found empty (no LCNT) with SNDH, so possibly both FSB5 and ADM3 could coexist
                if (read_u32be(offset + 0x00, sf) == get_id32be("PEFX")) {
                    pefx_offset = offset + 0x0c;
                    pefx_size = chunk_size;
                }
#endif
                break;

            case 0x534E4448: /* "SNDH" */
                sndh_offset = offset;
                sndh_size = chunk_size;
                if (sndh_size == 0) {
                    vgm_logi("FSB: bank has no subsongs (ignore)\n");
                    goto fail;
                }
                break;

            default:
                break;
        }

        offset += chunk_size;
    }

    if (sndh_offset == 0)
        return NULL;
    //;VGM_LOG("FSB5 FEV: offset=%x, size=%x\n", sndh_offset, sndh_size);

    uint32_t subfile_offset, subfile_size;
    {
        /* known bank versions:
         * 0x28: Transistor (iOS) [~2015]
         * 0x50: Runic Rampage (PC), Forza 7 (PC), Shantae: Half Genie Hero (Switch) [~2017]
         * 0x58: Mana Spark (PC), Shantae and the Seven Sirens (PC) [~2018]
         * 0x63: Banner Saga 3 (PC) [~2018]
         * 0x64: Guacamelee! Super Turbo Championship Edition (Switch) [~2018]
         * 0x65: Carrion (Switch) [~2020]
         * 0x7D: Fall Guys (PC) [~2020]
         * 0x84: SCP Unity (PC) [~2020]
         * 0x86: Hades (Switch) [~2020] */
        uint32_t entry_size = version <= 0x28 ? 0x04 : 0x08;

        /* 0x00: unknown (chunk version? ex LE: 0x00080003, 0x00080005) */
        int banks = (sndh_size - 0x04) / entry_size;

        /* multiple banks is possible but rare [Hades (Switch), Guacamelee 2 (Switch)],
         * must map bank (global) subsong to FSB (internal) subsong */

        if (target_subsong == 0) target_subsong = 1;

        fsb5_pos = 0;
        fsb5_subsong = -1;
        total_subsongs = 0;
        for (int i = 0; i < banks; i++) {
            //TODO: fsb5_size fails for v0x28< + encrypted, but only used with multibanks = unlikely
            uint32_t fsb5_offset  = read_u32le(sndh_offset + 0x04 + entry_size * i + 0x00,sf);
            uint32_t fsb5_size    = read_u32le(sndh_offset + 0x08 + entry_size * i,sf);
            int fsb5_subsongs = get_subsongs(sf, fsb5_offset, fsb5_size);
            if (!fsb5_subsongs) {
                vgm_logi("FSB: couldn't load bank %i at %x (encrypted?)\n", i, fsb5_offset);
                goto fail;
            }

            /* target in range */
            if (target_subsong >= total_subsongs + 1 && target_subsong < total_subsongs + 1 + fsb5_subsongs) {
                fsb5_pos = i;
                fsb5_subsong = target_subsong - total_subsongs;
            }

            total_subsongs += fsb5_subsongs;
        }
        if (fsb5_subsong < 0)
            goto fail;

        if (version <= 0x28) {
            subfile_offset  = read_u32le(sndh_offset+0x04 + entry_size*fsb5_pos,sf);
            subfile_size    = /* meh */
                read_u32le(subfile_offset + 0x0C,sf) + 
                read_u32le(subfile_offset + 0x10,sf) + 
                read_u32le(subfile_offset + 0x14,sf) + 
                0x3C;
        }
        else {
            subfile_offset  = read_u32le(sndh_offset+0x04 + entry_size*fsb5_pos,sf);
            subfile_size    = read_u32le(sndh_offset+0x08 + entry_size*fsb5_pos,sf);
        }
    }

    ;VGM_LOG("FSB5 FEV: offset=%x, size=%x\n", subfile_offset, subfile_size);

    temp_sf = setup_subfile_streamfile(sf, subfile_offset,subfile_size, "fsb");
    if (!temp_sf) goto fail;

    temp_sf->stream_index = fsb5_subsong; /* relative subsong, in case of multiple FSBs */

    vgmstream = (read_u32be(0x00, temp_sf) == 0x46534235) ? /* "FSB5" (better flag?)*/
        init_vgmstream_fsb5(temp_sf) :
        init_vgmstream_fsb_encrypted(temp_sf);
    if (!vgmstream) {
        vgm_logi("FSB: couldn't load bank (encrypted?)\n");
        goto fail;
    }

    vgmstream->stream_index = sf->stream_index; //target_subsong; /* 0-index matters */
    vgmstream->num_streams = total_subsongs;

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}

static int get_subsongs(STREAMFILE* sf, uint32_t fsb5_offset, uint32_t fsb5_size) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    int subsongs = 0;

    /* standard */
    if (is_id32be(fsb5_offset, sf, "FSB5")) {
        return read_s32le(fsb5_offset + 0x08,sf);
    }

    /* encrypted, meh */
    temp_sf = setup_subfile_streamfile(sf, fsb5_offset, fsb5_size, "fsb");
    if (!temp_sf) goto end;

    temp_sf->stream_index = 0;
    vgmstream = init_vgmstream_fsb_encrypted(temp_sf);
    if (!vgmstream) goto end;

    subsongs = vgmstream->num_streams;

end:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return subsongs;
}
