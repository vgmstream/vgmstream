#include "codec_info.h"

//TODO: move to root folder?

const codec_info_t* codec_get_info(VGMSTREAM* v) {
    switch(v->coding_type) {
        case coding_CRI_HCA:
            extern const codec_info_t hca_decoder;
            return &hca_decoder;

        case coding_KA1A:
            extern const codec_info_t ka1a_decoder;
            return &ka1a_decoder;

        case coding_UBI_MPEG:
            extern const codec_info_t ubimpeg_decoder;
            return &ubimpeg_decoder;

#ifdef VGM_USE_VORBIS
        case coding_OGG_VORBIS:
            extern const codec_info_t ogg_vorbis_decoder;
            return &ogg_vorbis_decoder;

        case coding_VORBIS_custom:
            extern const codec_info_t vorbis_custom_decoder;
            return &vorbis_custom_decoder;
#endif

        case coding_TAC:
            extern const codec_info_t tac_decoder;
            return &tac_decoder;

        case coding_COMPRESSWAVE:
            extern const codec_info_t compresswave_decoder;
            return &compresswave_decoder;

#ifdef VGM_USE_SPEEX
        case coding_SPEEX:
            extern const codec_info_t speex_decoder;
            return &speex_decoder;
#endif

        case coding_IMUSE:
            extern const codec_info_t imuse_decoder;
            return &imuse_decoder;

        case coding_MIO:
            extern const codec_info_t mio_decoder;
            return &mio_decoder;

        case coding_BINKA:
            extern const codec_info_t binka_decoder;
            return &binka_decoder;

        case coding_PCM32LE:
            extern const codec_info_t pcm32_decoder;
            return &pcm32_decoder;

        case coding_PCM24LE:
        case coding_PCM24BE:
            extern const codec_info_t pcm24_decoder;
            return &pcm24_decoder;

        case coding_PCMFLOAT:
            extern const codec_info_t pcmfloat_decoder;
            return &pcmfloat_decoder;

#ifdef VGM_USE_FFMPEG
        case coding_FFmpeg:
            extern const codec_info_t ffmpeg_decoder;
            return &ffmpeg_decoder;
#endif
#ifdef VGM_USE_ATRAC9
        case coding_ATRAC9:
            extern const codec_info_t atrac9_decoder;
            return &atrac9_decoder;
#endif
#ifdef VGM_USE_CELT
        case coding_CELT_FSB:
            extern const codec_info_t celt_fsb_decoder;
            return &celt_fsb_decoder;
#endif

#ifdef VGM_USE_MPEG
        case coding_MPEG_custom:
        case coding_MPEG_ealayer3:
        case coding_MPEG_layer1:
        case coding_MPEG_layer2:
        case coding_MPEG_layer3:
            extern const codec_info_t mpeg_decoder;
            return &mpeg_decoder;
#endif

        case coding_RELIC:
            extern const codec_info_t relic_decoder;
            return &relic_decoder;

        case coding_CF_DF_ADPCM_V40:
            extern const codec_info_t cf_df_v40_decoder;
            return &cf_df_v40_decoder;

        case coding_CF_DF_DPCM_V41:
            extern const codec_info_t cf_df_v41_decoder;
            return &cf_df_v41_decoder;

        case coding_CF_DF_ADPCM_v5:
            extern const codec_info_t cf_df_v5_v40_decoder;
            return &cf_df_v5_v40_decoder;

        case coding_AAC_raw:
            extern const codec_info_t aac_decoder;
            return &aac_decoder;

        default:
            return NULL;
    }
}
