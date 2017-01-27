#include "meta.h"
#include "../util.h"

/* MSA (from Psyvariar -Complete Edition-) */
VGMSTREAM * init_vgmstream_ps2_msa(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, datasize, filesize;
    int loop_flag, channel_count;

    /* check extension, case insensitive */
    if (!check_extensions(streamFile, "msa")) goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x00000000)
        goto fail;

    loop_flag = 0;
    channel_count = 2;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    start_offset = 0x14;
    datasize = read_32bitLE(0x4,streamFile);
    filesize = get_streamfile_size(streamFile);
    vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x10,streamFile);
    vgmstream->num_samples = datasize*28/(16*channel_count);
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x4000;
    vgmstream->meta_type = meta_PS2_MSA;

    /* MSAs are strangely truncated, so manually calculate samples
     *  data after last usable block is always silence or garbage */
    if (datasize > filesize) {
        off_t usable_size = filesize - start_offset;
        usable_size -= usable_size % (vgmstream->interleave_block_size*channel_count);/* block-aligned */
        vgmstream->num_samples = usable_size * 28 / (16*channel_count);
    }


    /* open the file for reading */
    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
