#include "meta.h"
#include "../coding/coding.h"

VGMSTREAM * init_vgmstream_ktss(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    int loop_flag, channel_count;
    int8_t version, num_layers, codec_id;
    int32_t loop_length, coef_start_offset, coef_spacing;
    off_t start_offset;
    size_t data_size, skip = 0;

    if (!check_extensions(streamFile, "kns,ktss"))
        goto fail;

    if (read_32bitBE(0, streamFile) != 0x4B545353) /* "KTSS" */
        goto fail;

    codec_id = read_8bit(0x20, streamFile);
    loop_length = read_32bitLE(0x38, streamFile);
    loop_flag = loop_length > 0;

    // A layered stream/track model seems to be used in Hyrule Warriors (Switch).
    // It's also present in other Koei Tecmo KNS but the channel count was always
    // explicitly defined in the 0x29 byte and the number of layers was set to 1.
    // Here, 10 channel files are set up with 2 channels in 5 layers.
    // Super hacky on KT's part and ours to implement but it works.
    num_layers = read_8bit(0x28, streamFile);

    channel_count = read_8bit(0x29, streamFile) * num_layers;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->num_samples = read_32bitLE(0x30, streamFile);
    vgmstream->sample_rate = read_32bitLE(0x2c, streamFile);
    vgmstream->loop_start_sample = read_32bitLE(0x34, streamFile);
    vgmstream->loop_end_sample = vgmstream->loop_start_sample + loop_length;
    vgmstream->meta_type = meta_KTSS;
    start_offset = read_32bitLE(0x24, streamFile) + 0x20;

    switch (codec_id) {
    case 0x2: /* DSP ADPCM - Hyrule Warriors, Fire Emblem Warriors, and other Koei Tecmo games */
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

        vgmstream->coding_type = coding_NGC_DSP;
        vgmstream->layout_type = layout_interleave;
        vgmstream->interleave_block_size = 0x8;
        dsp_read_coefs_le(vgmstream, streamFile, coef_start_offset, coef_spacing);
        break;

#ifdef VGM_USE_FFMPEG
    case 0x9: /* Opus - Dead or Alive Xtreme 3: Scarlet */
        data_size = read_32bitLE(0x44, streamFile);
        {
            vgmstream->codec_data = init_ffmpeg_switch_opus(streamFile, start_offset, data_size, vgmstream->channels, skip, vgmstream->sample_rate);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            if (vgmstream->num_samples == 0) {
                vgmstream->num_samples = switch_opus_get_samples(start_offset, data_size, streamFile) - skip;
            }
        }
        break;
    
    default:
        goto fail;
#endif
    }

    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
