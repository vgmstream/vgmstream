#include "meta.h"
#include "../coding/coding.h"

/* FWSE - Capcom's MT Framework V1.x sound file */
VGMSTREAM *init_vgmstream_fwse(STREAMFILE *streamFile) {
    VGMSTREAM *vgmstream = NULL;
    uint32_t version, /*data_size,*/ buffer_offset;
    int channel_count, loop_flag, sample_count, sample_rate, loop_start, loop_end;


    /* checks*/
    /* .fwse: header id, no apparent extension in bigfiles */
    if (!check_extensions(streamFile,"fwse"))
        goto fail;

    if ((read_32bitBE(0x00,streamFile)) != 0x46575345) /* "FWSE" */
        goto fail;

    version = read_32bitLE(0x04,streamFile);
    /* v2: Resident Evil 5 (PC)
     * v3: Ace Attourney: Dual Destinies (Android) */
    if (version != 2 && version != 3)
        goto fail;

  //data_size = read_32bitLE(0x08,streamFile);
    buffer_offset = read_32bitLE(0x0C,streamFile); 
    channel_count = read_32bitLE(0x10,streamFile);

    if (channel_count > 2)
        goto fail;

    sample_count = read_32bitLE(0x14,streamFile);
    sample_rate = read_32bitLE(0x18,streamFile);
    loop_start = read_32bitLE(0x20,streamFile);
    loop_end = read_32bitLE(0x24,streamFile);
    loop_flag = (loop_start != -1);
    /* 0x28: some kind of setup? */
    /* 0x40: some kind of seek table with ADPCM hist/steps? */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_FWSE;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = sample_count;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;
    vgmstream->coding_type = coding_MTF_IMA;
    vgmstream->layout_type = layout_none;


    if (!vgmstream_open_stream(vgmstream,streamFile,buffer_offset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
