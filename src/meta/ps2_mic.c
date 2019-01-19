#include "meta.h"
#include "../coding/coding.h"

/* .MIC - from KOEI games [Crimson Sea 2 (PS2), Dynasty Tactics 2 (PS2)] */
VGMSTREAM * init_vgmstream_ps2_mic(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count, loop_start, loop_end, sample_rate;
    size_t interleave, block_size;


    /* checks */
    if (!check_extensions(streamFile, "mic"))
        goto fail;

    start_offset  = read_32bitLE(0x00,streamFile);
    if (start_offset != 0x800) goto fail;
    sample_rate   = read_32bitLE(0x04,streamFile);
    channel_count = read_32bitLE(0x08,streamFile);
    interleave    = read_32bitLE(0x0c,streamFile);
    loop_end      = read_32bitLE(0x10,streamFile);
    loop_start    = read_32bitLE(0x14,streamFile);
    loop_flag     = (loop_start != 1);
    block_size    = interleave * channel_count;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_PS2_MIC;
    vgmstream->sample_rate = sample_rate;

    vgmstream->coding_type = coding_PSX;
    vgmstream->interleave_block_size = interleave;
    vgmstream->layout_type = layout_interleave;

    vgmstream->num_samples = ps_bytes_to_samples(loop_end * block_size, channel_count);
    vgmstream->loop_start_sample = ps_bytes_to_samples(loop_start * block_size, channel_count);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
