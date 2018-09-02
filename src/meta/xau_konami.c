#include "meta.h"
#include "../coding/coding.h"

/* XAU - from Konami games [Yu-Gi-Oh - The Dawn of Destiny (Xbox)] */
VGMSTREAM * init_vgmstream_xau_konami(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, header_offset;
    size_t stream_size;
    int loop_flag, channel_count, sample_rate;
    off_t loop_start, loop_end;


    /* checks */
    if ( !check_extensions(streamFile,"xau") )
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x53465842) /* "SFXB" */
        goto fail;

    //todo: subsongs used in sfx packs (rare)
    if (read_32bitLE(0x54,streamFile) != 1) /* subsong count */
        goto fail;

    start_offset = 0x60 + read_32bitLE(0x34,streamFile);
    header_offset = 0x60 + 0x20 + 0x40*0; /* target subsong */

    if (read_32bitBE(header_offset+0x00,streamFile) != 0x52494646) /* "RIFF" */
        goto fail;
    if (read_16bitLE(header_offset+0x14,streamFile) != 0x01) /* codec */
        goto fail;
    channel_count = read_16bitLE(header_offset+0x16,streamFile);
    sample_rate   = read_32bitLE(header_offset+0x18,streamFile);
    loop_start    = read_32bitLE(header_offset+030,streamFile);
    loop_end      = read_32bitLE(header_offset+0x34,streamFile);
    loop_flag = (loop_end > 0);

    stream_size = get_streamfile_size(streamFile) - start_offset;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_XAU_KONAMI;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = pcm_bytes_to_samples(stream_size,channel_count,16);
    vgmstream->loop_start_sample = pcm_bytes_to_samples(loop_start,channel_count,16);
    vgmstream->loop_end_sample = pcm_bytes_to_samples(loop_end,channel_count,16);

    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x02;

    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
