#include "meta.h"
#include "../coding/coding.h"
#include "sfh_streamfile.h"


/* .SFH - Capcom wrapper [Devil May Cry 4 Demo (PS3), Jojo's Bizarre Adventure HD (PS3)] */
VGMSTREAM * init_vgmstream_sfh(STREAMFILE *streamFile) {
    VGMSTREAM *vgmstream = NULL;
    STREAMFILE *temp_streamFile = NULL;
    uint32_t version;
    size_t clean_size, block_size;

    /* check extensions */
    if ( !check_extensions(streamFile,"at3"))
        goto fail;

    if (read_32bitBE(0x00, streamFile) != 0x00534648) /* "\0SFH" */
        goto fail;
    if (read_32bitBE(0x10, streamFile) != 0x52494646) /* "RIFF" */
        goto fail;

    /* mini header */
    version     = read_32bitBE(0x04,streamFile);
    clean_size  = read_32bitBE(0x08,streamFile); /* there is padding data at the end */
    /* 0x0c: always 0 */

    switch(version) {
        case 0x00010000: block_size = 0x10010; break; /* DMC4 Demo (not retail) */
        case 0x00010001: block_size = 0x20000; break; /* Jojo */
        default: goto fail;
    }

    temp_streamFile = setup_sfh_streamfile(streamFile, 0x00, block_size, clean_size, "at3");
    if (!temp_streamFile) goto fail;

    vgmstream = init_vgmstream_riff(temp_streamFile);
    if (!vgmstream) goto fail;

    close_streamfile(temp_streamFile);
    return vgmstream;

fail:
    close_streamfile(temp_streamFile);
    close_vgmstream(vgmstream);
    return NULL;
}
