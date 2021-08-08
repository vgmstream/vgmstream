#ifndef _RELIC_DECODER_LIB_H_
#define _RELIC_DECODER_LIB_H_

#include <stdint.h>

#define RELIC_BUFFER_SIZE 0x104
#define RELIC_SAMPLES_PER_FRAME 512

typedef struct relic_handle_t relic_handle_t;

relic_handle_t* relic_init(int channels, int bitrate, int codec_rate);

void relic_free(relic_handle_t* handle);

void relic_reset(relic_handle_t* handle);

int relic_get_frame_size(relic_handle_t* handle);

int relic_decode_frame(relic_handle_t* handle, uint8_t* buf, int channel);

void relic_get_pcm16(relic_handle_t* handle, int16_t* outbuf, int32_t samples, int32_t skip);

#endif/*_RELIC_DECODER_LIB_H_ */
