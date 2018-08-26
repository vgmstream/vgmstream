#include "meta.h"
#include "../coding/coding.h"


/* .PCM - from Lunar: Eternal Blue (Sega CD) */
VGMSTREAM * init_vgmstream_scd_pcm(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;


    /* checks */
    if (!check_extensions(streamFile, "pcm"))
        goto fail;

    if (read_16bitBE(0x00,streamFile) == 0x0002) {
        channel_count = 1;
    }
    else if (read_16bitBE(0x00,streamFile) == 0x0001) {
        channel_count = 2; /* RP025.PCM, RP039.PCM */
    }
    else {
        goto fail;
    }

    start_offset = 0x800;


    /* extra validations since .pcm it's kinda generic */
    {
        off_t i;
        /* should be empty up to start (~0x0a/~0x10 sometimes has unknown values) */
        for (i = 0x20; i < start_offset; i++) {
            if (read_8bit(i, streamFile) != 0)
                goto fail;
        }
    }

    /* loops start 0 is possible, plus all? files loops
     * (even sfx/voices loop, but those set loop start in silence near the end) */
    loop_flag = (read_32bitBE(0x06,streamFile) != 0);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SCD_PCM;
    vgmstream->sample_rate = 32500; /* looks correct compared to emu/recordings */
    vgmstream->num_samples = pcm_bytes_to_samples(get_streamfile_size(streamFile) - start_offset, channel_count, 8);
    vgmstream->loop_start_sample = read_32bitBE(0x02,streamFile)*0x400*2;
    vgmstream->loop_end_sample = read_32bitBE(0x06,streamFile)*2;

    vgmstream->coding_type = coding_PCM8_SB;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x800;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
