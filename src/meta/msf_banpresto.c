#include "meta.h"
#include "../coding/coding.h"

/* WMSF - Banpresto MSFx wrapper [Dai-2-Ji Super Robot Taisen OG: The Moon Dwellers (PS3)] */
VGMSTREAM * init_vgmstream_msf_banpresto_wmsf(STREAMFILE *streamFile) {
    VGMSTREAM *vgmstream = NULL;
    STREAMFILE *temp_streamFile = NULL;
    off_t subfile_offset = 0x10;
    size_t subfile_size = get_streamfile_size(streamFile) - subfile_offset;


    /* checks */
    if ( !check_extensions(streamFile,"msf"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x574D5346) /* "WMSF" */
        goto fail;
    /* 0x04: size, 0x08: flags? 0x0c: null? */

    temp_streamFile = setup_subfile_streamfile(streamFile, subfile_offset,subfile_size, NULL);
    if (!temp_streamFile) goto fail;

    vgmstream = init_vgmstream_msf(temp_streamFile);
    if (!vgmstream) goto fail;

    close_streamfile(temp_streamFile);
    return vgmstream;

fail:
    close_streamfile(temp_streamFile);
    close_vgmstream(vgmstream);
    return NULL;
}

/* 2MSF - Banpresto RIFF wrapper [Dai-2-Ji Super Robot Taisen OG: The Moon Dwellers (PS4)] */
VGMSTREAM * init_vgmstream_msf_banpresto_2msf(STREAMFILE *streamFile) {
    VGMSTREAM *vgmstream = NULL;
    STREAMFILE *temp_streamFile = NULL;
    off_t subfile_offset = 0x14;
    size_t subfile_size = get_streamfile_size(streamFile) - subfile_offset;


    /* checks */
    if ( !check_extensions(streamFile,"at9"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x324D5346) /* "2MSF" */
        goto fail;
    /* 0x04: size, 0x08: flags? 0x0c: null?, 0x10: 0x01? (BE values even though RIFF is LE) */

    temp_streamFile = setup_subfile_streamfile(streamFile, subfile_offset,subfile_size, NULL);
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
