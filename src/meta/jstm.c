#include "meta.h"
#include "../coding/coding.h"
#include "jstm_streamfile.h"


/* JSTM - from Tantei Jinguji Saburo - Kind of Blue (PS2) */
VGMSTREAM * init_vgmstream_jstm(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE *temp_streamFile = NULL;
    off_t start_offset;
    int loop_flag, channel_count;


    /* checks */
    /* .stm: original, .jstm: header id */
    if (!check_extensions(streamFile, "stm,jstm"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x4A53544D) /* "JSTM" */
        goto fail;

    start_offset = 0x20;
    channel_count = read_16bitLE(0x04,streamFile);
    if (channel_count != read_16bitLE(0x06,streamFile)) /* 0x02, interleave? */
        goto fail;
    loop_flag = (read_32bitLE(0x14,streamFile) != -1);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(0x08,streamFile);
    vgmstream->num_samples = pcm_bytes_to_samples(read_32bitLE(0x0C,streamFile),channel_count,16);
    if (loop_flag) {
        vgmstream->loop_start_sample = pcm_bytes_to_samples(read_32bitLE(0x14,streamFile),channel_count,16);
        vgmstream->loop_end_sample = vgmstream->num_samples;
    }

    vgmstream->meta_type = meta_PS2_JSTM;
    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x02;

    temp_streamFile = setup_jstm_streamfile(streamFile, start_offset);
    if (!temp_streamFile) goto fail;

    if (!vgmstream_open_stream(vgmstream,temp_streamFile,start_offset))
        goto fail;
    
    close_streamfile(temp_streamFile);
    
    return vgmstream;

fail:
    close_streamfile(temp_streamFile);
    close_vgmstream(vgmstream);
    return NULL;
}
