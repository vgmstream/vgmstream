#ifndef _G7221_DECODER_AES_H
#define _G7221_DECODER_AES_H

#include <stdint.h>

typedef struct s14aes_handle s14aes_handle;

/* init/close handle (AES-192 in ECB mode) */
s14aes_handle* s14aes_init(void);

void s14aes_close(s14aes_handle* ctx);

/* set new key (can be called multiple times) */
void s14aes_set_key(s14aes_handle* ctx, const uint8_t* key);

/* decrypt a single 0x10 block */
void s14aes_decrypt(s14aes_handle* ctx, uint8_t* buf);

#endif
