#include "meta.h"
#include "../util.h"

/* SND - Might and Magic games [Warriors of M&M (PS2), Heroes of M&M: Quest for the DragonBone Staff (PS2)] */
VGMSTREAM * init_vgmstream_ps2_snd(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    size_t data_size;
    int loop_flag, channel_count;

    /* checks */
    if (!check_extensions(streamFile, "snd"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x53534E44) /* "SSND" */
        goto fail;

    start_offset = read_32bitLE(0x04,streamFile)+0x08;
    data_size = get_streamfile_size(streamFile) - start_offset;

    loop_flag = 1; /* force full Loop */
    channel_count = read_16bitLE(0x0a,streamFile);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = (uint16_t)read_16bitLE(0x0e,streamFile);
    vgmstream->num_samples = read_32bitLE(0x16,streamFile);
    vgmstream->loop_start_sample = 0;
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->meta_type = meta_PS2_SND;

    if (read_8bit(0x08,streamFile)==1) {
        vgmstream->coding_type = coding_DVI_IMA_int; /* Warriors of M&M DragonBone */
    }
    else {
        vgmstream->coding_type = coding_PCM16LE; /* Heroes of M&M */
    }
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = (uint16_t)read_16bitLE(0x12,streamFile);
    if (vgmstream->interleave_block_size)
        vgmstream->interleave_last_block_size = (data_size % (vgmstream->interleave_block_size*vgmstream->channels)) / vgmstream->channels;


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
