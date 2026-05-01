#include "meta.h"
#include "../coding/coding.h"
#include "../util/chunks.h"
#include "../util/endianness.h"


/* PLUG - Wwise (~2025.x) plugin container */
VGMSTREAM* init_vgmstream_plug(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;


    /* checks */
    if (!is_id32be(0x00, sf, "PLUG"))
        return NULL;
    if (!check_extensions(sf,"wem"))
        return NULL;

    /* Simple plugin container with hash/junk/data chunks */
    off_t subfile_offset = 0;
    size_t subfile_size = 0;
    if (!find_chunk_riff_le(sf, get_id32be("data"), 0x08, 0, &subfile_offset, &subfile_size))
        return NULL;

    //;VGM_LOG("PLUG: %x, %x\n", (int)subfile_offset, (int)subfile_size);

    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, NULL);
    if (!temp_sf) goto fail;

    if (is_id32be(0x00, temp_sf, "ADM3")) { // Screamer (PC)
        vgmstream = init_vgmstream_adm3(temp_sf);
        if (!vgmstream) goto fail;
    }
    else {
        // Aphelion (PC)
        vgmstream = init_vgmstream_bkhd_fx(temp_sf);
        if (!vgmstream) goto fail;

        //TODO: handle Oblivion Remake PLUG (unknown fx)
    }

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
