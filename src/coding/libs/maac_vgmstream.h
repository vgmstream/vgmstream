#ifndef MAAC_VGMSTREAM_H
#define MAAC_VGMSTREAM_H

/* miniaac 1.0.0 by John Regan
 * - https://buffering.party/software/miniaac/
 *
 * Decoder is mainly based on demos/decode-raw*.c
 * lib only decodes AAC-LC though (most common, other modes are usually for lower bitrates).
 *
 * Uses miniaac for simplicity as it provides an API to handle raw AAC frames in arbitrary chunks of data.
 * Often decoders expect single frames, with frame sizes provided by ADTS or MP4 headers, but for pure raw AAC
 * it's not possible to get the frame size unless you read the full frame's bitstream, which is rather complex
 * with many subtypes and huffman codes.
 *
 * Possibly could use FDK-AAC using:
 *   aacDecoder_Fill(...)
 *   aacDecoder_DecodeFrame(...)
 *
 * Or maybe FFmpeg, manually setting codec ID + passing AudioSpecificConfig (formatted AAC info) as extradata:
 *   ...
 *   codec = avcodec_find_decoder(AV_CODEC_ID_AAC);
 *   codecCtx = avcodec_alloc_context3(codec);
 *   codecCtx->extradata_size = sizeof(asc);
 *   codecCtx->extradata = av_malloc(ctx->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
 *   ...
 * (but would need handling frame boundaries based on avpkt's read data)
 */

#define MAAC_PRIVATE static
#define MAAC_IMPLEMENTATION
#include "maac.h"

#define MAAC_EXTRAS_IMPLEMENTATION
#include "maac_extras.h"

#endif
