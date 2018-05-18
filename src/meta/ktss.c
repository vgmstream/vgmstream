#include "meta.h"
#include "../util.h"
#include "../stack_alloc.h"
#include "../coding/coding.h"

VGMSTREAM * init_vgmstream_ktss(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    int loop_flag, channel_count;
    int8_t version;
    int32_t loop_length, coef_start_offset, coef_spacing;
    off_t start_offset;
    int8_t channelMultiplier;

    if (!check_extensions(streamFile, "kns,ktss"))
        goto fail;

    if (read_32bitBE(0, streamFile) != 0x4B545353) /* "KTSS" */
        goto fail;

    /* check type details */
    version = read_8bit(0x22, streamFile);
    if (version == 1) {
        coef_start_offset = 0x40;
        coef_spacing = 0x2e;
    }
    else if (version == 3) { // Fire Emblem Warriors (Switch)
        coef_start_offset = 0x5c;
        coef_spacing = 0x60;
    }
    else
        goto fail;

    loop_length = read_32bitLE(0x38, streamFile);
    loop_flag = loop_length > 0;

    // For unknown reasons, a channel multiplier is necessary in Hyrule Warriors (Switch)
    // It seems to be present in other Koei Tecmo KNS but the channel count was always
    // explicitly defined in the 0x29 byte. Here, 10 channel files have '2' in 0x29*
    // and '5' in 0x28 whereas previous titles usually contained '1'
    // This is super meh on KT's part but whatever
    channelMultiplier = read_8bit(0x28, streamFile);

    channel_count = read_8bit(0x29, streamFile) * channelMultiplier;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->num_samples = read_32bitLE(0x30, streamFile);
    vgmstream->sample_rate = (uint16_t)read_16bitLE(0x2c, streamFile);
    vgmstream->loop_start_sample = read_32bitLE(0x34, streamFile);
    vgmstream->loop_end_sample = vgmstream->loop_start_sample + loop_length;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->meta_type = meta_KTSS;

    vgmstream->interleave_block_size = 0x8;

    dsp_read_coefs_le(vgmstream, streamFile, coef_start_offset, coef_spacing);
    start_offset = read_32bitLE(0x24, streamFile) + 0x20;

    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
