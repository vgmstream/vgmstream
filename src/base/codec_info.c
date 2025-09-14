#include "codec_info.h"

//TODO: move to root folder?
extern const codec_info_t ka1a_decoder;
extern const codec_info_t ubimpeg_decoder;
extern const codec_info_t hca_decoder;
#ifdef VGM_USE_VORBIS
extern const codec_info_t ogg_vorbis_decoder;
extern const codec_info_t vorbis_custom_decoder;
#endif
extern const codec_info_t tac_decoder;
extern const codec_info_t compresswave_decoder;
extern const codec_info_t speex_decoder;
extern const codec_info_t imuse_decoder;
extern const codec_info_t mio_decoder;
extern const codec_info_t pcm32_decoder;
extern const codec_info_t pcm24_decoder;
extern const codec_info_t pcmfloat_decoder;
#ifdef VGM_USE_FFMPEG
extern const codec_info_t ffmpeg_decoder;
#endif
#ifdef VGM_USE_ATRAC9
extern const codec_info_t atrac9_decoder;
#endif
#ifdef VGM_USE_CELT
extern const codec_info_t celt_fsb_decoder;
#endif
#ifdef VGM_USE_MPEG
extern const codec_info_t mpeg_decoder;
#endif
extern const codec_info_t relic_decoder;
extern const codec_info_t binka_decoder;

extern const codec_info_t cf_df_v40_decoder;
extern const codec_info_t cf_df_v41_decoder;

const codec_info_t* codec_get_info(VGMSTREAM* v) {
    switch(v->coding_type) {
        case coding_CRI_HCA:
            return &hca_decoder;
        case coding_KA1A:
            return &ka1a_decoder;
        case coding_UBI_MPEG:
            return &ubimpeg_decoder;
#ifdef VGM_USE_VORBIS
        case coding_OGG_VORBIS:
            return &ogg_vorbis_decoder;
        case coding_VORBIS_custom:
            return &vorbis_custom_decoder;
#endif
        case coding_TAC:
            return &tac_decoder;
        case coding_COMPRESSWAVE:
            return &compresswave_decoder;
#ifdef VGM_USE_SPEEX
        case coding_SPEEX:
            return &speex_decoder;
#endif
        case coding_IMUSE:
            return &imuse_decoder;
        case coding_MIO:
            return &mio_decoder;
        case coding_BINKA:
            return &binka_decoder;
        case coding_PCM32LE:
            return &pcm32_decoder;
        case coding_PCM24LE:
        case coding_PCM24BE:
            return &pcm24_decoder;
        case coding_PCMFLOAT:
            return &pcmfloat_decoder;
#ifdef VGM_USE_FFMPEG
        case coding_FFmpeg:
            return &ffmpeg_decoder;
#endif
#ifdef VGM_USE_ATRAC9
        case coding_ATRAC9:
            return &atrac9_decoder;
#endif
#ifdef VGM_USE_CELT
        case coding_CELT_FSB:
            return &celt_fsb_decoder;
#endif
#ifdef VGM_USE_MPEG
        case coding_MPEG_custom:
        case coding_MPEG_ealayer3:
        case coding_MPEG_layer1:
        case coding_MPEG_layer2:
        case coding_MPEG_layer3:
            return &mpeg_decoder;
#endif
        case coding_RELIC:
            return &relic_decoder;
        case coding_CF_DF_ADPCM_V40:
            return &cf_df_v40_decoder;
        case coding_CF_DF_DPCM_V41:
            return &cf_df_v41_decoder;
        default:
            return NULL;
    }
}
