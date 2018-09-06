#include "meta.h"
#include "../coding/coding.h"

/* MSA - from Sucess games [Psyvariar -Complete Edition- (PS2), Konohana Pack: 3tsu no Jikenbo (PS2)]*/
VGMSTREAM * init_vgmstream_ps2_msa(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;
    size_t data_size, channel_size, file_size;


    /* checks */
    if (!check_extensions(streamFile, "msa"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x00000000)
        goto fail;
    if (read_32bitBE(0x08,streamFile) != 0x00000000)
        goto fail;

    loop_flag = 0;
    channel_count = 2;
    start_offset = 0x14;

    file_size = get_streamfile_size(streamFile);
    data_size = read_32bitLE(0x04,streamFile); /* wrong, see below */
    channel_size = read_32bitLE(0x0c,streamFile); /* also wrong like data_size */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_PS2_MSA;
    vgmstream->sample_rate = read_32bitLE(0x10,streamFile);
    if (vgmstream->sample_rate == 0) /* ex. Psyvariar's AME.MSA */
        vgmstream->sample_rate = 44100;
    vgmstream->num_samples = ps_bytes_to_samples(data_size, channel_count);

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;

    if (channel_size) /* Konohana Pack */
        vgmstream->interleave_block_size = 0x6000;
    else /* Psyvariar */
        vgmstream->interleave_block_size = 0x4000;
    /* (could also test first frame in various interleaves, as it's always 0'ed) */

    /* MSAs are strangely truncated, so manually calculate samples.
     * Data after last usable block is always silence or garbage. */
    if (data_size > file_size) {
        off_t usable_size = file_size - start_offset;
        usable_size -= usable_size % (vgmstream->interleave_block_size*channel_count);/* block-aligned */
        vgmstream->num_samples = ps_bytes_to_samples(usable_size, channel_count);//usable_size * 28 / (16*channel_count);
    }

    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
