#include "meta.h"
#include "../coding/coding.h"

VGMSTREAM* init_vgmstream_ktss(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    int loop_flag, channels, sample_rate;
    int8_t version, tracks, codec_id;
    int32_t num_samples, loop_start, loop_length, coef_offset, coef_spacing;
    off_t start_offset;
    size_t data_size, skip = 0;

    /* checks */
    /* .kns: Atelier Lydie & Suelle: The Alchemists and the Mysterious Paintings (Switch)
     * .kno: Ciel Nosurge DX (Switch)
     * .ktss: header id */
    if (!check_extensions(sf, "kns,kno,ktss"))
        goto fail;

    if (!is_id32be(0x00,sf, "KTSS"))
        goto fail;
    /* 0x04: data size */

    codec_id = read_u8(0x20, sf);
    /* 0x01: null, part of codec? */
    version = read_u8(0x22, sf);
    /* 0x03: same as version? */
    start_offset = read_u32le(0x24, sf) + 0x20;

    /* Layered tracks used in Hyrule Warriors (Switch), like 2*5 = 10 channels
     * Other games typically use 1 track. */
    tracks    = read_u8(0x28, sf);
    channels = read_u8(0x29, sf) * tracks;
    sample_rate   = read_u32le(0x2c, sf);

    num_samples = read_s32le(0x30, sf);
    loop_start  = read_s32le(0x34, sf);
    loop_length = read_s32le(0x38, sf);
    loop_flag = loop_length > 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_start + loop_length;
    vgmstream->meta_type = meta_KTSS;

    switch (codec_id) {
        case 0x2: /* DSP ADPCM - Hyrule Warriors, Fire Emblem Warriors */
            /* check type details */
            if (version == 1) {
                coef_offset = 0x40;
                coef_spacing = 0x2e;
            }
            else if (version == 3) { /* Fire Emblem Warriors (Switch) */
                coef_offset = 0x5c;
                coef_spacing = 0x60;
            }
            else
                goto fail;

            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x8;
            dsp_read_coefs_le(vgmstream, sf, coef_offset, coef_spacing);
            break;

#ifdef VGM_USE_FFMPEG
        case 0x9: { /* Opus - Dead or Alive Xtreme 3: Scarlet, Fire Emblem: Three Houses */
            opus_config cfg = {0};

            start_offset = read_u32le(0x40, sf); /* after seek table, if any */
            data_size = read_u32le(0x44, sf);
            /* 0x48: seek table start (0 if no seek table) */
            /* 0x4c: number of frames */

            /* 0x50: frame size, or 0 if VBR */
            /* 0x52: samples per frame */
            /* 0x54: sample rate */
            cfg.skip = read_s32le(0x58, sf);
            cfg.sample_rate = vgmstream->sample_rate;
            cfg.channels = vgmstream->channels;

            /* this info seems always included even for stereo streams */
            if (vgmstream->channels <= 8) {
                int i;
                cfg.stream_count = read_u8(0x5a,sf);
                cfg.coupled_count = read_u8(0x5b,sf);
                for (i = 0; i < vgmstream->channels; i++) {
                    cfg.channel_mapping[i] = read_u8(0x5c + i,sf);
                }
            }

            /* 0x60: null/reserved */
            /* 0x70: seek table (0x02 * frames) if VBR */
            /* later games use VBR frames, hence the seek table [Warriors Orochi 4 Ultimate DLC (Switch)] */

            vgmstream->codec_data = init_ffmpeg_switch_opus_config(sf, start_offset, data_size, &cfg);
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
                vgmstream->num_samples = switch_opus_get_samples(start_offset, data_size, sf) - skip;
            }
            break;
        }
#endif
        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
