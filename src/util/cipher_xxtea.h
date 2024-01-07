#ifndef _CIPHER_XXTEA_H_
#define _CIPHER_XXTEA_H_
#include <inttypes.h>

/* Decrypts xxtea blocks.
 * Mostly the standard implementation with minor cleanup. Official xxtea decrypts + encrypts
 * in the same function using a goofy "negative size means decrypt" flag, while this just
 * decrypts. OG code also assumes 32b little endian.
 *
 * Note that xxtea decrypts big block chunks and needs exact sizes to work
 * (so 0x1000 encrypted bytes needs to be decrypted as 0x1000 bytes, can't be 0x800 + 0x800).
 * 
 * v: data buf
 * size: buf size
 * key: 0x10 key converted into 4 u32le ints
 */
void xxtea_decrypt(uint8_t* v, uint32_t size, const uint32_t* key);
#endif
