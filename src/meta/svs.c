#include "meta.h"
#include "../coding/coding.h"


/* SVS - SeqVagStream from Square games [Unlimited Saga (PS2) music] */
VGMSTREAM * init_vgmstream_svs(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int channel_count, loop_flag, pitch;


    /* checks */
    /* .svs: header id (probably ok like The Bouncer's .vs) */
    if (!check_extensions(streamFile, "svs"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x53565300) /* "SVS\0" */
        goto fail;

    /* 0x04: flags (1=stereo?, 2=loop) */
    pitch = read_32bitLE(0x10,streamFile); /* usually 0x1000 = 48000 */
    /* 0x14: volume? */
    /* 0x18: file id (may be null) */
    /* 0x1c: null */

    loop_flag = (read_32bitLE(0x08,streamFile) > 0); /* loop start frame, min is 1 */
    channel_count = 2;
    start_offset = 0x20;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SVS;
    vgmstream->sample_rate = round10((48000 * pitch) / 4096); /* music = ~44100, ambience = 48000 (rounding makes more sense but not sure) */
    vgmstream->num_samples = ps_bytes_to_samples(get_streamfile_size(streamFile) - start_offset, channel_count);
    if (loop_flag) {
        vgmstream->loop_start_sample = read_32bitLE(0x08,streamFile) * 28; /* frame count (0x10*ch) */
        vgmstream->loop_end_sample = read_32bitLE(0x0c,streamFile) * 28; /* frame count, (not exact num_samples when no loop) */
        /* start/end on the same frame rarely happens too (ex. file_id 63 SVS), perhaps loop should be +1 */
    }

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x10;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
