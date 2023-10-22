#ifndef _CIPHER_BLOWFISH_H_
#define _CIPHER_BLOWFISH_H_
#include <inttypes.h>

typedef struct blowfish_ctx blowfish_ctx;

blowfish_ctx* blowfish_init_ecb(uint8_t* key, int key_len);
void blowfish_encrypt(blowfish_ctx *ctx, uint32_t* xl, uint32_t* xr);
void blowfish_decrypt(blowfish_ctx *ctx, uint32_t* xl, uint32_t* xr);
void blowfish_free(blowfish_ctx* ctx);

/* assumed block size is at least 0x08 */
void blowfish_decrypt_ecb(blowfish_ctx* ctx, uint8_t* block);
#endif
