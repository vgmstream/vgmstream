#ifndef _CIRCUS_DECODER_LIB_H_
#define _CIRCUS_DECODER_LIB_H_

#include "../streamfile.h"

typedef struct circus_handle_t circus_handle_t;

circus_handle_t* circus_init(off_t start, uint8_t codec, uint8_t flags);

void circus_free(circus_handle_t* handle);

void circus_reset(circus_handle_t* handle);

int circus_decode_frame(circus_handle_t* handle, STREAMFILE* sf, int16_t** p_buf, int* p_buf_samples_all);

#endif
