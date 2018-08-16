#include "meta.h"
#include "../coding/coding.h"


/* EXST - from Sony games [Shadow of the Colossus (PS2), Gacha Mecha Stadium Saru Battle (PS2)] */
VGMSTREAM * init_vgmstream_ps2_exst(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamBody = NULL;
    off_t start_offset;
    int loop_flag, channel_count;
    size_t block_size, num_blocks, loop_start_block;


    /* checks */
    /* .sts+int: main [Shadow of the Colossus (PS2)] (some .sts have manually joined header+body)
     * .x: header+body [Ape Escape 3 (PS2)] */
    if (!check_extensions(streamFile, "sts,x"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x45585354) /* "EXST" */
        goto fail;

    streamBody = open_streamfile_by_ext(streamFile,"int");
    if (!streamBody) {
        /* data+body joined */
        start_offset = 0x78;
        if (get_streamfile_size(streamFile) < start_offset)
            goto fail;
    }
    else {
        /* body is separate */
        start_offset = 0x00;
    }


    channel_count = read_16bitLE(0x06,streamFile);
    loop_flag = read_32bitLE(0x0C,streamFile) == 1;
    loop_start_block = read_32bitLE(0x10,streamFile);
    num_blocks = read_32bitLE(0x14,streamFile);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(0x08,streamFile);
    vgmstream->meta_type = meta_PS2_EXST;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x400;

    block_size = vgmstream->interleave_block_size * vgmstream->channels;
    vgmstream->num_samples = ps_bytes_to_samples(num_blocks*block_size, channel_count);
    if (vgmstream->loop_flag) {
        vgmstream->loop_start_sample = ps_bytes_to_samples(loop_start_block*block_size, channel_count);;
        vgmstream->loop_end_sample = vgmstream->num_samples;
    }

    if (!vgmstream_open_stream(vgmstream,streamBody ? streamBody : streamFile,start_offset))
        goto fail;
    close_streamfile(streamBody);
    return vgmstream;

fail:
    close_streamfile(streamBody);
    close_vgmstream(vgmstream);
    return NULL;
}
