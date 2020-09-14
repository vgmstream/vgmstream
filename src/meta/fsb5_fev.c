#include "meta.h"
#include "../coding/coding.h"

/* FEV+FSB5 container [Just Cause 3 (PC), Shantae: Half-Genie Hero (Switch)] */
VGMSTREAM* init_vgmstream_fsb5_fev_bank(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    off_t subfile_offset, chunk_offset, bank_offset, offset;
    size_t subfile_size, bank_size;
    uint32_t version = 0;


    /* checks */
    if (!check_extensions(sf, "bank"))
        goto fail;

    if (read_u32be(0x00,sf) != 0x52494646) /* "RIFF" */
        goto fail;
    if (read_u32be(0x08,sf) != 0x46455620) /* "FEV " */
        goto fail;
    version = read_u32le(0x14,sf); /* newer FEV have some kind of sub-version at 0x18 */

    /* .fev is an event format referencing various external .fsb, but FMOD can bake .fev and .fsb to
     * form a .bank, which is the format we support here (regular .fev is complex and not very interesting).
     * Format is RIFF with FMT (main), LIST (config) and SND (FSB5 data), we want the FSB5 offset inside LIST */

    if (!find_chunk_le(sf, 0x4C495354, 0x0c, 0, &chunk_offset,NULL)) /* "LIST" */
        goto fail;
    if (read_u32be(chunk_offset+0x00,sf) != 0x50524F4A || /* "PROJ" */
        read_u32be(chunk_offset+0x04,sf) != 0x424E4B49)   /* "BNKI" */
        goto fail; /* event .fev has "OBCT" instead of "BNKI" (which can also be empty) */


    /* inside BNKI is a bunch of LIST each with event subchunks and somewhere the FSB5 offset */
    bank_offset = 0;
    offset = chunk_offset + 0x04;
    while (bank_offset == 0 && offset < get_streamfile_size(sf)) {
        uint32_t chunk_type = read_u32be(offset + 0x00,sf);
        uint32_t chunk_size = read_u32le(offset + 0x04,sf);
        offset += 0x08;

        if (chunk_type == 0xFFFFFFFF || chunk_size == 0xFFFFFFFF)
            goto fail;

        switch(chunk_type) {
            case 0x4C495354: /* "LIST" with "SNDH" (usually v0x28 but also in Fall Guys's BNK_Music_RoundReveal) */
                if (read_u32be(offset + 0x04, sf) == 0x534E4448) {
                    bank_offset = offset + 0x0c;
                    bank_size = read_s32le(offset + 0x08,sf);
                }
                break;

            case 0x534E4448: /* "SNDH" */
                bank_offset = offset;
                bank_size = chunk_size;
                break;

            default:
                break;
        }

        offset += chunk_size;
    }

    if (bank_offset == 0)
        goto fail;

    /* 0x00: unknown (chunk version? ex LE: 0x00080003, 0x00080005) */
    {
        /* versions:
         * 0x28: Transistor (iOS) [+2015]
         * 0x50: Runic Rampage (PC), Forza 7 (PC), Shantae: Half Genie Hero (Switch) [+2017]
         * 0x58: Mana Spark (PC), Shantae and the Seven Sirens (PC) [+2018]
         * 0x63: Banner Saga 3 (PC) [+2018]
         * 0x64: Guacamelee! Super Turbo Championship Edition (Switch) [+2018]
         * 0x65: Carrion (Switch) [+2020]
         * 0x7D: Fall Guys (PC) [+2020] */
        size_t entry_size = version <= 0x28 ? 0x04 : 0x08;
        int banks;

        /* multiple banks is possible but rare (only seen an extra "Silence" FSB5 in Guacamelee 2 (Switch),
         *  which on PC is a regular subsong in the only FSB5) */
        banks = (bank_size - 0x04) / entry_size;
        VGM_ASSERT(banks > 1, "FSB5FEV: multiple banks found\n");

        /* Could try to set stream index based on FSB subsong ranges, also fixing num_streams and stream_index
         * kinda involved and hard to test so for now just ignore it and use first offset */

        if (banks > 2)
            goto fail;
        if (banks == 2) {
            off_t temp_offset  = read_u32le(bank_offset + 0x04 + entry_size*1 + 0x00,sf);
            //size_t temp_size = read_u32le(bank_offset + 0x04 + entry_size*1 + 0x04,sf);

            int bank_subsongs = read_s32le(temp_offset + 0x08,sf);
            if (bank_subsongs != 1) goto fail;
        }

        if (version <= 0x28) {
            subfile_offset  = read_u32le(bank_offset+0x04,sf);
            subfile_size    = /* meh */
                read_u32le(subfile_offset + 0x0C,sf) + 
                read_u32le(subfile_offset + 0x10,sf) + 
                read_u32le(subfile_offset + 0x14,sf) + 
                0x3C;
        }
        else {
            subfile_offset  = read_u32le(bank_offset+0x04,sf);
            subfile_size    = read_u32le(bank_offset+0x08,sf);
        }
    }

    temp_sf = setup_subfile_streamfile(sf, subfile_offset,subfile_size, "fsb");
    if (!temp_sf) goto fail;

    if (read_u32be(0x00, temp_sf) == 0x46534235) {
        vgmstream = init_vgmstream_fsb5(temp_sf);
        close_streamfile(temp_sf);
    }
    else { //other flag?
        vgmstream = init_vgmstream_fsb_encrypted(temp_sf);
        close_streamfile(temp_sf);
    }

    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
