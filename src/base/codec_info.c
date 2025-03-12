#include "codec_info.h"

//TODO: move to root folder?
extern const codec_info_t ka1a_decoder;
extern const codec_info_t ubimpeg_decoder;
extern const codec_info_t hca_decoder;
#ifdef VGM_USE_VORBIS
extern const codec_info_t vorbis_custom_decoder;
#endif

const codec_info_t* codec_get_info(VGMSTREAM* v) {
    switch(v->coding_type) {
        case coding_CRI_HCA:
            return &hca_decoder;
        case coding_KA1A:
            return &ka1a_decoder;
        case coding_UBI_MPEG:
            return &ubimpeg_decoder;
#ifdef VGM_USE_VORBIS
        case coding_VORBIS_custom:
            return &vorbis_custom_decoder;
#endif
        default:
            return NULL;
    }
}
