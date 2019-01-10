#include "meta.h"
#include "../util.h"

/* SD9 (found in beatmania IIDX Arcade games) */
VGMSTREAM * init_vgmstream_sd9(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;

    /* check extension */
    if (!check_extensions(streamFile, "sd9"))
        goto fail;

    /* check header */
    if (read_32bitBE(0x0, streamFile) != 0x53443900) /* SD9 */
        goto fail;
    if (read_32bitBE(0x20, streamFile) != 0x52494646) /* RIFF */
        goto fail;
    if (read_32bitBE(0x28, streamFile) != 0x57415645) /* WAVE */
        goto fail;
    if (read_32bitBE(0x2c, streamFile) != 0x666D7420) /* fmt */
        goto fail;
    if (read_32bitBE(0x72, streamFile) != 0x64617461) /* data */
        goto fail;

    /* Probably better to check if loop end exists and use as loop flag.
       Blame SD9s from beatmania IIDX 21: Spada that have a flase flag
       but still "loop" */

    //loop_flag = (read_16bitLE(0x0e,streamFile)==0x1);
    loop_flag = read_32bitLE(0x18, streamFile); // use loop end
    channel_count = read_16bitLE(0x36, streamFile);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    start_offset = 0x7a;
    vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x38, streamFile);
    vgmstream->coding_type = coding_MSADPCM;
    vgmstream->num_samples = read_32bitLE(0x6e, streamFile);
    if (loop_flag) {
        vgmstream->loop_start_sample = read_32bitLE(0x14, streamFile) / 2 / channel_count;
        vgmstream->loop_end_sample = read_32bitLE(0x18, streamFile) / 2 / channel_count;
    }

    vgmstream->layout_type = layout_none;
    vgmstream->interleave_block_size = read_16bitLE(0x40, streamFile);
    vgmstream->meta_type = meta_SD9;

    /* open the file for reading */
    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}