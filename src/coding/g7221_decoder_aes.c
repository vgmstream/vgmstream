#include <stdlib.h>
#include "g7221_decoder_aes.h"

/* Namco's NUS AES is just standard AES-192 in ECB mode, so this can be swapped with another lib,
 * if more code needs AES. Most implementations out there either use pre-calculated look-up tables,
 * or calculate manually every AES op. Namco's code calculates tables first in a slightly different
 * layout, so it may be interesting as a sort of doc piece. */


struct s14aes_handle {
    /* substitution box look-up table and the inverse */
    uint8_t sbox[256];
    uint8_t ibox[256];
    /* helper for various tables, not too sure about it */
    uint8_t xors[256];
    /* round constant, though only sets up to 8 */
    uint8_t rcon[16];
    /* MixColumn(?) LUTs, unlike normal Rijndael which uses 4 tables: Td0a Td0b..., Td1a Td1b..., ...
     * layout is: Td0a Td1a Td2a Td3a, Td0b Td0b Td1b Td2b, ... (better for CPU cache?) */
    uint32_t tds[256*4];
    /* expanded roundkey, actual final key (192-bit keys only need up to 52 though) */
    uint32_t rk[64];
} ;


#define GET_U32LE(p) (((p)[0] << 0 ) | ((p)[1] << 8 ) | ((p)[2] << 16) | ((p)[3] << 24))
#define GET_U32BE(p) (((p)[3] << 0 ) | ((p)[2] << 8 ) | ((p)[1] << 16) | ((p)[0] << 24))
#define GET_B0(x)  (((x) >> 0 ) & 0xFF)
#define GET_B1(x)  (((x) >> 8 ) & 0xFF)
#define GET_B2(x)  (((x) >> 16) & 0xFF)
#define GET_B3(x)  (((x) >> 24) & 0xFF)


static void aes_key_expand(s14aes_handle* ctx, const uint8_t* key, uint32_t* rk) {
    int i;

    rk[0] = GET_U32LE(key + 0);
    rk[1] = GET_U32LE(key + 4);
    rk[2] = GET_U32LE(key + 8);
    rk[3] = GET_U32LE(key + 12);
    rk[4] = GET_U32LE(key + 16);
    rk[5] = GET_U32LE(key + 20);

    for (i = 6; i < 52; i++) {
        uint8_t temp0 = (rk[5] >> 0 ) & 0xFF;
        uint8_t temp1 = (rk[5] >> 8 ) & 0xFF;
        uint8_t temp2 = (rk[5] >> 16) & 0xFF;
        uint8_t temp3 = (rk[5] >> 24) & 0xFF;
        if (i == 6 * (i / 6)) {
            uint8_t sv = ctx->sbox[temp1];
            temp1 = ctx->sbox[temp2];
            temp2 = ctx->sbox[temp3];
            temp3 = ctx->sbox[temp0];
            temp0 = ctx->rcon[i / 6u - 1] ^ sv;
        }
        rk[6] = ((temp0 ^ ((rk[0] >> 0 ) & 0xFF)) << 0 ) |
                ((temp1 ^ ((rk[0] >> 8 ) & 0xFF)) << 8 ) |
                ((temp2 ^ ((rk[0] >> 16) & 0xFF)) << 16) |
                ((temp3 ^ ((rk[0] >> 24) & 0xFF)) << 24);
        rk++;
    }
}

static void aes_init_key(s14aes_handle* ctx, const uint8_t* key) {
    const uint8_t invcols[4][4] = {
        {0x0E,0x0B,0x0D,0x09},
        {0x09,0x0E,0x0B,0x0D},
        {0x0D,0x09,0x0E,0x0B}, 
        {0x0B,0x0D,0x09,0x0E}
    };
    unsigned int roundkey[52];
    int i, j, row, col, b;

    aes_key_expand(ctx, key, roundkey);

    for (i = 0; i < 4; i++) {
        ctx->rk[i] = ((roundkey[i] << 24) & 0xFF000000) |
                     ((roundkey[i] << 8 ) & 0x00FF0000) |
                     ((roundkey[i] >> 8 ) & 0x0000FF00) |
                     ((roundkey[i] >> 24) & 0x000000FF);
    }

    for (i = 4; i < 48; i += 4) {
        for (j = i; j < i + 4; j++) {
            uint32_t val = 0;

            for (row = 0; row < 4; row++) {
                uint8_t xv = 0;

                for (col = 0; col < 4; col++) {
                    uint16_t rv1 = 0;
                    uint16_t rv2 = (roundkey[j] >> (col * 8)) & 0xFF;
                    uint8_t ic = invcols[row][col];

                    for (b = 0; b < 8; b++) {
                        if (ic & (1 << b))
                            rv1 ^= rv2;
                        rv2 *= 2;
                    }

                    xv ^= rv1 ^ ctx->xors[rv1 >> 8];
                }

                val = (val << 8) | xv;
            }

            ctx->rk[j] = val;
        }
    }

    for (i = 48; i < 52; i++) {
        ctx->rk[i] = ((roundkey[i] << 24) & 0xFF000000) |
                     ((roundkey[i] << 8 ) & 0x00FF0000) |
                     ((roundkey[i] >> 8 ) & 0x0000FF00) |
                     ((roundkey[i] >> 24) & 0x000000FF);
    }
}

static void aes_init_state(s14aes_handle* ctx) {
    const uint8_t invcol[4] = {
        0x0E, 0x0B, 0x0D, 0x09
    };
    unsigned int *tds_ptr;
    uint8_t rcon[32];
    uint8_t box[256];
    int i, j, k, b;

    for (i = 0; i < 32; i++) {
        uint16_t rv;
        if (i >= 8) {
            rv = 128;
            for (j = 0; j < i - 7; j++) {
                rv *= 2;
                if (rv & 256)
                    rv ^= 0x11Bu;
            }
        }
        else {
            rv = 1 << i;
        }
        rcon[i] = rv;
    }

    for (i = 0; i < 256; i++) {
        uint8_t xv = 0;
        for (j = 0; j < 8; j++) {
            if (i & (1 << j))
                xv ^= rcon[j + 8];
        }
        ctx->xors[i] = xv;
    }

    tds_ptr = ctx->tds;
    for (i = 0; i < 256; i++) {
        uint32_t val = 0;
        for (j = 0; j < 4; j++) {
            uint16_t tv1 = 0;
            uint16_t tv2 = invcol[j];
            for (b = 0; b < 8; b++) {
                if (i & (1 << b))
                    tv1 ^= tv2;
                tv2 *= 2;
            }

            val = ((val >> 8u) & 0x00FFFFFF) | ((val << 24u) & 0xFF000000);
            val = ((uint8_t)tv1 ^ ctx->xors[tv1 >> 8]) | val;
        }

        val = ((val >> 16u) & 0x0000FFFF) | ((val << 16u) & 0xFFFF0000);
        for (j = 0; j < 4; j++) {
            *tds_ptr++ = val;
            val = ((val >> 8u) & 0x00FFFFFF) | ((val << 24u) & 0xFF000000);
        }
    }

    box[0] = 0;
    for (i = 1; i < 256; i++) {
        for (j = 1; j < 256; j++) {
            uint16_t bv1 = 0;
            uint16_t bv2 = j;
            for (k = 0; k < 8; k++) {
                if (i & (1 << k))
                    bv1 ^= bv2;
                bv2 *= 2;
            }

            if (((uint8_t)bv1 ^ ctx->xors[bv1 >> 8]) == 1)
                break;
        }
        box[i] = j;

        if (j == 256) /* ??? */
            return;
    }

    for (i = 0; i < 256; i += 16) {
        for (j = 0; j < 16; j++) {
            uint8_t val = 0;
            for (k = 0; k < 8; k++) {
                val |= box[i | j] & (1 << k);
                for (b = 0; b < 4; b++) {
                    uint8_t bv = box[i | j];
                    if (bv & (1 << ((b + k - 4) & 7)))
                        val ^= 1 << k;
                }
            }

            ctx->sbox[i + j] = val ^ 0x63;
            ctx->ibox[val ^ 0x63] = i + j;
        }
    }

    /* originally recalculated in Namco's code (inlined?) */
    for (i = 0; i < 8; i++) {
        ctx->rcon[i] = rcon[i];
    }
}

static void aes_decrypt_block(s14aes_handle* ctx, uint8_t* buf) {
    uint32_t s0, s1, s2, s3, t0, t1, t2, t3;
    uint8_t* ibox = ctx->ibox;
    uint32_t* tds = ctx->tds;
    uint32_t* rk = ctx->rk;

    s0 = rk[48] ^ GET_U32BE(buf + 0);
    s1 = rk[49] ^ GET_U32BE(buf + 4);
    s2 = rk[50] ^ GET_U32BE(buf + 8);
    s3 = rk[51] ^ GET_U32BE(buf + 12);

    t0 = rk[44] ^ tds[4 * ibox[GET_B0(s1)] + 3] ^ tds[4 * ibox[GET_B1(s2)] + 2] ^ tds[4 * ibox[GET_B2(s3)] + 1] ^ tds[4 * ibox[GET_B3(s0)] + 0];
    t1 = rk[45] ^ tds[4 * ibox[GET_B0(s2)] + 3] ^ tds[4 * ibox[GET_B1(s3)] + 2] ^ tds[4 * ibox[GET_B2(s0)] + 1] ^ tds[4 * ibox[GET_B3(s1)] + 0];
    t2 = rk[46] ^ tds[4 * ibox[GET_B0(s3)] + 3] ^ tds[4 * ibox[GET_B1(s0)] + 2] ^ tds[4 * ibox[GET_B2(s1)] + 1] ^ tds[4 * ibox[GET_B3(s2)] + 0];
    t3 = rk[47] ^ tds[4 * ibox[GET_B0(s0)] + 3] ^ tds[4 * ibox[GET_B1(s1)] + 2] ^ tds[4 * ibox[GET_B2(s2)] + 1] ^ tds[4 * ibox[GET_B3(s3)] + 0];

    s0 = rk[40] ^ tds[4 * ibox[GET_B0(t1)] + 3] ^ tds[4 * ibox[GET_B1(t2)] + 2] ^ tds[4 * ibox[GET_B2(t3)] + 1] ^ tds[4 * ibox[GET_B3(t0)] + 0];
    s1 = rk[41] ^ tds[4 * ibox[GET_B0(t2)] + 3] ^ tds[4 * ibox[GET_B1(t3)] + 2] ^ tds[4 * ibox[GET_B2(t0)] + 1] ^ tds[4 * ibox[GET_B3(t1)] + 0];
    s2 = rk[42] ^ tds[4 * ibox[GET_B0(t3)] + 3] ^ tds[4 * ibox[GET_B1(t0)] + 2] ^ tds[4 * ibox[GET_B2(t1)] + 1] ^ tds[4 * ibox[GET_B3(t2)] + 0];
    s3 = rk[43] ^ tds[4 * ibox[GET_B0(t0)] + 3] ^ tds[4 * ibox[GET_B1(t1)] + 2] ^ tds[4 * ibox[GET_B2(t2)] + 1] ^ tds[4 * ibox[GET_B3(t3)] + 0];

    t0 = rk[36] ^ tds[4 * ibox[GET_B0(s1)] + 3] ^ tds[4 * ibox[GET_B1(s2)] + 2] ^ tds[4 * ibox[GET_B2(s3)] + 1] ^ tds[4 * ibox[GET_B3(s0)] + 0];
    t1 = rk[37] ^ tds[4 * ibox[GET_B0(s2)] + 3] ^ tds[4 * ibox[GET_B1(s3)] + 2] ^ tds[4 * ibox[GET_B2(s0)] + 1] ^ tds[4 * ibox[GET_B3(s1)] + 0];
    t2 = rk[38] ^ tds[4 * ibox[GET_B0(s3)] + 3] ^ tds[4 * ibox[GET_B1(s0)] + 2] ^ tds[4 * ibox[GET_B2(s1)] + 1] ^ tds[4 * ibox[GET_B3(s2)] + 0];
    t3 = rk[39] ^ tds[4 * ibox[GET_B0(s0)] + 3] ^ tds[4 * ibox[GET_B1(s1)] + 2] ^ tds[4 * ibox[GET_B2(s2)] + 1] ^ tds[4 * ibox[GET_B3(s3)] + 0];

    s0 = rk[32] ^ tds[4 * ibox[GET_B0(t1)] + 3] ^ tds[4 * ibox[GET_B1(t2)] + 2] ^ tds[4 * ibox[GET_B2(t3)] + 1] ^ tds[4 * ibox[GET_B3(t0)] + 0];
    s1 = rk[33] ^ tds[4 * ibox[GET_B0(t2)] + 3] ^ tds[4 * ibox[GET_B1(t3)] + 2] ^ tds[4 * ibox[GET_B2(t0)] + 1] ^ tds[4 * ibox[GET_B3(t1)] + 0];
    s2 = rk[34] ^ tds[4 * ibox[GET_B0(t3)] + 3] ^ tds[4 * ibox[GET_B1(t0)] + 2] ^ tds[4 * ibox[GET_B2(t1)] + 1] ^ tds[4 * ibox[GET_B3(t2)] + 0];
    s3 = rk[35] ^ tds[4 * ibox[GET_B0(t0)] + 3] ^ tds[4 * ibox[GET_B1(t1)] + 2] ^ tds[4 * ibox[GET_B2(t2)] + 1] ^ tds[4 * ibox[GET_B3(t3)] + 0];

    t0 = rk[28] ^ tds[4 * ibox[GET_B0(s1)] + 3] ^ tds[4 * ibox[GET_B1(s2)] + 2] ^ tds[4 * ibox[GET_B2(s3)] + 1] ^ tds[4 * ibox[GET_B3(s0)] + 0];
    t1 = rk[29] ^ tds[4 * ibox[GET_B0(s2)] + 3] ^ tds[4 * ibox[GET_B1(s3)] + 2] ^ tds[4 * ibox[GET_B2(s0)] + 1] ^ tds[4 * ibox[GET_B3(s1)] + 0];
    t2 = rk[30] ^ tds[4 * ibox[GET_B0(s3)] + 3] ^ tds[4 * ibox[GET_B1(s0)] + 2] ^ tds[4 * ibox[GET_B2(s1)] + 1] ^ tds[4 * ibox[GET_B3(s2)] + 0];
    t3 = rk[31] ^ tds[4 * ibox[GET_B0(s0)] + 3] ^ tds[4 * ibox[GET_B1(s1)] + 2] ^ tds[4 * ibox[GET_B2(s2)] + 1] ^ tds[4 * ibox[GET_B3(s3)] + 0];

    s0 = rk[24] ^ tds[4 * ibox[GET_B0(t1)] + 3] ^ tds[4 * ibox[GET_B1(t2)] + 2] ^ tds[4 * ibox[GET_B2(t3)] + 1] ^ tds[4 * ibox[GET_B3(t0)] + 0];
    s1 = rk[25] ^ tds[4 * ibox[GET_B0(t2)] + 3] ^ tds[4 * ibox[GET_B1(t3)] + 2] ^ tds[4 * ibox[GET_B2(t0)] + 1] ^ tds[4 * ibox[GET_B3(t1)] + 0];
    s2 = rk[26] ^ tds[4 * ibox[GET_B0(t3)] + 3] ^ tds[4 * ibox[GET_B1(t0)] + 2] ^ tds[4 * ibox[GET_B2(t1)] + 1] ^ tds[4 * ibox[GET_B3(t2)] + 0];
    s3 = rk[27] ^ tds[4 * ibox[GET_B0(t0)] + 3] ^ tds[4 * ibox[GET_B1(t1)] + 2] ^ tds[4 * ibox[GET_B2(t2)] + 1] ^ tds[4 * ibox[GET_B3(t3)] + 0];

    t0 = rk[20] ^ tds[4 * ibox[GET_B0(s1)] + 3] ^ tds[4 * ibox[GET_B1(s2)] + 2] ^ tds[4 * ibox[GET_B2(s3)] + 1] ^ tds[4 * ibox[GET_B3(s0)] + 0];
    t1 = rk[21] ^ tds[4 * ibox[GET_B0(s2)] + 3] ^ tds[4 * ibox[GET_B1(s3)] + 2] ^ tds[4 * ibox[GET_B2(s0)] + 1] ^ tds[4 * ibox[GET_B3(s1)] + 0];
    t2 = rk[22] ^ tds[4 * ibox[GET_B0(s3)] + 3] ^ tds[4 * ibox[GET_B1(s0)] + 2] ^ tds[4 * ibox[GET_B2(s1)] + 1] ^ tds[4 * ibox[GET_B3(s2)] + 0];
    t3 = rk[23] ^ tds[4 * ibox[GET_B0(s0)] + 3] ^ tds[4 * ibox[GET_B1(s1)] + 2] ^ tds[4 * ibox[GET_B2(s2)] + 1] ^ tds[4 * ibox[GET_B3(s3)] + 0];

    s0 = rk[16] ^ tds[4 * ibox[GET_B0(t1)] + 3] ^ tds[4 * ibox[GET_B1(t2)] + 2] ^ tds[4 * ibox[GET_B2(t3)] + 1] ^ tds[4 * ibox[GET_B3(t0)] + 0];
    s1 = rk[17] ^ tds[4 * ibox[GET_B0(t2)] + 3] ^ tds[4 * ibox[GET_B1(t3)] + 2] ^ tds[4 * ibox[GET_B2(t0)] + 1] ^ tds[4 * ibox[GET_B3(t1)] + 0];
    s2 = rk[18] ^ tds[4 * ibox[GET_B0(t3)] + 3] ^ tds[4 * ibox[GET_B1(t0)] + 2] ^ tds[4 * ibox[GET_B2(t1)] + 1] ^ tds[4 * ibox[GET_B3(t2)] + 0];
    s3 = rk[19] ^ tds[4 * ibox[GET_B0(t0)] + 3] ^ tds[4 * ibox[GET_B1(t1)] + 2] ^ tds[4 * ibox[GET_B2(t2)] + 1] ^ tds[4 * ibox[GET_B3(t3)] + 0];

    t0 = rk[12] ^ tds[4 * ibox[GET_B0(s1)] + 3] ^ tds[4 * ibox[GET_B1(s2)] + 2] ^ tds[4 * ibox[GET_B2(s3)] + 1] ^ tds[4 * ibox[GET_B3(s0)] + 0];
    t1 = rk[13] ^ tds[4 * ibox[GET_B0(s2)] + 3] ^ tds[4 * ibox[GET_B1(s3)] + 2] ^ tds[4 * ibox[GET_B2(s0)] + 1] ^ tds[4 * ibox[GET_B3(s1)] + 0];
    t2 = rk[14] ^ tds[4 * ibox[GET_B0(s3)] + 3] ^ tds[4 * ibox[GET_B1(s0)] + 2] ^ tds[4 * ibox[GET_B2(s1)] + 1] ^ tds[4 * ibox[GET_B3(s2)] + 0];
    t3 = rk[15] ^ tds[4 * ibox[GET_B0(s0)] + 3] ^ tds[4 * ibox[GET_B1(s1)] + 2] ^ tds[4 * ibox[GET_B2(s2)] + 1] ^ tds[4 * ibox[GET_B3(s3)] + 0];

    s0 = rk[8 ] ^ tds[4 * ibox[GET_B0(t1)] + 3] ^ tds[4 * ibox[GET_B1(t2)] + 2] ^ tds[4 * ibox[GET_B2(t3)] + 1] ^ tds[4 * ibox[GET_B3(t0)] + 0];
    s1 = rk[9 ] ^ tds[4 * ibox[GET_B0(t2)] + 3] ^ tds[4 * ibox[GET_B1(t3)] + 2] ^ tds[4 * ibox[GET_B2(t0)] + 1] ^ tds[4 * ibox[GET_B3(t1)] + 0];
    s2 = rk[10] ^ tds[4 * ibox[GET_B0(t3)] + 3] ^ tds[4 * ibox[GET_B1(t0)] + 2] ^ tds[4 * ibox[GET_B2(t1)] + 1] ^ tds[4 * ibox[GET_B3(t2)] + 0];
    s3 = rk[11] ^ tds[4 * ibox[GET_B0(t0)] + 3] ^ tds[4 * ibox[GET_B1(t1)] + 2] ^ tds[4 * ibox[GET_B2(t2)] + 1] ^ tds[4 * ibox[GET_B3(t3)] + 0];

    t0 = rk[4 ] ^ tds[4 * ibox[GET_B0(s1)] + 3] ^ tds[4 * ibox[GET_B1(s2)] + 2] ^ tds[4 * ibox[GET_B2(s3)] + 1] ^ tds[4 * ibox[GET_B3(s0)] + 0];
    t1 = rk[5 ] ^ tds[4 * ibox[GET_B0(s2)] + 3] ^ tds[4 * ibox[GET_B1(s3)] + 2] ^ tds[4 * ibox[GET_B2(s0)] + 1] ^ tds[4 * ibox[GET_B3(s1)] + 0];
    t2 = rk[6 ] ^ tds[4 * ibox[GET_B0(s3)] + 3] ^ tds[4 * ibox[GET_B1(s0)] + 2] ^ tds[4 * ibox[GET_B2(s1)] + 1] ^ tds[4 * ibox[GET_B3(s2)] + 0];
    t3 = rk[7 ] ^ tds[4 * ibox[GET_B0(s0)] + 3] ^ tds[4 * ibox[GET_B1(s1)] + 2] ^ tds[4 * ibox[GET_B2(s2)] + 1] ^ tds[4 * ibox[GET_B3(s3)] + 0];

    buf[0 ] = GET_B3(rk[0]) ^ ibox[GET_B3(t0)];
    buf[1 ] = GET_B2(rk[0]) ^ ibox[GET_B2(t3)];
    buf[2 ] = GET_B1(rk[0]) ^ ibox[GET_B1(t2)];
    buf[3 ] = GET_B0(rk[0]) ^ ibox[GET_B0(t1)];
    buf[4 ] = GET_B3(rk[1]) ^ ibox[GET_B3(t1)];
    buf[5 ] = GET_B2(rk[1]) ^ ibox[GET_B2(t0)];
    buf[6 ] = GET_B1(rk[1]) ^ ibox[GET_B1(t3)];
    buf[7 ] = GET_B0(rk[1]) ^ ibox[GET_B0(t2)];
    buf[8 ] = GET_B3(rk[2]) ^ ibox[GET_B3(t2)];
    buf[9 ] = GET_B2(rk[2]) ^ ibox[GET_B2(t1)];
    buf[10] = GET_B1(rk[2]) ^ ibox[GET_B1(t0)];
    buf[11] = GET_B0(rk[2]) ^ ibox[GET_B0(t3)];
    buf[12] = GET_B3(rk[3]) ^ ibox[GET_B3(t3)];
    buf[13] = GET_B2(rk[3]) ^ ibox[GET_B2(t2)];
    buf[14] = GET_B1(rk[3]) ^ ibox[GET_B1(t1)];
    buf[15] = GET_B0(rk[3]) ^ ibox[GET_B0(t0)];
}

/* **************************** */

s14aes_handle* s14aes_init(void) {
    s14aes_handle* ctx = malloc(sizeof(s14aes_handle));
    if (!ctx) goto fail;

    aes_init_state(ctx);

    return ctx;
fail:
    return NULL;
}

void s14aes_close(s14aes_handle* ctx) {
    free(ctx);
}

void s14aes_set_key(s14aes_handle* ctx, const uint8_t* key) {
    aes_init_key(ctx, key);
}

void s14aes_decrypt(s14aes_handle* ctx, uint8_t* buf) {
    aes_decrypt_block(ctx, buf);
}
