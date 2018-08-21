#include "meta.h"
#include "../coding/coding.h"

static STREAMFILE* setup_sps_streamfile(STREAMFILE *streamfile, off_t subfile_offset, size_t subfile_size, char* extension);

/* .SPS - Nippon Ichi's RIFF AT3 wrapper [ClaDun (PSP)] */
VGMSTREAM * init_vgmstream_sps_n1(STREAMFILE *streamFile) {
    VGMSTREAM *vgmstream = NULL;
    STREAMFILE *temp_streamFile = NULL;
    int type, sample_rate;
    off_t subfile_offset;
    size_t subfile_size;

    /* check extensions */
    if ( !check_extensions(streamFile,"sps"))
        goto fail;

    /* mini header */
    type = read_32bitLE(0x00,streamFile); //todo channels? all known VAG are mono and AT3 stereo
    subfile_size = read_32bitLE(0x04,streamFile);
    sample_rate = (uint16_t)read_16bitLE(0x08,streamFile);
    /* 0x0a/0b: stereo+loop flags? */
    //num_samples = read_32bitLE(0x0c,streamFile);
    subfile_offset = 0x10;

    /* init the VGMSTREAM */
    switch(type) {
        case 1: /* .vag */
            temp_streamFile = setup_sps_streamfile(streamFile, subfile_offset, subfile_size, "vag");
            if (!temp_streamFile) goto fail;

            vgmstream = init_vgmstream_vag(temp_streamFile);
            if (!vgmstream) goto fail;
            break;

        case 2: /* .at3 */
            temp_streamFile = setup_sps_streamfile(streamFile, subfile_offset, subfile_size, "at3");
            if (!temp_streamFile) goto fail;

            vgmstream = init_vgmstream_riff(temp_streamFile);
            if (!vgmstream) goto fail;
            break;
        default:
            goto fail;
    }

    vgmstream->sample_rate = sample_rate; /* .vag header doesn't match */

    close_streamfile(temp_streamFile);
    return vgmstream;

fail:
    close_streamfile(temp_streamFile);
    close_vgmstream(vgmstream);
    return NULL;
}

static STREAMFILE* setup_sps_streamfile(STREAMFILE *streamFile, off_t subfile_offset, size_t subfile_size, char* extension) {
    STREAMFILE *temp_streamFile = NULL, *new_streamFile = NULL;

    /* setup subfile */
    new_streamFile = open_wrap_streamfile(streamFile);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_clamp_streamfile(temp_streamFile, subfile_offset,subfile_size);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_fakename_streamfile(temp_streamFile, NULL,extension);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    return temp_streamFile;

fail:
    close_streamfile(temp_streamFile);
    return NULL;
}
