#include "meta.h"
#include "../coding/coding.h"

/* FEV+FSB5 container [Just Cause 3 (PC), Shantae: Half-Genie Hero (Switch)] */
VGMSTREAM * init_vgmstream_fsb5_fev_bank(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE *temp_streamFile = NULL;
    off_t subfile_offset, chunk_offset, first_offset = 0x0c;
    size_t subfile_size, chunk_size;


    /* checks */
    if (!check_extensions(streamFile, "bank"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x52494646) /* "RIFF" */
        goto fail;
    if (read_32bitBE(0x08,streamFile) != 0x46455620) /* "FEV " */
        goto fail;

    /* .fev is an event format referencing various external .fsb, but FMOD can bake .fev and .fsb to
     * form a .bank, which is the format we support here (regular .fev is complex and not very interesting).
     * Format is RIFF with FMT (main), LIST (config) and SND (FSB5 data), we want the FSB5 offset inside LIST */
    if (!find_chunk_le(streamFile, 0x4C495354,first_offset,0, &chunk_offset,NULL)) /* "LIST" */
        goto fail;

    if (read_32bitBE(chunk_offset+0x00,streamFile) != 0x50524F4A || /* "PROJ" */
        read_32bitBE(chunk_offset+0x04,streamFile) != 0x424E4B49)   /* "BNKI" */
        goto fail; /* event .fev has "OBCT" instead of "BNKI" */

    /* inside BNKI is a bunch of LIST each with event subchunks and finally the fsb offset */
    first_offset = chunk_offset + 0x04;
    if (!find_chunk_le(streamFile, 0x534E4448,first_offset,0, &chunk_offset,&chunk_size)) /* "SNDH" */
        goto fail;

    if (chunk_size != 0x0c)
        goto fail; /* assuming only one FSB5 is possible */
    subfile_offset  = read_32bitLE(chunk_offset+0x04,streamFile);
    subfile_size    = read_32bitLE(chunk_offset+0x08,streamFile);


    temp_streamFile = setup_subfile_streamfile(streamFile, subfile_offset,subfile_size, "fsb");
    if (!temp_streamFile) goto fail;

    vgmstream = init_vgmstream_fsb5(temp_streamFile);
    close_streamfile(temp_streamFile);

    return vgmstream;

fail:
    close_streamfile(temp_streamFile);
    close_vgmstream(vgmstream);
    return NULL;
}
