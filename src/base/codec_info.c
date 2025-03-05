#include "codec_info.h"

//TODO: move to root folder?
extern const codec_info_t ka1a_decoder;
extern const codec_info_t ubimpeg_decoder;


const codec_info_t* codec_get_info(VGMSTREAM* v) {
    switch(v->coding_type) {
        case coding_KA1A:
            return &ka1a_decoder;
        case coding_UBI_MPEG:
            return &ubimpeg_decoder;

        default:
            return NULL;
    }
}
