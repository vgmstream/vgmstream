#include "meta.h"
#include "../coding/coding.h"

/* FEV+FSB5 container [Just Cause 3 (PC), Shantae: Half-Genie Hero (Switch)] */
VGMSTREAM* init_vgmstream_fsb5_fev_bank(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    off_t subfile_offset, chunk_offset, bank_offset, offset;
    size_t subfile_size, bank_size;
    int is_old = 0;


    /* checks */
    if (!check_extensions(sf, "bank"))
        goto fail;

    if (read_u32be(0x00,sf) != 0x52494646) /* "RIFF" */
        goto fail;
    if (read_u32be(0x08,sf) != 0x46455620) /* "FEV " */
        goto fail;

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
            case 0x4C495354: /* "LIST" with "SNDH" (older) */
                if (read_u32be(offset + 0x04, sf) == 0x534E4448) {
                    bank_offset = offset + 0x0c;
                    bank_size = read_s32le(offset + 0x08,sf);
                    is_old = 1;
                }
                break;

            case 0x534E4448: /* "SNDH" (newer) */
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

    /* 0x00: unknown (version? ex LE: 0x00080003, 0x00080005) */
    {
        int banks;
        size_t entry_size = is_old ? 0x04 : 0x08;

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

        if (is_old) {
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

    vgmstream = init_vgmstream_fsb5(temp_sf);
    close_streamfile(temp_sf);

    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
