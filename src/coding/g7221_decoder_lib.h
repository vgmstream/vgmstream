/*
    Interface to Namco G.722.1 decoder
*/
#ifndef _G7221_DECODER_LIB_H
#define _G7221_DECODER_LIB_H

#include <stdint.h>

/* forward definition for the opaque handle object */
typedef struct g7221_handle g7221_handle;

/* return a handle for decoding on successful init, NULL on failure */
g7221_handle* g7221_init(int bytes_per_frame);

/* decode a frame, at code_words, into 16-bit PCM in sample_buffer. returns <0 on error */
int g7221_decode_frame(g7221_handle* handle, uint8_t* data, int16_t* out_samples);

#if 0
/* decodes an empty frame after no more data is found (may be used to "drain" window samples */
int g7221_decode_empty(g7221_handle* handle, int16_t* out_samples);
#endif

/* reset the decoder to its initial state */
void g7221_reset(g7221_handle* handle);

/* free resources */
void g7221_free(g7221_handle* handle);

/* set new key (ignores key on failure). returns <0 on error */
int g7221_set_key(g7221_handle* handle, const uint8_t* key);

#endif
