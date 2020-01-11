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

    vgmstream->sample_rate = read_32bitLE(0x2c, streamFile);
    vgmstream->num_samples = read_32bitLE(0x30, streamFile);
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
        case 0x9: { /* Opus - Dead or Alive Xtreme 3: Scarlet, Fire Emblem: Three Houses */
            opus_config cfg = {0};

            data_size = read_32bitLE(0x44, streamFile);

            cfg.channels = vgmstream->channels;
            cfg.skip = read_32bitLE(0x58, streamFile);
            cfg.sample_rate = vgmstream->sample_rate; /* also at 0x54 */

            /* this info seems always included even for stereo streams */
            if (vgmstream->channels <= 8) {
                int i;
                cfg.stream_count = read_8bit(0x5a,streamFile);
                cfg.coupled_count = read_8bit(0x5b,streamFile);
                for (i = 0; i < vgmstream->channels; i++) {
                    cfg.channel_mapping[i] = read_8bit(0x5c + i,streamFile);
                }
            }

            vgmstream->codec_data = init_ffmpeg_switch_opus_config(streamFile, start_offset, data_size, &cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            vgmstream->channel_layout = ffmpeg_get_channel_layout(vgmstream->codec_data);

            /* apparently KTSS doesn't need standard Opus reordering, so we undo their thing */
            switch(vgmstream->channels) {
                case 6: {
                    /*  FL FC FR BL LFE BR > FL FR FC LFE BL BR */
                    int channel_remap[] = { 0, 2, 2, 3, 3, 5 };
                    ffmpeg_set_channel_remapping(vgmstream->codec_data, channel_remap);
                    break;
                }
                default:
                    break;
            }
            if (vgmstream->num_samples == 0) {
                vgmstream->num_samples = switch_opus_get_samples(start_offset, data_size, streamFile) - skip;
            }
            break;
        }
#endif
        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
