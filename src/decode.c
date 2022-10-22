#include "vgmstream.h"
#include "decode.h"
#include "layout/layout.h"
#include "coding/coding.h"
#include "mixing.h"
#include "plugins.h"
#ifdef VGM_USE_MAIATRAC3PLUS
#include "at3plus_decoder.h"
#endif

/* custom codec handling, not exactly "decode" stuff but here to simplify adding new codecs */


void free_codec(VGMSTREAM* vgmstream) {

#ifdef VGM_USE_VORBIS
    if (vgmstream->coding_type == coding_OGG_VORBIS) {
        free_ogg_vorbis(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_VORBIS_custom) {
        free_vorbis_custom(vgmstream->codec_data);
    }
#endif

    if (vgmstream->coding_type == coding_CIRCUS_VQ) {
        free_circus_vq(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_RELIC) {
        free_relic(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_CRI_HCA) {
        free_hca(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_TAC) {
        free_tac(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_ICE_RANGE ||
        vgmstream->coding_type == coding_ICE_DCT) {
        free_ice(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_UBI_ADPCM) {
        free_ubi_adpcm(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_IMUSE) {
        free_imuse(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_COMPRESSWAVE) {
        free_compresswave(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_EA_MT) {
        free_ea_mt(vgmstream->codec_data, vgmstream->channels);
    }

#ifdef VGM_USE_FFMPEG
    if (vgmstream->coding_type == coding_FFmpeg) {
        free_ffmpeg(vgmstream->codec_data);
    }
#endif

#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
    if (vgmstream->coding_type == coding_MP4_AAC) {
        free_mp4_aac(vgmstream->codec_data);
    }
#endif

#ifdef VGM_USE_MPEG
    if (vgmstream->coding_type == coding_MPEG_custom ||
        vgmstream->coding_type == coding_MPEG_ealayer3 ||
        vgmstream->coding_type == coding_MPEG_layer1 ||
        vgmstream->coding_type == coding_MPEG_layer2 ||
        vgmstream->coding_type == coding_MPEG_layer3) {
        free_mpeg(vgmstream->codec_data);
    }
#endif

#ifdef VGM_USE_G7221
    if (vgmstream->coding_type == coding_G7221C) {
        free_g7221(vgmstream->codec_data);
    }
#endif

#ifdef VGM_USE_G719
    if (vgmstream->coding_type == coding_G719) {
        free_g719(vgmstream->codec_data, vgmstream->channels);
    }
#endif

#ifdef VGM_USE_MAIATRAC3PLUS
    if (vgmstream->coding_type == coding_AT3plus) {
        free_at3plus(vgmstream->codec_data);
    }
#endif

#ifdef VGM_USE_ATRAC9
    if (vgmstream->coding_type == coding_ATRAC9) {
        free_atrac9(vgmstream->codec_data);
    }
#endif

#ifdef VGM_USE_CELT
    if (vgmstream->coding_type == coding_CELT_FSB) {
        free_celt_fsb(vgmstream->codec_data);
    }
#endif

#ifdef VGM_USE_SPEEX
    if (vgmstream->coding_type == coding_SPEEX) {
        free_speex(vgmstream->codec_data);
    }
#endif

    if (vgmstream->coding_type == coding_ACM) {
        free_acm(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_NWA) {
        free_nwa(vgmstream->codec_data);
    }
}


void seek_codec(VGMSTREAM* vgmstream) {
    if (vgmstream->coding_type == coding_CIRCUS_VQ) {
        seek_circus_vq(vgmstream->codec_data, vgmstream->loop_current_sample);
    }

    if (vgmstream->coding_type == coding_RELIC) {
        seek_relic(vgmstream->codec_data, vgmstream->loop_current_sample);
    }

    if (vgmstream->coding_type == coding_CRI_HCA) {
        loop_hca(vgmstream->codec_data, vgmstream->loop_current_sample);
    }

    if (vgmstream->coding_type == coding_TAC) {
        seek_tac(vgmstream->codec_data, vgmstream->loop_current_sample);
    }

    if (vgmstream->coding_type == coding_ICE_RANGE ||
        vgmstream->coding_type == coding_ICE_DCT) {
        seek_ice(vgmstream->codec_data, vgmstream->loop_current_sample);
    }

    if (vgmstream->coding_type == coding_UBI_ADPCM) {
        seek_ubi_adpcm(vgmstream->codec_data, vgmstream->loop_current_sample);
    }

    if (vgmstream->coding_type == coding_IMUSE) {
        seek_imuse(vgmstream->codec_data, vgmstream->loop_current_sample);
    }

    if (vgmstream->coding_type == coding_COMPRESSWAVE) {
        seek_compresswave(vgmstream->codec_data, vgmstream->loop_current_sample);
    }

    if (vgmstream->coding_type == coding_EA_MT) {
        seek_ea_mt(vgmstream, vgmstream->loop_current_sample);
    }

#ifdef VGM_USE_VORBIS
    if (vgmstream->coding_type == coding_OGG_VORBIS) {
        seek_ogg_vorbis(vgmstream->codec_data, vgmstream->loop_current_sample);
    }

    if (vgmstream->coding_type == coding_VORBIS_custom) {
        seek_vorbis_custom(vgmstream, vgmstream->loop_current_sample);
    }
#endif

#ifdef VGM_USE_FFMPEG
    if (vgmstream->coding_type == coding_FFmpeg) {
        seek_ffmpeg(vgmstream->codec_data, vgmstream->loop_current_sample);
    }
#endif

#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
    if (vgmstream->coding_type == coding_MP4_AAC) {
        seek_mp4_aac(vgmstream, vgmstream->loop_sample);
    }
#endif

#ifdef VGM_USE_MAIATRAC3PLUS
    if (vgmstream->coding_type == coding_AT3plus) {
        seek_at3plus(vgmstream, vgmstream->loop_current_sample);
    }
#endif

#ifdef VGM_USE_ATRAC9
    if (vgmstream->coding_type == coding_ATRAC9) {
        seek_atrac9(vgmstream, vgmstream->loop_current_sample);
    }
#endif

#ifdef VGM_USE_CELT
    if (vgmstream->coding_type == coding_CELT_FSB) {
        seek_celt_fsb(vgmstream, vgmstream->loop_current_sample);
    }
#endif

#ifdef VGM_USE_SPEEX
    if (vgmstream->coding_type == coding_SPEEX) {
        seek_speex(vgmstream, vgmstream->loop_current_sample);
    }
#endif

#ifdef VGM_USE_MPEG
    if (vgmstream->coding_type == coding_MPEG_custom ||
        vgmstream->coding_type == coding_MPEG_ealayer3 ||
        vgmstream->coding_type == coding_MPEG_layer1 ||
        vgmstream->coding_type == coding_MPEG_layer2 ||
        vgmstream->coding_type == coding_MPEG_layer3) {
        seek_mpeg(vgmstream, vgmstream->loop_current_sample);
    }
#endif

    if (vgmstream->coding_type == coding_NWA) {
        seek_nwa(vgmstream->codec_data, vgmstream->loop_current_sample);
    }
}


void reset_codec(VGMSTREAM* vgmstream) {

#ifdef VGM_USE_VORBIS
    if (vgmstream->coding_type == coding_OGG_VORBIS) {
        reset_ogg_vorbis(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_VORBIS_custom) {
        reset_vorbis_custom(vgmstream);
    }
#endif

    if (vgmstream->coding_type == coding_CIRCUS_VQ) {
        reset_circus_vq(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_RELIC) {
        reset_relic(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_CRI_HCA) {
        reset_hca(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_TAC) {
        reset_tac(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_ICE_RANGE ||
        vgmstream->coding_type == coding_ICE_DCT) {
        reset_ice(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_UBI_ADPCM) {
        reset_ubi_adpcm(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_IMUSE) {
        reset_imuse(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_COMPRESSWAVE) {
        reset_compresswave(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_EA_MT) {
        reset_ea_mt(vgmstream);
    }

#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
    if (vgmstream->coding_type == coding_MP4_AAC) {
        reset_mp4_aac(vgmstream);
    }
#endif

#ifdef VGM_USE_MPEG
    if (vgmstream->coding_type == coding_MPEG_custom ||
        vgmstream->coding_type == coding_MPEG_ealayer3 ||
        vgmstream->coding_type == coding_MPEG_layer1 ||
        vgmstream->coding_type == coding_MPEG_layer2 ||
        vgmstream->coding_type == coding_MPEG_layer3) {
        reset_mpeg(vgmstream->codec_data);
    }
#endif

#ifdef VGM_USE_G7221
    if (vgmstream->coding_type == coding_G7221C) {
        reset_g7221(vgmstream->codec_data);
    }
#endif

#ifdef VGM_USE_G719
    if (vgmstream->coding_type == coding_G719) {
        reset_g719(vgmstream->codec_data, vgmstream->channels);
    }
#endif

#ifdef VGM_USE_MAIATRAC3PLUS
    if (vgmstream->coding_type == coding_AT3plus) {
        reset_at3plus(vgmstream);
    }
#endif

#ifdef VGM_USE_ATRAC9
    if (vgmstream->coding_type == coding_ATRAC9) {
        reset_atrac9(vgmstream->codec_data);
    }
#endif

#ifdef VGM_USE_CELT
    if (vgmstream->coding_type == coding_CELT_FSB) {
        reset_celt_fsb(vgmstream->codec_data);
    }
#endif

#ifdef VGM_USE_SPEEX
    if (vgmstream->coding_type == coding_SPEEX) {
        reset_speex(vgmstream->codec_data);
    }
#endif

#ifdef VGM_USE_FFMPEG
    if (vgmstream->coding_type == coding_FFmpeg) {
        reset_ffmpeg(vgmstream->codec_data);
    }
#endif

    if (vgmstream->coding_type == coding_ACM) {
        reset_acm(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_NWA) {
        reset_nwa(vgmstream->codec_data);
    }
}


/* Get the number of samples of a single frame (smallest self-contained sample group, 1/N channels) */
int get_vgmstream_samples_per_frame(VGMSTREAM* vgmstream) {
    /* Value returned here is the max (or less) that vgmstream will ask a decoder per
     * "decode_x" call. Decoders with variable samples per frame or internal discard
     * may return 0 here and handle arbitrary samples_to_do values internally
     * (or some internal sample buffer max too). */

    switch (vgmstream->coding_type) {
        case coding_SILENCE:
            return 0;

        case coding_CRI_ADX:
        case coding_CRI_ADX_fixed:
        case coding_CRI_ADX_exp:
        case coding_CRI_ADX_enc_8:
        case coding_CRI_ADX_enc_9:
            return (vgmstream->interleave_block_size - 2) * 2;

        case coding_NGC_DSP:
        case coding_NGC_DSP_subint:
            return 14;
        case coding_NGC_AFC:
        case coding_VADPCM:
            return 16;
        case coding_NGC_DTK:
            return 28;
        case coding_G721:
            return 1;

        case coding_PCM16LE:
        case coding_PCM16BE:
        case coding_PCM16_int:
        case coding_PCM8:
        case coding_PCM8_int:
        case coding_PCM8_U:
        case coding_PCM8_U_int:
        case coding_PCM8_SB:
        case coding_ULAW:
        case coding_ULAW_int:
        case coding_ALAW:
        case coding_PCMFLOAT:
        case coding_PCM24LE:
            return 1;
#ifdef VGM_USE_VORBIS
        case coding_OGG_VORBIS:
        case coding_VORBIS_custom:
#endif
#ifdef VGM_USE_MPEG
        case coding_MPEG_custom:
        case coding_MPEG_ealayer3:
        case coding_MPEG_layer1:
        case coding_MPEG_layer2:
        case coding_MPEG_layer3:
#endif
        case coding_SDX2:
        case coding_SDX2_int:
        case coding_CBD2:
        case coding_CBD2_int:
        case coding_ACM:
        case coding_DERF:
        case coding_WADY:
        case coding_NWA:
        case coding_SASSC:
        case coding_CIRCUS_ADPCM:
            return 1;

        case coding_IMA:
        case coding_DVI_IMA:
        case coding_SNDS_IMA:
        case coding_QD_IMA:
        case coding_UBI_IMA:
        case coding_UBI_SCE_IMA:
        case coding_OKI16:
        case coding_OKI4S:
        case coding_MTF_IMA:
            return 1;
        case coding_PCM4:
        case coding_PCM4_U:
        case coding_IMA_int:
        case coding_DVI_IMA_int:
        case coding_NW_IMA:
        case coding_WV6_IMA:
        case coding_HV_IMA:
        case coding_FFTA2_IMA:
        case coding_BLITZ_IMA:
        case coding_PCFX:
            return 2;
        case coding_XBOX_IMA:
        case coding_XBOX_IMA_mch:
        case coding_XBOX_IMA_int:
        case coding_FSB_IMA:
        case coding_WWISE_IMA:
        case coding_CD_IMA:
            return 64;
        case coding_APPLE_IMA4:
            return 64;
        case coding_MS_IMA:
        case coding_REF_IMA:
            return ((vgmstream->interleave_block_size - 0x04*vgmstream->channels) * 2 / vgmstream->channels) + 1;/* +1 from header sample */
        case coding_MS_IMA_mono:
            return ((vgmstream->frame_size - 0x04) * 2) + 1; /* +1 from header sample */
        case coding_RAD_IMA:
            return (vgmstream->interleave_block_size - 0x04*vgmstream->channels) * 2 / vgmstream->channels;
        case coding_NDS_IMA:
        case coding_DAT4_IMA:
            return (vgmstream->interleave_block_size - 0x04) * 2;
        case coding_AWC_IMA:
            return (0x800 - 0x04) * 2;
        case coding_RAD_IMA_mono:
            return 32;
        case coding_H4M_IMA:
            return 0; /* variable (block-controlled) */

        case coding_XA:
        case coding_XA_EA:
            return 28*8 / vgmstream->channels; /* 8 subframes per frame, mono/stereo */
        case coding_XA8:
            return 28*4 / vgmstream->channels; /* 4 subframes per frame, mono/stereo */
        case coding_PSX:
        case coding_PSX_badflags:
        case coding_HEVAG:
            return 28;
        case coding_PSX_cfg:
        case coding_PSX_pivotal:
            return (vgmstream->interleave_block_size - 0x01) * 2; /* size 0x01 header */

        case coding_EA_XA:
        case coding_EA_XA_int:
        case coding_EA_XA_V2:
        case coding_MAXIS_XA:
            return 28;
        case coding_EA_XAS_V0:
            return 32;
        case coding_EA_XAS_V1:
            return 128;

        case coding_MSADPCM:
            return (vgmstream->frame_size - 0x07*vgmstream->channels)*2 / vgmstream->channels + 2;
        case coding_MSADPCM_int:
        case coding_MSADPCM_ck:
            return (vgmstream->frame_size - 0x07)*2 + 2;
        case coding_WS: /* only works if output sample size is 8 bit, which always is for WS ADPCM */
            return vgmstream->ws_output_size;
        case coding_AICA:
        case coding_CP_YM:
            return 1;
        case coding_AICA_int:
            return 2;
        case coding_ASKA:
            return (vgmstream->frame_size - 0x04*vgmstream->channels) * 2 / vgmstream->channels;
        case coding_NXAP:
            return (0x40-0x04) * 2;
        case coding_NDS_PROCYON:
            return 30;
        case coding_L5_555:
            return 32;
        case coding_LSF:
            return 54;

#ifdef VGM_USE_G7221
        case coding_G7221C:
            return 32000/50; /* Siren7: 16000/50 */
#endif
#ifdef VGM_USE_G719
        case coding_G719:
            return 48000/50;
#endif
#ifdef VGM_USE_FFMPEG
        case coding_FFmpeg:
            return 0;
#endif
        case coding_MTAF:
            return 128*2;
        case coding_MTA2:
            return 128*2;
        case coding_MC3:
            return 10;
        case coding_FADPCM:
            return 256; /* (0x8c - 0xc) * 2 */
        case coding_ASF:
            return 32;  /* (0x11 - 0x1) * 2 */
        case coding_TANTALUS:
            return 30; /* (0x10 - 0x01) * 2 */
        case coding_DSA:
            return 14;  /* (0x08 - 0x1) * 2 */
        case coding_XMD:
            return (vgmstream->interleave_block_size - 0x06)*2 + 2;
        case coding_PTADPCM:
            return (vgmstream->interleave_block_size - 0x05)*2 + 2;
        case coding_UBI_ADPCM:
            return 0; /* varies per mode */
        case coding_IMUSE:
            return 0; /* varies per frame */
        case coding_COMPRESSWAVE:
            return 0; /* multiple of 2 */
        case coding_EA_MT:
            return 0; /* 432, but variable in looped files */
        case coding_CIRCUS_VQ:
            return 0;
        case coding_RELIC:
            return 0; /* 512 */
        case coding_CRI_HCA:
            return 0; /* 1024 - delay/padding (which can be bigger than 1024) */
        case coding_TAC:
            return 0; /* 1024 - delay/padding */
        case coding_ICE_RANGE:
        case coding_ICE_DCT:
            return 0; /* ~100 (range), ~16 (DCT) */
#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
        case coding_MP4_AAC:
            return ((mp4_aac_codec_data*)vgmstream->codec_data)->samples_per_frame;
#endif
#ifdef VGM_USE_MAIATRAC3PLUS
        case coding_AT3plus:
            return 2048 - ((maiatrac3plus_codec_data*)vgmstream->codec_data)->samples_discard;
#endif
#ifdef VGM_USE_ATRAC9
        case coding_ATRAC9:
            return 0; /* varies with config data, usually 256 or 1024 */
#endif
#ifdef VGM_USE_CELT
        case coding_CELT_FSB:
            return 0; /* 512? */
#endif
#ifdef VGM_USE_SPEEX
        case coding_SPEEX:
            return 0;
#endif
        default:
            return 0;
    }
}

/* Get the number of bytes of a single frame (smallest self-contained byte group, 1/N channels) */
int get_vgmstream_frame_size(VGMSTREAM* vgmstream) {
    switch (vgmstream->coding_type) {
        case coding_SILENCE:
            return 0;

        case coding_CRI_ADX:
        case coding_CRI_ADX_fixed:
        case coding_CRI_ADX_exp:
        case coding_CRI_ADX_enc_8:
        case coding_CRI_ADX_enc_9:
            return vgmstream->interleave_block_size;

        case coding_NGC_DSP:
            return 0x08;
        case coding_NGC_DSP_subint:
            return 0x08 * vgmstream->channels;
        case coding_NGC_AFC:
        case coding_VADPCM:
            return 0x09;
        case coding_NGC_DTK:
            return 0x20;
        case coding_G721:
            return 0;

        case coding_PCM16LE:
        case coding_PCM16BE:
        case coding_PCM16_int:
            return 0x02;
        case coding_PCM8:
        case coding_PCM8_int:
        case coding_PCM8_U:
        case coding_PCM8_U_int:
        case coding_PCM8_SB:
        case coding_ULAW:
        case coding_ULAW_int:
        case coding_ALAW:
            return 0x01;
        case coding_PCMFLOAT:
            return 0x04;
        case coding_PCM24LE:
            return 0x03;

        case coding_SDX2:
        case coding_SDX2_int:
        case coding_CBD2:
        case coding_CBD2_int:
        case coding_DERF:
        case coding_WADY:
        case coding_NWA:
        case coding_SASSC:
        case coding_CIRCUS_ADPCM:
            return 0x01;

        case coding_PCM4:
        case coding_PCM4_U:
        case coding_IMA:
        case coding_IMA_int:
        case coding_DVI_IMA:
        case coding_DVI_IMA_int:
        case coding_NW_IMA:
        case coding_WV6_IMA:
        case coding_HV_IMA:
        case coding_FFTA2_IMA:
        case coding_BLITZ_IMA:
        case coding_PCFX:
        case coding_OKI16:
        case coding_OKI4S:
        case coding_MTF_IMA:
            return 0x01;
        case coding_RAD_IMA:
        case coding_NDS_IMA:
        case coding_DAT4_IMA:
        case coding_REF_IMA:
            return vgmstream->interleave_block_size;
        case coding_MS_IMA:
        case coding_MS_IMA_mono:
            return vgmstream->frame_size;
        case coding_AWC_IMA:
            return 0x800;
        case coding_RAD_IMA_mono:
            return 0x14;
        case coding_SNDS_IMA:
        case coding_QD_IMA:
            return 0; //todo: 0x01?
        case coding_UBI_IMA: /* variable (PCM then IMA) */
            return 0;
        case coding_UBI_SCE_IMA:
            return 0;
        case coding_XBOX_IMA:
            //todo should be  0x48 when stereo, but blocked/interleave layout don't understand stereo codecs
            return 0x24; //vgmstream->channels==1 ? 0x24 : 0x48;
        case coding_XBOX_IMA_int:
        case coding_WWISE_IMA:
        case coding_CD_IMA:
            return 0x24;
        case coding_XBOX_IMA_mch:
        case coding_FSB_IMA:
            return 0x24 * vgmstream->channels;
        case coding_APPLE_IMA4:
            return 0x22;
        case coding_H4M_IMA:
            return 0x00; /* variable (block-controlled) */

        case coding_XA:
        case coding_XA_EA:
        case coding_XA8:
            return 0x80;
        case coding_PSX:
        case coding_PSX_badflags:
        case coding_HEVAG:
            return 0x10;
        case coding_PSX_cfg:
        case coding_PSX_pivotal:
            return vgmstream->interleave_block_size;

        case coding_EA_XA:
            return 0x1E;
        case coding_EA_XA_int:
            return 0x0F;
        case coding_MAXIS_XA:
            return 0x0F*vgmstream->channels;
        case coding_EA_XA_V2:
            return 0; /* variable (ADPCM frames of 0x0f or PCM frames of 0x3d) */
        case coding_EA_XAS_V0:
            return 0xF+0x02+0x02;
        case coding_EA_XAS_V1:
            return 0x4c*vgmstream->channels;

        case coding_MSADPCM:
        case coding_MSADPCM_int:
        case coding_MSADPCM_ck:
            return vgmstream->frame_size;
        case coding_WS:
            return vgmstream->current_block_size;
        case coding_AICA:
        case coding_AICA_int:
        case coding_CP_YM:
            return 0x01;
        case coding_ASKA:
            return vgmstream->frame_size;
        case coding_NXAP:
            return 0x40;
        case coding_NDS_PROCYON:
            return 0x10;
        case coding_L5_555:
            return 0x12;
        case coding_LSF:
            return 0x1C;

#ifdef VGM_USE_G7221
        case coding_G7221C:
#endif
#ifdef VGM_USE_G719
        case coding_G719:
#endif
#ifdef VGM_USE_MAIATRAC3PLUS
        case coding_AT3plus:
#endif
#ifdef VGM_USE_FFMPEG
        case coding_FFmpeg:
#endif
        case coding_MTAF:
            return vgmstream->interleave_block_size;
        case coding_MTA2:
            return 0x90;
        case coding_MC3:
            return 0x04;
        case coding_FADPCM:
            return 0x8c;
        case coding_ASF:
            return 0x11;
        case coding_TANTALUS:
            return 0x10;
        case coding_DSA:
            return 0x08;
        case coding_XMD:
            return vgmstream->interleave_block_size;
        case coding_PTADPCM:
            return vgmstream->interleave_block_size;
        /* UBI_ADPCM: varies per mode? */
        /* IMUSE: VBR */
        /* EA_MT: VBR, frames of bit counts or PCM frames */
        /* COMPRESSWAVE: VBR/huffman bits */
        /* ATRAC9: CBR around  0x100-200 */
        /* CELT FSB: varies, usually 0x80-100 */
        /* SPEEX: varies, usually 0x40-60 */
        /* TAC: VBR around ~0x200-300 */
        /* Vorbis, MPEG, ACM, etc: varies */
        default: /* (VBR or managed by decoder) */
            return 0;
    }
}

/* In NDS IMA the frame size is the block size, so the last one is short */
int get_vgmstream_samples_per_shortframe(VGMSTREAM* vgmstream) {
    switch (vgmstream->coding_type) {
        case coding_NDS_IMA:
            return (vgmstream->interleave_last_block_size-4)*2;
        default:
            return get_vgmstream_samples_per_frame(vgmstream);
    }
}

int get_vgmstream_shortframe_size(VGMSTREAM* vgmstream) {
    switch (vgmstream->coding_type) {
        case coding_NDS_IMA:
            return vgmstream->interleave_last_block_size;
        default:
            return get_vgmstream_frame_size(vgmstream);
    }
}

/* Decode samples into the buffer. Assume that we have written samples_written into the
 * buffer already, and we have samples_to_do consecutive samples ahead of us (won't call
 * more than one frame if configured above to do so).
 * Called by layouts since they handle samples written/to_do */
void decode_vgmstream(VGMSTREAM* vgmstream, int samples_written, int samples_to_do, sample_t* buffer) {
    int ch;

    buffer += samples_written * vgmstream->channels; /* passed externally to simplify I guess */

    switch (vgmstream->coding_type) {
        case coding_SILENCE:
            memset(buffer, 0, samples_to_do * vgmstream->channels * sizeof(sample_t));
            break;

        case coding_CRI_ADX:
        case coding_CRI_ADX_exp:
        case coding_CRI_ADX_fixed:
        case coding_CRI_ADX_enc_8:
        case coding_CRI_ADX_enc_9:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_adx(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do,
                        vgmstream->interleave_block_size, vgmstream->coding_type);
            }
            break;
        case coding_NGC_DSP:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ngc_dsp(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_NGC_DSP_subint:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ngc_dsp_subint(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch,
                        vgmstream->interleave_block_size);
            }
            break;

        case coding_PCM16LE:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcm16le(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_PCM16BE:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcm16be(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_PCM16_int:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcm16_int(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do,
                        vgmstream->codec_endian);
            }
            break;
        case coding_PCM8:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcm8(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_PCM8_int:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcm8_int(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_PCM8_U:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcm8_unsigned(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_PCM8_U_int:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcm8_unsigned_int(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_PCM8_SB:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcm8_sb(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_PCM4:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcm4(vgmstream,&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_PCM4_U:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcm4_unsigned(vgmstream, &vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;

        case coding_ULAW:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ulaw(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_ULAW_int:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ulaw_int(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_ALAW:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_alaw(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_PCMFLOAT:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcmfloat(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do,
                        vgmstream->codec_endian);
            }
            break;

        case coding_PCM24LE:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcm24le(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;

        case coding_NDS_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_nds_ima(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_DAT4_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_dat4_ima(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_XBOX_IMA:
        case coding_XBOX_IMA_int: {
            int is_stereo = (vgmstream->channels > 1 && vgmstream->coding_type == coding_XBOX_IMA);
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_xbox_ima(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch, is_stereo);
            }
            break;
        }
        case coding_XBOX_IMA_mch:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_xbox_ima_mch(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_MS_IMA:
        case coding_MS_IMA_mono:
            //TODO: improve
            vgmstream->codec_config = (vgmstream->coding_type == coding_MS_IMA_mono) || vgmstream->channels == 1; /* mono mode */
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ms_ima(vgmstream,&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_RAD_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_rad_ima(vgmstream,&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_RAD_IMA_mono:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_rad_ima_mono(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_NGC_DTK:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ngc_dtk(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_G721:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_g721(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_NGC_AFC:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ngc_afc(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_VADPCM: {
            int order = vgmstream->codec_config;
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_vadpcm(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, order);
            }
            break;
        }
        case coding_PSX:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_psx(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do,
                        0, vgmstream->codec_config);
            }
            break;
        case coding_PSX_badflags:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_psx(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do,
                        1, vgmstream->codec_config);
            }
            break;
        case coding_PSX_cfg:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_psx_configurable(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do,
                        vgmstream->interleave_block_size, vgmstream->codec_config);
            }
            break;
        case coding_PSX_pivotal:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_psx_pivotal(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do,
                        vgmstream->interleave_block_size);
            }
            break;
        case coding_HEVAG:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_hevag(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_XA:
        case coding_XA_EA:
        case coding_XA8: {
            decode_xa(vgmstream, buffer, samples_to_do);
            break;
        }
        case coding_EA_XA:
        case coding_EA_XA_int: {
            int is_stereo = (vgmstream->channels > 1 && vgmstream->coding_type == coding_EA_XA);
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ea_xa(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch, is_stereo);
            }
            break;
        }
        case coding_EA_XA_V2:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ea_xa_v2(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_MAXIS_XA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_maxis_xa(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_EA_XAS_V0:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ea_xas_v0(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_EA_XAS_V1:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ea_xas_v1(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
#ifdef VGM_USE_VORBIS
        case coding_OGG_VORBIS:
            decode_ogg_vorbis(vgmstream->codec_data, buffer, samples_to_do, vgmstream->channels);
            break;

        case coding_VORBIS_custom:
            decode_vorbis_custom(vgmstream, buffer, samples_to_do, vgmstream->channels);
            break;
#endif
        case coding_CIRCUS_VQ:
            decode_circus_vq(vgmstream->codec_data, buffer, samples_to_do, vgmstream->channels);
            break;
        case coding_RELIC:
            decode_relic(&vgmstream->ch[0], vgmstream->codec_data, buffer, samples_to_do);
            break;
        case coding_CRI_HCA:
            decode_hca(vgmstream->codec_data, buffer, samples_to_do);
            break;
        case coding_TAC:
            decode_tac(vgmstream, buffer, samples_to_do);
            break;
        case coding_ICE_RANGE:
        case coding_ICE_DCT:
            decode_ice(vgmstream->codec_data, buffer, samples_to_do);
            break;
#ifdef VGM_USE_FFMPEG
        case coding_FFmpeg:
            decode_ffmpeg(vgmstream, buffer, samples_to_do, vgmstream->channels);
            break;
#endif
#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
        case coding_MP4_AAC:
            decode_mp4_aac(vgmstream->codec_data, buffer, samples_to_do, vgmstream->channels);
            break;
#endif
        case coding_SDX2:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_sdx2(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_SDX2_int:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_sdx2_int(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_CBD2:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_cbd2(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_CBD2_int:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_cbd2_int(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_DERF:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_derf(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_WADY:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_wady(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_CIRCUS_ADPCM:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_circus_adpcm(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;

        case coding_IMA:
        case coding_IMA_int:
        case coding_DVI_IMA:
        case coding_DVI_IMA_int: {
            int is_stereo = (vgmstream->channels > 1 && vgmstream->coding_type == coding_IMA)
                    || (vgmstream->channels > 1 && vgmstream->coding_type == coding_DVI_IMA);
            int is_high_first = vgmstream->coding_type == coding_DVI_IMA
                    || vgmstream->coding_type == coding_DVI_IMA_int;
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_standard_ima(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch,
                        is_stereo, is_high_first);
            }
            break;
        }
        case coding_MTF_IMA: {
            int is_stereo = (vgmstream->channels > 1);
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_mtf_ima(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch, is_stereo);
            }
            break;
        }
        case coding_NW_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_nw_ima(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_WV6_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_wv6_ima(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_HV_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_hv_ima(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_FFTA2_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ffta2_ima(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_BLITZ_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_blitz_ima(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;

        case coding_APPLE_IMA4:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_apple_ima4(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_SNDS_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_snds_ima(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_QD_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_otns_ima(vgmstream, &vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_FSB_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_fsb_ima(vgmstream, &vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_WWISE_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_wwise_ima(vgmstream,&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_REF_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ref_ima(vgmstream,&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_AWC_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_awc_ima(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_UBI_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ubi_ima(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_UBI_SCE_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ubi_sce_ima(&vgmstream->ch[ch], buffer+ch,
                    vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_H4M_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                uint16_t frame_format = (uint16_t)((vgmstream->codec_config >> 8) & 0xFFFF);

                decode_h4m_ima(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch,
                        frame_format);
            }
            break;
        case coding_CD_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_cd_ima(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;

        case coding_WS:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ws(vgmstream, ch, buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;

#ifdef VGM_USE_MPEG
        case coding_MPEG_custom:
        case coding_MPEG_ealayer3:
        case coding_MPEG_layer1:
        case coding_MPEG_layer2:
        case coding_MPEG_layer3:
            decode_mpeg(vgmstream, buffer, samples_to_do, vgmstream->channels);
            break;
#endif
#ifdef VGM_USE_G7221
        case coding_G7221C:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_g7221(vgmstream, buffer+ch, vgmstream->channels, samples_to_do, ch);
            }
            break;
#endif
#ifdef VGM_USE_G719
        case coding_G719:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_g719(vgmstream, buffer+ch, vgmstream->channels, samples_to_do, ch);
            }
            break;
#endif
#ifdef VGM_USE_MAIATRAC3PLUS
        case coding_AT3plus:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_at3plus(vgmstream, buffer+ch, vgmstream->channels, samples_to_do, ch);
            }
            break;
#endif
#ifdef VGM_USE_ATRAC9
        case coding_ATRAC9:
            decode_atrac9(vgmstream, buffer, samples_to_do, vgmstream->channels);
            break;
#endif
#ifdef VGM_USE_CELT
        case coding_CELT_FSB:
            decode_celt_fsb(vgmstream, buffer, samples_to_do, vgmstream->channels);
            break;
#endif
#ifdef VGM_USE_SPEEX
        case coding_SPEEX:
            decode_speex(vgmstream, buffer, samples_to_do);
            break;
#endif
        case coding_ACM:
            decode_acm(vgmstream->codec_data, buffer, samples_to_do, vgmstream->channels);
            break;
        case coding_NWA:
            decode_nwa(vgmstream->codec_data, buffer, samples_to_do);
            break;
        case coding_MSADPCM:
        case coding_MSADPCM_int:
            if (vgmstream->channels == 1 || vgmstream->coding_type == coding_MSADPCM_int) {
                for (ch = 0; ch < vgmstream->channels; ch++) {
                    decode_msadpcm_mono(vgmstream,buffer+ch,
                            vgmstream->channels,vgmstream->samples_into_block, samples_to_do, ch,
                            vgmstream->codec_config);
                }
            }
            else if (vgmstream->channels == 2) {
                decode_msadpcm_stereo(vgmstream, buffer, vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_MSADPCM_ck:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_msadpcm_ck(vgmstream, buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_AICA:
        case coding_AICA_int: {
            int is_stereo = (vgmstream->channels > 1 && vgmstream->coding_type == coding_AICA);
            int is_high_first = vgmstream->codec_config == 1;
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_aica(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch,
                        is_stereo, is_high_first);
            }
            break;
        }
        case coding_CP_YM: {
            int is_stereo = (vgmstream->channels > 1);
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_cp_ym(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch,
                        is_stereo);
            }
            break;
        }
        case coding_ASKA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_aska(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch, vgmstream->frame_size);
            }
            break;
        case coding_NXAP:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_nxap(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_TGC:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_tgc(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_NDS_PROCYON:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_nds_procyon(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_L5_555:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_l5_555(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_SASSC:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_sassc(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_LSF:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_lsf(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_MTAF:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_mtaf(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_MTA2:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_mta2(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch, vgmstream->codec_config);
            }
            break;
        case coding_MC3:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_mc3(vgmstream, &vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_FADPCM:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_fadpcm(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels,vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_ASF:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_asf(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_TANTALUS:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_tantalus(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_DSA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_dsa(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_XMD:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_xmd(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do,
                        vgmstream->interleave_block_size);
            }
            break;
        case coding_PTADPCM:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ptadpcm(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do,
                        vgmstream->interleave_block_size);
            }
            break;
        case coding_PCFX:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcfx(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do,
                        vgmstream->codec_config);
            }
            break;
        case coding_OKI16:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_oki16(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;

        case coding_OKI4S:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_oki4s(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;

        case coding_UBI_ADPCM:
            decode_ubi_adpcm(vgmstream, buffer, samples_to_do);
            break;

        case coding_IMUSE:
            decode_imuse(vgmstream, buffer, samples_to_do);
            break;

        case coding_COMPRESSWAVE:
            decode_compresswave(vgmstream->codec_data, buffer, samples_to_do);
            break;

        case coding_EA_MT:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ea_mt(vgmstream, buffer+ch, vgmstream->channels, samples_to_do, ch);
            }
            break;
        default:
            break;
    }
}

/* Calculate number of consecutive samples we can decode. Takes into account hitting
 * a loop start or end, or going past a single frame. */
int get_vgmstream_samples_to_do(int samples_this_block, int samples_per_frame, VGMSTREAM* vgmstream) {
    int samples_to_do;
    int samples_left_this_block;

    samples_left_this_block = samples_this_block - vgmstream->samples_into_block;
    samples_to_do = samples_left_this_block; /* by default decodes all samples left */

    /* fun loopy crap, why did I think this would be any simpler? */
    if (vgmstream->loop_flag) {
        int samples_after_decode = vgmstream->current_sample + samples_left_this_block;

        /* are we going to hit the loop end during this block? */
        if (samples_after_decode > vgmstream->loop_end_sample) {
            /* only do samples up to loop end */
            samples_to_do = vgmstream->loop_end_sample - vgmstream->current_sample;
        }

        /* are we going to hit the loop start during this block? (first time only) */
        if (samples_after_decode > vgmstream->loop_start_sample && !vgmstream->hit_loop) {
            /* only do samples up to loop start */
            samples_to_do = vgmstream->loop_start_sample - vgmstream->current_sample;
        }
    }

    /* if it's a framed encoding don't do more than one frame */
    if (samples_per_frame > 1 && (vgmstream->samples_into_block % samples_per_frame) + samples_to_do > samples_per_frame)
        samples_to_do = samples_per_frame - (vgmstream->samples_into_block % samples_per_frame);

    return samples_to_do;
}

/* Detect loop start and save values, or detect loop end and restore (loop back).
 * Returns 1 if loop was done. */
int vgmstream_do_loop(VGMSTREAM* vgmstream) {
    /*if (!vgmstream->loop_flag) return 0;*/

    /* is this the loop end? = new loop, continue from loop_start_sample */
    if (vgmstream->current_sample == vgmstream->loop_end_sample) {

        /* disable looping if target count reached and continue normally
         * (only needed with the "play stream end after looping N times" option enabled) */
        vgmstream->loop_count++;
        if (vgmstream->loop_target && vgmstream->loop_target == vgmstream->loop_count) {
            vgmstream->loop_flag = 0; /* could be improved but works ok, will be restored on resets */
            return 0;
        }

        /* against everything I hold sacred, preserve adpcm history before looping for certain types */
        if (vgmstream->meta_type == meta_DSP_STD ||
            vgmstream->meta_type == meta_DSP_RS03 ||
            vgmstream->meta_type == meta_DSP_CSTR ||
            vgmstream->coding_type == coding_PSX ||
            vgmstream->coding_type == coding_PSX_badflags) {
            int ch;
            for (ch = 0; ch < vgmstream->channels; ch++) {
                vgmstream->loop_ch[ch].adpcm_history1_16 = vgmstream->ch[ch].adpcm_history1_16;
                vgmstream->loop_ch[ch].adpcm_history2_16 = vgmstream->ch[ch].adpcm_history2_16;
                vgmstream->loop_ch[ch].adpcm_history1_32 = vgmstream->ch[ch].adpcm_history1_32;
                vgmstream->loop_ch[ch].adpcm_history2_32 = vgmstream->ch[ch].adpcm_history2_32;
            }
        }

        //TODO: improve
        /* loop codecs that need special handling, usually:
         * - on hit_loop, current offset is copied to loop_ch[].offset
         * - some codecs will overwrite loop_ch[].offset with a custom value
         * - loop_ch[] is copied to ch[] (with custom value)
         * - then codec will use ch[]'s offset
         * regular codecs may use copied loop_ch[] offset without issue */
        seek_codec(vgmstream);

        /* restore! */
        memcpy(vgmstream->ch, vgmstream->loop_ch, sizeof(VGMSTREAMCHANNEL) * vgmstream->channels);
        vgmstream->current_sample = vgmstream->loop_current_sample;
        vgmstream->samples_into_block = vgmstream->loop_samples_into_block;
        vgmstream->current_block_size = vgmstream->loop_block_size;
        vgmstream->current_block_samples = vgmstream->loop_block_samples;
        vgmstream->current_block_offset = vgmstream->loop_block_offset;
        vgmstream->next_block_offset = vgmstream->loop_next_block_offset;
        //vgmstream->pstate = vgmstream->lstate; /* play state is applied over loops */

        /* loop layouts (after restore, in case layout needs state manipulations) */
        switch(vgmstream->layout_type) {
            case layout_segmented:
                loop_layout_segmented(vgmstream, vgmstream->loop_current_sample);
                break;
            case layout_layered:
                loop_layout_layered(vgmstream, vgmstream->loop_current_sample);
                break;
            default:
                break;
        }

        return 1; /* looped */
    }


    /* is this the loop start? save if we haven't saved yet (right when first loop starts) */
    if (!vgmstream->hit_loop && vgmstream->current_sample == vgmstream->loop_start_sample) {
        /* save! */
        memcpy(vgmstream->loop_ch, vgmstream->ch, sizeof(VGMSTREAMCHANNEL)*vgmstream->channels);
        vgmstream->loop_current_sample = vgmstream->current_sample;
        vgmstream->loop_samples_into_block = vgmstream->samples_into_block;
        vgmstream->loop_block_size = vgmstream->current_block_size;
        vgmstream->loop_block_samples = vgmstream->current_block_samples;
        vgmstream->loop_block_offset = vgmstream->current_block_offset;
        vgmstream->loop_next_block_offset = vgmstream->next_block_offset;
        //vgmstream->lstate = vgmstream->pstate; /* play state is applied over loops */

        vgmstream->hit_loop = 1; /* info that loop is now ready to use */
    }

    return 0; /* not looped */
}
