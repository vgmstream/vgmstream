#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"


/* .ADX - from Capcom games [Resident Evil: Revelations (Switch), Monster Hunter XX (Switch)] */
VGMSTREAM * init_vgmstream_adpcm_capcom(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;


    /* checks */
    /* .mca: Monster Hunter Generations Ultimate / XX */
    if (!check_extensions(streamFile,"adpcm,mca"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x02000000)
        goto fail;

    channel_count = read_16bitLE(0x04, streamFile);
    if (channel_count > 2) goto fail; /* header size seems fixed for mono/stereo */
    /* 0x08: channel size */
    /* 0x0c-14: some config/id? (may be shared between files) */
    loop_flag = read_16bitLE(0x68, streamFile);

    start_offset = 0xd8; /* also fixed for mono/stereo */


    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_ADPCM_CAPCOM;
    vgmstream->sample_rate = read_32bitLE(0x64,streamFile); /* from first headerm repeated at +0x60 */
    vgmstream->num_samples = read_32bitLE(0x60, streamFile);
    vgmstream->loop_start_sample = read_32bitLE(0x6c, streamFile);
    vgmstream->loop_end_sample   = read_32bitLE(0x70, streamFile) + 1;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_16bitLE(0x06, streamFile);
    dsp_read_coefs_le(vgmstream,streamFile, 0x18, 0x60);

    if (!vgmstream_open_stream(vgmstream,streamFile, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
