#include "meta.h"
#include "../util.h"

/* STRM - from Final Fantasy Tactics A2 (NDS) */
VGMSTREAM * init_vgmstream_nds_strm_ffta2(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;

    /* check extension, case insensitive (the ROM seems to use simply .bin though) */
    if (!check_extensions(streamFile,"strm"))
        goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x52494646 ||  /* "RIFF" */
        read_32bitBE(0x08,streamFile) != 0x494D4120)    /* "IMA " */
        goto fail;

    loop_flag = (read_32bitLE(0x20,streamFile) !=0);
    channel_count = read_32bitLE(0x24,streamFile);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    start_offset = 0x2C;
    vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x0C,streamFile);
    vgmstream->coding_type = coding_IMA_int; //todo: seems it has some diffs vs regular IMA
    vgmstream->num_samples = (read_32bitLE(0x04,streamFile)-start_offset);
    if (loop_flag) {
        vgmstream->loop_start_sample = read_32bitLE(0x20,streamFile);
        vgmstream->loop_end_sample = read_32bitLE(0x28,streamFile);
    }

    vgmstream->interleave_block_size = 0x80;
    vgmstream->layout_type = layout_interleave;
    vgmstream->meta_type = meta_NDS_STRM_FFTA2;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
