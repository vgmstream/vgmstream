#include "meta.h"
#include "../coding/coding.h"

static STREAMFILE* setup_atsl3_streamfile(STREAMFILE *streamFile, off_t subfile_offset, size_t subfile_size);

/* .ATSL3 - Koei Tecmo container of multiple .AT3 [One Piece Pirate Warriors (PS3)] */
VGMSTREAM * init_vgmstream_atsl3(STREAMFILE *streamFile) {
    VGMSTREAM *vgmstream = NULL;
    STREAMFILE *temp_streamFile = NULL;
    int total_subsongs, target_subsong = streamFile->stream_index;
    off_t subfile_offset;
    size_t subfile_size, header_size, entry_size;

    /* check extensions */
    if ( !check_extensions(streamFile,"atsl3"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x4154534C) /* "ATSL" */
        goto fail;

    /* main header (LE) */
    header_size = read_32bitLE(0x04,streamFile);
    /* 0x08/0c: flags?, 0x10: some size? */
    total_subsongs = read_32bitLE(0x14,streamFile);
    entry_size = read_32bitLE(0x18,streamFile);
    /* 0x1c: null, 0x20: subheader size, 0x24/28: null */

    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong > total_subsongs || total_subsongs <= 0) goto fail;

    /* entry header (BE) */
    /* 0x00: id */
    subfile_offset = read_32bitBE(header_size + (target_subsong-1)*entry_size + 0x04,streamFile);
    subfile_size   = read_32bitBE(header_size + (target_subsong-1)*entry_size + 0x08,streamFile);
    /* 0x08+: sample rate/num_samples/loop_start/etc, matching subfile header */
    /* some kind of seek/switch table follows */

    temp_streamFile = setup_atsl3_streamfile(streamFile, subfile_offset,subfile_size);
    if (!temp_streamFile) goto fail;

    /* init the VGMSTREAM */
    vgmstream = init_vgmstream_riff(temp_streamFile);
    if (!vgmstream) goto fail;
    vgmstream->num_streams = total_subsongs;

    close_streamfile(temp_streamFile);
    return vgmstream;

fail:
    close_streamfile(temp_streamFile);
    close_vgmstream(vgmstream);
    return NULL;
}


static STREAMFILE* setup_atsl3_streamfile(STREAMFILE *streamFile, off_t subfile_offset, size_t subfile_size) {
    STREAMFILE *temp_streamFile = NULL, *new_streamFile = NULL;

    /* setup subfile */
    new_streamFile = open_wrap_streamfile(streamFile);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_clamp_streamfile(temp_streamFile, subfile_offset,subfile_size);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_fakename_streamfile(temp_streamFile, NULL,"at3");
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    return temp_streamFile;

fail:
    close_streamfile(temp_streamFile);
    return NULL;
}
