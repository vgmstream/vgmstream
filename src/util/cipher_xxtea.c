#include <stdlib.h>
#include "cipher_xxtea.h"
#include "reader_get.h"
#include "reader_put.h"


// Original MX is pasted and uses stuff declared below, rather than working like a function.
// Separate steps here but probably the same (better?) after compiler optimizations.
// #define MX ((((z >> 5) ^ (y << 2)) + ((y >> 3) ^ (z << 4))) ^ ((sum ^ y) + (key[(p&3) ^ e] ^ z)))
// #define XXTEA_DELTA 0x9e3779b9

static inline uint32_t xxtea_mx(uint32_t y, uint32_t z, uint32_t sum, unsigned int p, unsigned int e, const uint32_t* key) {
    int index = (p & 3) ^ e;
    uint32_t xor1 = (z >> 5) ^ (y << 2);
    uint32_t xor2 = (y >> 3) ^ (z << 4);
    uint32_t xor3 = (sum ^ y);
    uint32_t xor4 = key[index] ^ z;
    
    return (xor1 + xor2) ^ (xor3 + xor4);
}

void xxtea_decrypt(uint8_t* v, uint32_t size, const uint32_t* key) {
    const uint32_t xxtea_delta = 0x9e3779b9;

    if (size <= 0x04)
        return;
    uint32_t y, z, sum;
    unsigned n = size >> 2; /* in ints */
    unsigned rounds = 6 + 52 / n;
    sum = rounds * xxtea_delta;

    y = get_u32le(v + 0 * 4);
    do {
        unsigned int e = (sum >> 2) & 3;
        unsigned int p;
        for (p = n - 1; p > 0; p--) {
          z = get_u32le(v + (p - 1) * 4);
          y = get_u32le(v + p * 4) - xxtea_mx(y, z, sum, p, e, key);
          put_u32le(v + p * 4, y);
        }
        z = get_u32le(v + (n - 1) * 4);
        y = get_u32le(v + 0 * 4) - xxtea_mx(y, z, sum, p, e, key);
        put_u32le(v + 0 * 4, y);
        sum -= xxtea_delta;
    }
    while (--rounds);
}
