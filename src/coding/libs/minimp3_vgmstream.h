#ifndef MINIMP3_VGMSTREAM_H
#define MINIMP3_VGMSTREAM_H

#define MINIMP3_FLOAT_OUTPUT
#define MINIMP3_NO_SIMD
//#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

#define UBIMPEG_SURR_NONE 0
#define UBIMPEG_SURR_FAKE 1
#define UBIMPEG_SURR_FULL 2


/* Based on minimp3's mp3dec_decode_frame but simplified, since Ubi-MPEG only uses Layer II and optional surround frames 
 * - dequantize mp3_main's coefs
 * - dequantize mp3_surr's coefs (if surr_mode is 2)
 * - mix surr coefs
 * - finally convert to pcm
 * (not sure if fully decoding both frames separately then adding would be equivalent, but OG code does mix them first)
 */
int mp3dec_decode_frame_ubimpeg(
        mp3dec_t *dec_main, const uint8_t *mp3_main, int mp3_main_bytes, mp3dec_frame_info_t *info_main,
        mp3dec_t *dec_surr, const uint8_t *mp3_surr, int mp3_surr_bytes, mp3dec_frame_info_t *info_surr,
        int surr_mode,
        mp3d_sample_t *pcm);

#endif
