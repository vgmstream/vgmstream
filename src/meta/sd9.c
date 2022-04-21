#include "meta.h"
#include "../coding/coding.h"

/* SD9 - from Konami arcade games [beatmania IIDX series (AC), BeatStream (AC)] */
VGMSTREAM * init_vgmstream_sd9(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;

    /* checks */
    if (!check_extensions(streamFile, "sd9"))
        goto fail;

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

    /* Some SD9s may loop without any loop points specificed.
       If loop_flag is set with no points, loop entire song. */

    loop_flag = read_16bitLE(0x0e,streamFile);
    //loop_flag = read_32bitLE(0x18, streamFile); // use loop end
    channel_count = read_16bitLE(0x36, streamFile);
    start_offset = 0x7a;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(0x38, streamFile);
    vgmstream->num_samples = read_32bitLE(0x6e, streamFile);
    if (loop_flag > 0) {
        vgmstream->loop_start_sample = read_32bitLE(0x14, streamFile) / 2 / channel_count;
        vgmstream->loop_end_sample = read_32bitLE(0x18, streamFile) / 2 / channel_count;
        if (vgmstream->loop_end_sample == 0) {
            vgmstream->loop_end_sample = vgmstream->num_samples;
        }
    }

    /* beatmania IIDX 21: Spada is a special case. Loop flag is false but loops exist.
       Konami, Why? */
    if ((loop_flag < 0) && (read_32bitLE(0x18, streamFile) !=0)) {
        vgmstream->loop_start_sample = read_32bitLE(0x14, streamFile) / 2 / channel_count;
        vgmstream->loop_end_sample = read_32bitLE(0x18, streamFile) / 2 / channel_count;
    }

    vgmstream->coding_type = coding_MSADPCM;
    vgmstream->layout_type = layout_none;
    vgmstream->frame_size = read_16bitLE(0x40, streamFile);
    vgmstream->meta_type = meta_SD9;
    if (!msadpcm_check_coefs(streamFile, 0x48))
        goto fail;

    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
