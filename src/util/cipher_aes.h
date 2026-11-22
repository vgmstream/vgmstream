#ifndef _CIPHER_AES_H_
#define _CIPHER_AES_H_
#include <inttypes.h>

typedef struct aes_handle_t aes_handle_t;

/* init/close handle (AES-192 in ECB mode) */
aes_handle_t* aes_init_192_ecb(void);

void aes_free(aes_handle_t* ctx);

/* set new key (can be called multiple times) */
void aes_set_key(aes_handle_t* ctx, const uint8_t* key);

/* decrypt a single 0x10 block */
void aes_decrypt(aes_handle_t* ctx, uint8_t* buf);

#endif
