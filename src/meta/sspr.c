#include "meta.h"
#include "../coding/coding.h"

/* SSPR - Capcom container [Sengoku Basara 4 (PS3/PS4), Mega Man Zero ZX Legacy Collection (PS4)] */
VGMSTREAM* init_vgmstream_sspr(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    uint32_t name_offset, subfile_offset, subfile_size, name_size;
    int big_endian;
    int total_subsongs, target_subsong = sf->stream_index;
    char* extension;
    uint32_t (*read_u32)(off_t,STREAMFILE*) = NULL;


    /* checks */
    if (!check_extensions(sf,"sspr"))
        goto fail;
    if (!is_id32be(0x00,sf,"SSPR"))
        goto fail;

    /* Simple (audio only) container used some Capcom games (common engine?).
     * Some files come with a .stqr with unknown data (cues?). */

    big_endian = guess_endianness32bit(0x04, sf); /* 0x01 (version?) */
    read_u32 = big_endian ? read_u32be : read_u32le;

    total_subsongs = read_u32(0x08,sf);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;
    /* 0x0c: null */

    name_offset     = read_u32(0x10 + (target_subsong-1) * 0x10 + 0x00,sf);
    subfile_offset  = read_u32(0x10 + (target_subsong-1) * 0x10 + 0x04,sf);
    name_size       = read_u32(0x10 + (target_subsong-1) * 0x10 + 0x08,sf);
    subfile_size    = read_u32(0x10 + (target_subsong-1) * 0x10 + 0x0c,sf);

    extension = big_endian ? "at3" : "at9";

    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, extension);
    if (!temp_sf) goto fail;

    vgmstream = init_vgmstream_riff(temp_sf);
    if (!vgmstream) goto fail;

    vgmstream->num_streams = total_subsongs;
    read_string(vgmstream->stream_name,name_size+1, name_offset,sf);

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
