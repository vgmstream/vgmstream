#include "meta.h"
#include "../coding/coding.h"


/* BKHD - Wwise soundbank container */
VGMSTREAM* init_vgmstream_bkhd(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    off_t subfile_offset, didx_offset, data_offset, offset;
    size_t subfile_size, didx_size;
    uint32_t subfile_id;
    int big_endian;
    uint32_t (*read_u32)(off_t,STREAMFILE*);
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    if (!check_extensions(sf,"bnk"))
        goto fail;
    if (read_u32be(0x00, sf) != 0x424B4844) /* "BKHD" */
        goto fail;
    big_endian = guess_endianness32bit(0x04, sf);
    read_u32 = big_endian ? read_u32be : read_u32le;

    /* Wwise banks have event/track/sequence/etc info in the HIRC chunk, as well
     * as other chunks, and may have a DIDX index to memory .wem in DATA.
     * We support the internal .wem mainly for quick tests, as the HIRC is
     * complex and better handled with TXTP (some info from Nicknine's script) */

    /* unlike RIFF first chunk follows chunk rules */
    if (!find_chunk(sf, 0x44494458, 0x00,0, &didx_offset, &didx_size, big_endian, 0)) /* "DIDX" */
        goto fail;
    if (!find_chunk(sf, 0x44415441, 0x00,0, &data_offset, NULL, big_endian, 0)) /* "DATA" */
        goto fail;

    total_subsongs = didx_size / 0x0c;
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    offset = didx_offset + (target_subsong - 1) * 0x0c;
    subfile_id      = read_u32(offset + 0x00, sf);
    subfile_offset  = read_u32(offset + 0x04, sf) + data_offset;
    subfile_size    = read_u32(offset + 0x08, sf);


    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, "wem");
    if (!temp_sf) goto fail;

    vgmstream = init_vgmstream_wwise(temp_sf);
    if (!vgmstream) goto fail;

    vgmstream->num_streams = total_subsongs;
    snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%i", subfile_id);

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
