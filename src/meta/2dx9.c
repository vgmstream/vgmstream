#include "meta.h"
#include "../coding/coding.h"

/* 2DX9 - from Konami arcade games [beatmaniaIIDX16: EMPRESS (AC), BeatStream (AC), REFLEC BEAT (AC)] */
VGMSTREAM * init_vgmstream_2dx9(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int channel_count, loop_flag;


    /* checks */
    if (!check_extensions(streamFile, "2dx9"))
        goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x32445839) /* 2DX9 */
        goto fail;
    if (read_32bitBE(0x18,streamFile) != 0x52494646) /* RIFF */
        goto fail;
    if (read_32bitBE(0x20,streamFile) != 0x57415645) /* WAVE */
        goto fail;
    if (read_32bitBE(0x24,streamFile) != 0x666D7420) /* fmt */
        goto fail;
    if (read_32bitBE(0x6a,streamFile) != 0x64617461) /* data */
        goto fail;

    /* Some data loop from beginning to the end by hardcoded flag so cannot be recognized from sound file */
    loop_flag = (read_32bitLE(0x14,streamFile) > 0);
    channel_count = read_16bitLE(0x2e,streamFile);
    start_offset = 0x72;
    
    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_2DX9;
    vgmstream->sample_rate = read_32bitLE(0x30,streamFile);
    vgmstream->num_samples = read_32bitLE(0x66,streamFile);
    if (loop_flag) {
        vgmstream->loop_start_sample = read_32bitLE(0x14,streamFile) / 2 / channel_count;
        vgmstream->loop_end_sample = vgmstream->num_samples;
    }

    vgmstream->coding_type = coding_MSADPCM;
    vgmstream->layout_type = layout_none;
    vgmstream->frame_size  = read_16bitLE(0x38,streamFile);
    if (!msadpcm_check_coefs(streamFile, 0x40))
        goto fail;

    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
