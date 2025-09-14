#include <math.h>
#include "binka_transform.h"

/* over-optimized iDCT (type III) / iDFT based on decompilation.
 * Probably only interesting as an historical artifact (note that audio doesn't use all functions).
 *
 * Somewhat related: https://fgiesen.wordpress.com/2010/11/05/planar-rotations-and-the-dct/
 */ 
 //TODO: cleanup, restore inline'd functions/fors, renames
 //TODO: maybe replace with a more standard implementation

static void swap_2(float* coefs, int from, int to) {
    float v = coefs[from];
    coefs[from] = coefs[to];
    coefs[to] = v;
}

static inline void transform_4_dct(float* coefs) {
    swap_2(coefs, 2, 8);
    swap_2(coefs, 3, 9);
    swap_2(coefs, 7, 13);
    swap_2(coefs, 6, 12);
}

static void transform_4(float* coefs) {
    float v0 = coefs[0];
    float v1 = coefs[1];
    float v2 = coefs[2];
    float v3 = coefs[3];

    coefs[0] = v0 + v2;
    coefs[1] = v1 + v3;
    coefs[2] = v0 - v2;
    coefs[3] = v1 - v3;
}

static void transform_8_dct(float* coefs) {
    float v0 = coefs[0] + coefs[4];
    float v1 = coefs[0] - coefs[4];
    float v2 = coefs[1] + coefs[5];
    float v3 = coefs[1] - coefs[5];
    float v4 = coefs[2] + coefs[6];
    float v5 = coefs[2] - coefs[6];
    float v6 = coefs[3] + coefs[7];
    float v7 = coefs[3] - coefs[7];

    coefs[0] = v0 + v4;
    coefs[1] = v2 + v6;
    coefs[2] = v1 - v7;
    coefs[3] = v3 + v5;
    coefs[4] = v0 - v4;
    coefs[5] = v2 - v6;
    coefs[6] = v1 + v7;
    coefs[7] = v3 - v5;
}

static void transform_8_dft(float* coefs) {
    float v0 = coefs[0] + coefs[4];
    float v1 = coefs[0] - coefs[4];
    float v2 = coefs[1] + coefs[5];
    float v3 = coefs[1] - coefs[5];
    float v4 = coefs[2] + coefs[6];
    float v5 = coefs[2] - coefs[6];
    float v6 = coefs[3] + coefs[7];
    float v7 = coefs[3] - coefs[7];

    coefs[0] = v0 + v4;
    coefs[1] = v2 + v6;
    coefs[2] = v1 + v7;
    coefs[3] = v3 - v5;
    coefs[4] = v0 - v4;
    coefs[5] = v2 - v6;
    coefs[6] = v1 - v7;
    coefs[7] = v3 + v5;
}

static void transform_16_dft(float* coefs) {
    float c2  = coefs[2];
    float c3  = coefs[3];
    float c4  = coefs[4];
    float c5  = coefs[5];
    float c6  = coefs[6];
    float c7  = coefs[7];
    float c8  = coefs[8];
    float c9  = coefs[9];
    float c10 = coefs[10];
    float c11 = coefs[11];
    float c12 = coefs[12];
    float c13 = coefs[13];
    float c14 = coefs[14];
    float c15 = coefs[15];

    coefs[2]  = c14;
    coefs[3]  = c15;
    coefs[4]  = c6;
    coefs[5]  = c7;
    coefs[6]  = c10;
    coefs[7]  = c11;
    coefs[8]  = c2;
    coefs[9]  = c3;
    coefs[10] = c12;
    coefs[11] = c13;
    coefs[12] = c4;
    coefs[13] = c5;
    coefs[14] = c8;
    coefs[15] = c9;
}

static void transform_32_swap(float* coefs) {
    swap_2(coefs, 2, 16);
    swap_2(coefs, 3, 17);
    swap_2(coefs, 4, 8);
    swap_2(coefs, 5, 9);
    swap_2(coefs, 6, 24);
    swap_2(coefs, 7, 25);
    swap_2(coefs, 10, 20);
    swap_2(coefs, 11, 21);
    swap_2(coefs, 14, 28);
    swap_2(coefs, 15, 29);
    swap_2(coefs, 22, 26);
    swap_2(coefs, 23, 27);
}

static void transform_32_dft(float* coefs) {
    float c2  = coefs[2];
    float c3  = coefs[3];
    float c4  = coefs[4];
    float c5  = coefs[5];
    float c6  = coefs[6];
    float c7  = coefs[7];
    float c8  = coefs[8];
    float c9  = coefs[9];
    float c10 = coefs[10];
    float c11 = coefs[11];
    float c12 = coefs[12];
    float c13 = coefs[13];
    float c14 = coefs[14];
    float c15 = coefs[15];
    float c16 = coefs[16];
    float c17 = coefs[17];
    float c18 = coefs[18];
    float c19 = coefs[19];
    float c20 = coefs[20];
    float c21 = coefs[21];
    float c22 = coefs[22];
    float c23 = coefs[23];
    float c24 = coefs[24];
    float c25 = coefs[25];
    float c26 = coefs[26];
    float c27 = coefs[27];
    float c28 = coefs[28];
    float c29 = coefs[29];
    float c30 = coefs[30];
    float c31 = coefs[31];

    coefs[2]  = c30;
    coefs[3]  = c31;
    coefs[4]  = c14;
    coefs[5]  = c15;
    coefs[6]  = c22;
    coefs[7]  = c23;
    coefs[8]  = c6;
    coefs[9]  = c7;
    coefs[10] = c26;
    coefs[11] = c27;
    coefs[12] = c10;
    coefs[13] = c11;
    coefs[14] = c18;
    coefs[15] = c19;
    coefs[16] = c2;
    coefs[17] = c3;
    coefs[18] = c28;
    coefs[19] = c29;
    coefs[20] = c12;
    coefs[21] = c13;
    coefs[22] = c20;
    coefs[23] = c21;
    coefs[24] = c4;
    coefs[25] = c5;
    coefs[26] = c24;
    coefs[27] = c25;
    coefs[28] = c8;
    coefs[29] = c9;
    coefs[30] = c16;
    coefs[31] = c17;
}

static void rotation_32_a(float* coefs, const float* table) {
    float t1 = table[1];
    float t2 = table[2] * t1;
    float t3 = table[2] + t2;

    float a0 = coefs[0] - coefs[16];
    float a1 = coefs[0] + coefs[16];
    float a2 = coefs[1] + coefs[17];
    float a3 = coefs[1] - coefs[17];
    float a4 = coefs[8] - coefs[24];
    float a5 = coefs[8] + coefs[24];
    float a6 = coefs[9] - coefs[25];
    float a7 = coefs[9] + coefs[25];
    float a8 = coefs[10] + coefs[26];
    float a9 = coefs[10] - coefs[26];

    float b0, b1, b2, b3, b4, b5, b6, b7;
    float c0, c1, c2, c3, c4, c5, c6, c7;
    float d0, d1, d2, d3, d4, d5, d6, d7;
    float e1, e0;

    b1 = a5 + a1;
    b0 = a7 + a2;
    a1 = a1 - a5;
    a2 = a2 - a7;
    b2 = a4 + a3;
    a3 = a3 - a4;
    a7 = a0 - a6;
    a6 = a6 + a0;

    b3 = coefs[2] + coefs[18];
    a0 = coefs[2] - coefs[18];
    a4 = coefs[11] + coefs[27];
    b5 = coefs[11] - coefs[27];
    b6 = coefs[3] + coefs[19];
    b7 = coefs[3] - coefs[19];

    b4 = a8 + b3;
    a5 = a4 + b6;
    b3 = b3 - a8;
    b6 = b6 - a4;
    a4 = a0 - b5;
    b5 = b5 + a0;
    a8 = a9 + b7;
    b7 = b7 - a9;
    c0 = a4 * t3 - a8 * t2;
    c1 = a8 * t3 + a4 * t2;
    c2 = b5 * t3 + b7 * t2;
    c3 = b5 * t2 - b7 * t3;

    b7 = coefs[4] - coefs[20];
    c4 = coefs[4] + coefs[20];
    c5 = coefs[5] + coefs[21];
    c6 = coefs[5] - coefs[21];
    a8 = coefs[12] + coefs[28];
    a9 = coefs[13] + coefs[29];
    a0 = coefs[12] - coefs[28];
    b5 = coefs[13] - coefs[29];

    a4 = a8 + c4;
    c7 = a0 + c6;
    c6 = c6 - a0;
    c4 = c4 - a8;
    a8 = a9 + c5;
    c5 = c5 - a9;
    a9 = b7 - b5;
    b5 = b5 + b7;
    d3 = (a9 - c7) * t1;
    b7 = (c6 - b5) * t1;
    d4 = (c6 + b5) * t1;
    d5 = (c7 + a9) * t1;

    d6 = coefs[6] + coefs[22];
    d7 = coefs[6] - coefs[22];
    d2 = coefs[7] + coefs[23];
    d1 = coefs[7] - coefs[23];
    d0 = coefs[14] + coefs[30];
    a0 = coefs[14] - coefs[30];
    c6 = coefs[15] + coefs[31];
    c7 = coefs[15] - coefs[31];

    a9 = d0 + d6;
    b5 = a0 + d1;
    e0 = d7 - c7;
    c7 = c7 + d7;
    d1 = d1 - a0;
    a0 = c6 + d2;
    d2 = d2 - c6;
    d6 = d6 - d0;
    e1 = e0 * t2 - b5 * t3;
    e0 = e0 * t3 + b5 * t2;
    d7 = c7 * t3 - d1 * t2;
    t3 = d1 * t3 + c7 * t2;

    c7 = a3 - b7;
    b7 = b7 + a3;
    d0 = a6 - d4;
    d4 = d4 + a6;
    b5 = c3 - d7;
    c6 = c2 - t3;
    t3 = t3 + c2;
    d7 = d7 + c3;

    coefs[24] = b5 + d0;
    coefs[25] = c6 + c7;
    coefs[26] = d0 - b5;
    coefs[27] = c7 - c6;
    coefs[28] = d4 - t3;
    coefs[29] = d7 + b7;
    coefs[30] = t3 + d4;
    coefs[31] = b7 - d7;

    d0 = d3 + a7;
    c7 = d5 + b2;
    a7 = a7 - d3;
    b2 = b2 - d5;
    b5 = e1 + c0;
    c6 = e0 + c1;
    c1 = c1 - e0;
    c0 = c0 - e1;

    coefs[16] = b5 + d0;
    coefs[17] = c6 + c7;
    coefs[18] = d0 - b5;
    coefs[19] = c7 - c6;
    coefs[20] = a7 - c1;
    coefs[21] = c0 + b2;
    coefs[22] = c1 + a7;
    coefs[23] = b2 - c0;

    b7 = b3 - d2;
    d2 = d2 + b3;
    d0 = d6 + b6;
    b6 = b6 - d6;
    b5 = (d2 - b6) * t1;
    c7 = (d0 + b7) * t1;
    c6 = (b7 - d0) * t1;
    t1 = (b6 + d2) * t1;
    a7 = a1 - c5;
    c5 = c5 + a1;
    b7 = c4 + a2;
    a2 = a2 - c4;
    b2 = a4 + b1;

    coefs[8] = c6 + a7;
    coefs[9] = c7 + b7;
    coefs[10] = a7 - c6;
    coefs[11] = b7 - c7;
    coefs[12] = c5 - t1;
    coefs[13] = b5 + a2;
    coefs[14] = t1 + c5;
    coefs[15] = a2 - b5;

    b1 = b1 - a4;
    a4 = a8 + b0;
    b0 = b0 - a8;
    a7 = a0 + a5;
    a5 = a5 - a0;
    t1 = a9 + b4;
    b4 = b4 - a9;

    coefs[0] = t1 + b2;
    coefs[1] = a7 + a4;
    coefs[2] = b2 - t1;
    coefs[3] = a4 - a7;
    coefs[4] = b1 - a5;
    coefs[5] = b4 + b0;
    coefs[6] = a5 + b1;
    coefs[7] = b0 - b4;
}

static void rotation_32_b(float* coefs, const float* table) {
    float tc1 = table[1];
    float tc4 = table[4];
    float tc5 = table[5];
    float tc6 = table[6];
    float tc7 = table[7];
    float tc8 = table[8];
    float tc9 = table[9];

    float a0, a1, a2, a3, a4, a5, a6, a7;
    float b0, b1, b2, b3; //, b4, b5, b6, b7;
    float c0, c1, c2, c3, c4, c5, c6, c7;
    float d0, d1, d2, d3, d4, d5, d6, d7;
    float e0, e1, e2, e3, e4, e5, e6, e7, e8;

    a0 = coefs[0] - coefs[17];
    a1 = coefs[0] + coefs[17];
    a2 = coefs[1] + coefs[16];
    a3 = coefs[1] - coefs[16];
    a4 = coefs[8] + coefs[25];
    a5 = coefs[8] - coefs[25];
    a6 = coefs[9] + coefs[24];
    a7 = coefs[9] - coefs[24];

    b0 = (a5 - a6) * tc1;
    a6 = (a6 + a5) * tc1;
    a5 = b0 + a0;
    a0 = a0 - b0;
    b0 = a6 + a2;
    a2 = a2 - a6;
    a6 = (a4 - a7) * tc1;
    b1 = (a7 + a4) * tc1;
    b2 = a6 + a3;
    b3 = a1 - b1;
    a3 = a3 - a6;
    b1 = b1 + a1;

    a7 = coefs[2] - coefs[19];
    a6 = coefs[3] + coefs[18];
    c0 = coefs[3] - coefs[18];

    c1 = a7 * tc4 - a6 * tc5;
    a4 = a7 * tc5 + a6 * tc4;
    c2 = coefs[19] + coefs[2];
    a1 = coefs[11] + coefs[26];
    a6 = coefs[10] - coefs[27];
    c3 = coefs[27] + coefs[10];
    a7 = a6 * tc7 - a1 * tc6;
    a1 = a1 * tc7 + a6 * tc6;

    a6 = a7 + c1;
    c1 = c1 - a7;
    a7 = a1 + a4;
    a4 = a4 - a1;
    a1 = coefs[11] - coefs[26];

    c4 = c2 * tc7 + c0 * tc6;
    c5 = c2 * tc6 - c0 * tc7;
    c7 = a1 * tc5 + c3 * tc4;
    d0 = a1 * tc4 - c3 * tc5;
    c6 = c5 - c7;
    c7 = c7 + c5;
    c5 = c4 - d0;

    c3 = coefs[4] - coefs[21];
    d0 = d0 + c4;
    d1 = coefs[5] + coefs[20];
    a1 = c3 * tc9 + d1 * tc8;
    d2 = c3 * tc8 - d1 * tc9;
    d3 = coefs[5] - coefs[20];
    c0 = coefs[13] + coefs[28];
    d4 = coefs[13] - coefs[28];
    c3 = coefs[12] - coefs[29];
    c4 = coefs[21] + coefs[4];

    c2 = c0 * tc9 + c3 * tc8;
    c3 = c3 * tc9 - c0 * tc8;
    d5 = c2 + a1;
    a1 = a1 - c2;
    c0 = coefs[29] + coefs[12];
    d6 = c3 + d2;
    d2 = d2 - c3;
    c3 = c4 * tc9 - d3 * tc8;
    c2 = d3 * tc9 + c4 * tc8;
    c4 = c0 * tc8 - d4 * tc9;
    d3 = c0 * tc9 + d4 * tc8;
    d7 = c3 - c4;
    c4 = c4 + c3;
    c0 = coefs[7] + coefs[22];

    e0 = c2 - d3;
    d3 = d3 + c2;
    d4 = coefs[15] + coefs[30];
    c2 = coefs[6] - coefs[23];
    c3 = c2 * table[7] + c0 * tc6;
    e1 = c2 * tc6 - c0 * table[7];
    c2 = coefs[14] - coefs[31];
    e2 = coefs[15] - coefs[30];
    e3 = coefs[31] + coefs[14];
    e4 = coefs[23] + coefs[6];

    e5 = d5 + b0;
    c0 = c2 * table[5] - d4 * table[4];
    c2 = d4 * table[5] + c2 * table[4];
    e6 = c2 + c3;
    c3 = c3 - c2;
    c2 = coefs[7] - coefs[22];
    e7 = c0 + e1;
    e1 = e1 - c0;
    c0 = e4 * table[5] + c2 * table[4];
    d4 = c2 * table[5] - e4 * table[4];
    e4 = d6 + a5;
    c2 = e3 * table[7] - e2 * tc6;
    d1 = e2 * table[7] + e3 * tc6;

    e3 = e6 + a7;
    e2 = d1 + d4;
    d4 = d4 - d1;
    a7 = a7 - e6;
    d1 = e7 + a6;
    a6 = a6 - e7;
    e7 = c2 + c0;
    c0 = c0 - c2;
    a5 = a5 - d6;
    b0 = b0 - d5;

    coefs[0] = d1 + e4;
    coefs[1] = e3 + e5;
    coefs[2] = e4 - d1;
    coefs[3] = e5 - e3;
    coefs[4] = a5 - a7;
    coefs[5] = a6 + b0;
    coefs[6] = a7 + a5;
    coefs[7] = b0 - a6;

    a7 = d2 + a2;
    d1 = c1 - c3;
    b0 = e1 + a4;
    a4 = a4 - e1;
    a6 = a0 - a1;
    a5 = (d1 - b0) * tc1;
    b0 = (b0 + d1) * tc1;
    coefs[8] = a5 + a6;
    c3 = c3 + c1;
    a1 = a1 + a0;
    d1 = (c3 - a4) * tc1;
    a4 = (a4 + c3) * tc1;
    a2 = a2 - d2;
    coefs[9] = b0 + a7;
    coefs[12] = a1 - a4;
    coefs[10] = a6 - a5;
    a5 = c6 - e7;
    coefs[11] = a7 - b0;
    a6 = d7 + b3;
    coefs[15] = a2 - d1;
    b0 = c5 - e2;
    coefs[14] = a4 + a1;
    coefs[13] = d1 + a2;
    coefs[16] = a5 + a6;
    d1 = e0 + b2;
    b3 = b3 - d7;
    e2 = e2 + c5;
    coefs[19] = d1 - b0;
    coefs[17] = b0 + d1;
    coefs[18] = a6 - a5;
    b2 = b2 - e0;
    e7 = e7 + c6;
    d7 = d0 - c0;
    coefs[20] = b3 - e2;
    coefs[21] = e7 + b2;
    coefs[22] = e2 + b3;
    coefs[23] = b2 - e7;

    d1 = d4 + c7;
    c0 = c0 + d0;
    c7 = c7 - d4;
    c6 = (d1 - d7) * tc1;
    d1 = (d7 + d1) * tc1;
    d7 = b1 - d3;
    d3 = d3 + b1;
    c5 = c4 + a3;
    e0 = (c0 + c7) * tc1;
    e8 = (c7 - c0) * tc1;
    a3 = a3 - c4;

    coefs[24] = c6 + d7;
    coefs[25] = d1 + c5;
    coefs[26] = d7 - c6;
    coefs[27] = c5 - d1;
    coefs[28] = d3 - e0;
    coefs[29] = e8 + a3;
    coefs[30] = e0 + d3;
    coefs[31] = a3 - e8;
}

static void rotation_16_a(float* coefs, const float* table) {
    const float t1 = table[1];

    float a0, a1, a2, a3, a4, a5, a6, a7;
    float b0, b1, b2, b3, b4, b5, b6, b7;

    a0 = coefs[0] + coefs[8];
    b0 = coefs[0] - coefs[8];
    a1 = coefs[1] + coefs[9];
    b1 = coefs[1] - coefs[9];
    a2 = coefs[2] + coefs[10];
    b2 = coefs[2] - coefs[10];
    a3 = coefs[3] + coefs[11];
    b3 = coefs[3] - coefs[11];
    a4 = coefs[4] + coefs[12];
    b4 = coefs[4] - coefs[12];
    a5 = coefs[5] + coefs[13];
    b5 = coefs[5] - coefs[13];
    a6 = coefs[6] + coefs[14];
    b6 = coefs[6] - coefs[14];
    a7 = coefs[7] + coefs[15];
    b7 = coefs[7] - coefs[15];

    float c0, c1, c2, c3, d0, d1, e0;
    c0 = a0 + a4;
    d0 = a0 - a4;
    c1 = a1 + a5;
    d1 = a1 - a5;
    c2 = b1 + b4;
    b1 = b1 - b4;
    a4 = b2 - b7;
    b7 = b7 + b2;
    e0 = b0 - b5;
    b5 = b0 + b5;
    c3 = a3 + a7;
    b0 = b6 + b3;
    a3 = a3 - a7;
    b3 = b3 - b6;
    b2 = a6 + a2;
    a2 = a2 - a6;
    b6 = (b0 + a4) * t1;
    b4 = (b7 - b3) * t1;
    a4 = (a4 - b0) * t1;
    a5 = (b3 + b7) * t1;

    coefs[0] = b2 + c0;
    coefs[1] = c3 + c1;
    coefs[2] = c0 - b2;
    coefs[3] = c1 - c3;
    coefs[4] = d0 - a3;
    coefs[5] = a2 + d1;
    coefs[6] = a3 + d0;
    coefs[7] = d1 - a2;
    coefs[8] = a4 + e0;
    coefs[9] = b6 + c2;
    coefs[10] = e0 - a4;
    coefs[12] = b5 - a5;
    coefs[15] = b1 - b4;
    coefs[13] = b4 + b1;
    coefs[11] = c2 - b6;
    coefs[14] = a5 + b5;
}

static void rotation_16_b(float* coefs, const float* table) {
    float t1 = table[1];
    float t4 = table[4];
    float t5 = table[5];

    float a0, a1, a2, a3, a4, a5, a6, a7;
    float b0, b1, b2, b3, b4, b5, b6, b7;
    float c0, c1, c2;

    a0 = coefs[0] + coefs[9];
    a1 = coefs[0] - coefs[9];
    a2 = coefs[1] + coefs[8];
    a3 = coefs[1] - coefs[8];
    a4 = coefs[4] + coefs[13];
    a5 = coefs[4] - coefs[13];
    a6 = coefs[5] + coefs[12];
    a7 = coefs[5] - coefs[12];
    b0 = (a5 - a6) * t1;
    b1 = (a6 + a5) * t1;
    b2 = (a7 + a4) * t1;
    t1 = (a4 - a7) * t1;
    a5 = coefs[3] + coefs[10];
    a7 = coefs[3] - coefs[10];
    a6 = coefs[2] - coefs[11];
    a4 = coefs[11] + coefs[2];
    b3 = a6 * t4 - a5 * t5;
    b4 = a6 * t5 + a5 * t4;
    a5 = coefs[6] - coefs[15];
    b5 = a7 * t5 + a4 * t4;
    b6 = a4 * t5 - a7 * t4;
    a4 = coefs[7] + coefs[14];
    a7 = coefs[7] - coefs[14];
    b7 = coefs[15] + coefs[6];
    c0 = a5 * t5 - a4 * t4;
    c1 = a4 * t5 + a5 * t4;
    a5 = c0 + b3;
    c2 = b7 * t5 + a7 * t4;
    a4 = c1 + b4;
    a6 = b0 + a1;
    a7 = b7 * t4 - a7 * t5;
    t4 = b1 + a2;
    a1 = a1 - b0;
    coefs[0] = a5 + a6;
    b3 = b3 - c0;
    b4 = b4 - c1;
    coefs[1] = a4 + t4;
    coefs[4] = a1 - b4;
    coefs[2] = a6 - a5;
    t5 = b6 - a7;
    coefs[3] = t4 - a4;
    t4 = b5 - c2;
    a5 = t1 + a3;
    a2 = a2 - b1;
    coefs[6] = b4 + a1;
    coefs[5] = b3 + a2;
    c2 = c2 + b5;
    a1 = a0 - b2;
    b2 = b2 + a0;
    a3 = a3 - t1;
    a7 = a7 + b6;
    coefs[8] = t5 + a1;
    coefs[7] = a2 - b3;
    coefs[9] = t4 + a5;
    coefs[10] = a1 - t5;
    coefs[11] = a5 - t4;
    coefs[12] = b2 - c2;
    coefs[14] = c2 + b2;
    coefs[15] = a3 - a7;
    coefs[13] = a7 + a3;
}


static void swap_4_a(float* coefs_a, float* coefs_b) {
    float a0, a1, b0, b1;

    a0 = coefs_a[0];
    a1 = coefs_a[1];
    b0 = coefs_b[0];
    b1 = coefs_b[1];
    coefs_b[0] = a0;
    coefs_b[1] = a1;
    coefs_a[0] = b0;
    coefs_a[1] = b1;
}

static void transform_dct_post(int samples, float* coefs) {
    int indexes[64];

    int limit = 1;
    indexes[0] = 0;
    for (int k = 8; k < samples; k *= 2) {
        samples = samples >> 1;
        for (int i = 0; i < limit; i++) {
            indexes[limit + i] = samples + indexes[i];
        }
        limit *= 2;
    }

    int i0, i1;

    if (samples == limit * 8) {
        for (int i = 0, j = 0; i < limit; i++, j += 2) {
            for (int k = 0; k < i; k++) {
                i1 = indexes[i] + k * 2;
                i0 = indexes[k] + j;
                swap_4_a(coefs + i0, coefs + i1);

                i1 += limit * 2;
                i0 += limit * 4;
                swap_4_a(coefs + i0, coefs + i1);

                i1 += limit * 2;
                i0 += limit * -2;
                swap_4_a(coefs + i0, coefs + i1);

                i1 += limit * 2;
                i0 += limit * 4;
                swap_4_a(coefs + i0, coefs + i1);
            }

            i1 = indexes[i] + j + limit * 2;
            i0 = i1 + limit * 2;
            swap_4_a(coefs + i0, coefs + i1);
        }
    }
    else {
        for (int i = 1, j = 2; i < limit; i++, j += 2) {
            for (int k = 0; k < i; k++) {
                i0 = indexes[k] + j;
                i1 = indexes[i] + k * 2;
                swap_4_a(coefs + i0, coefs + i1);

                i0 += limit * 2;
                i1 += limit * 2;
                swap_4_a(coefs + i0, coefs + i1);
            }
        }
    }
}

static void swap_4_b(float* coefs_a, float* coefs_b) {
    float a0, b1, b0, a1;

    a0 = coefs_a[0];
    a1 = coefs_a[1];
    b0 = coefs_b[0];
    b1 = coefs_b[1];
    coefs_b[0] = a0;
    coefs_b[1] = -a1;
    coefs_a[0] = b0;
    coefs_a[1] = -b1;
}

static void transform_dft_post(int samples, float* coefs) {
    int indexes[64];

    int limit = 1;
    indexes[0] = 0;
    for (int k = 8; k < samples; k *= 2) {
        samples = samples >> 1;
        for (int i = 0; i < limit; i++) {
            indexes[limit + i] = samples + indexes[i];
        }
        limit *= 2;
    }

    int i0, i1;

    if (samples == limit * 8) {
        for (int i = 0, j = 0; i < limit; i++, j += 2) {
            for (int k = 0; k < i; k++) {
                i0 = indexes[k] + j;
                i1 = indexes[i] + k * 2; 
                swap_4_b(coefs + i0, coefs + i1);

                i0 += limit * 4;
                i1 += limit * 2;
                swap_4_b(coefs + i0, coefs + i1);

                i0 += limit * -2;
                i1 += limit * 2;
                swap_4_b(coefs + i0, coefs + i1);

                i0 += limit * 4;
                i1 += limit * 2;
                swap_4_b(coefs + i0, coefs + i1);
            }

            i0 = indexes[i] + j;
            coefs[i0 + 1] = -coefs[i0 + 1];

            i0 += limit * 2;
            i1 = i0 + limit * 2;
            swap_4_b(coefs + i0, coefs + i1);

            i1 += limit * 2;
            coefs[i1 + 1] = -coefs[i1 + 1];
        }
    }
    else {
        i0 = 0;
        i1 = i0 + limit * 2;
        coefs[i0 + 1] = -coefs[i0 + 1];
        coefs[i1 + 1] = -coefs[i1 + 1];

        for (int i = 1, j = 2; i < limit; i++, j += 2) {
            for (int k = 0; k < i; k++) {
                i0 = indexes[k] + j;
                i1 = indexes[i] + k * 2; 
                swap_4_b(coefs + i0, coefs + i1);

                i0 += limit * 2;
                i1 += limit * 2;
                swap_4_b(coefs + i0, coefs + i1);
            }

            i0 = indexes[i] + j;
            i1 = i0 + limit * 2;
            coefs[i0 + 1] = -coefs[i0 + 1];
            coefs[i1 + 1] = -coefs[i1 + 1];
        }
    }
}

static void transform_dct_pre(int samples, float* coefs, const float* table) {
    const int samples_oct = samples >> 3;

    float a0, a1, a2, a3, a4, a5, a6, a7;
    {
        int i2 = (samples_oct * 2);
        int i4 = (samples_oct * 4);
        int i6 = (samples_oct * 6);

        a0 = coefs[0] + coefs[i4 + 0];
        a1 = coefs[1] + coefs[i4 + 1];
        a2 = coefs[0] - coefs[i4 + 0];
        a3 = coefs[1] - coefs[i4 + 1];
        a4 = coefs[i2 + 0] + coefs[i6 + 0];
        a5 = coefs[i2 + 0] - coefs[i6 + 0];
        a6 = coefs[i2 + 1] + coefs[i6 + 1];
        a7 = coefs[i2 + 1] - coefs[i6 + 1];

        coefs[0] = a4 + a0;
        coefs[1] = a6 + a1;
        coefs[i2 + 1] = a1 - a6;
        coefs[i2 + 0] = a0 - a4;
        coefs[i4 + 0] = a2 - a7;
        coefs[i4 + 1] = a5 + a3;
        coefs[i6 + 0] = a7 + a2;
        coefs[i6 + 1] = a3 - a5;
    }

    a6 = table[1];
    a4 = table[2];
    a7 = table[3];

    float b0, b1, b2, b3;
    b0 = 0.0;
    b1 = 0.0;
    b2 = 1.0;
    b3 = 1.0;

    float c0, c1, c2, c3, c4, c5, c6, c7;
    float d0, d1, d2, d3, d4, d5, d6, d7, d8;

    {
        const float* table_tmp = table + 2;

        int i2 = (samples_oct * 2);
        float* coefs_0a = coefs + 4;
        float* coefs_1a = coefs + i2 + 4;
        float* coefs_1b = coefs + i2 - 4;
        float* coefs_2a = coefs + i2 * 2 + 4;
        float* coefs_2b = coefs + i2 * 2 - 4;
        float* coefs_3a = coefs + i2 * 3 + 4;
        float* coefs_3b = coefs + i2 * 3 - 4;
        float* coefs_4b = coefs + i2 * 4 - 4;

        a5 = b0;
        a1 = b2;
        a0 = b3;

        for (int i = ((samples_oct - 5) >> 2) + 1; i > 0; i--) {
            b2 = table_tmp[2];
            b0 = table_tmp[3];
            b3 = table_tmp[4];

            a3 = (b2 + a1) * a4;
            a2 = (b0 + a5) * a4;
            a0 = (b3 + a0) * a7;
            c1 = b1 - table_tmp[5];
            b1 = -table_tmp[5];
            c1 = c1 * a7;

            c0 = coefs_0a[-2] + coefs_2a[-2];
            c2 = coefs_0a[-2] - coefs_2a[-2];
            c4 = coefs_0a[-1] + coefs_2a[-1];
            a5 = coefs_0a[-1] - coefs_2a[-1];
            c3 = coefs_0a[0] + coefs_2a[0];
            c5 = coefs_0a[0] - coefs_2a[0];
            c6 = coefs_1a[-2] + coefs_3a[-2];
            c7 = coefs_1a[-2] - coefs_3a[-2];
            d1 = coefs_1a[-1] + coefs_3a[-1];
            d2 = coefs_1a[-1] - coefs_3a[-1];
            d0 = coefs_0a[1] + coefs_2a[1];
            a1 = coefs_0a[1] - coefs_2a[1];
            d3 = coefs_3a[1] + coefs_1a[1];
            d4 = coefs_1a[1] - coefs_3a[1];
            d5 = coefs_3a[0] + coefs_1a[0];
            d6 = coefs_1a[0] - coefs_3a[0];

            coefs_0a[-2] = c6 + c0;
            coefs_0a[-1] = d1 + c4;
            coefs_0a[0] = d5 + c3;
            coefs_0a[1] = d3 + d0;
            coefs_1a[-2] = c0 - c6;
            coefs_1a[1] = d0 - d3;
            coefs_1a[0] = c3 - d5;
            coefs_1a[-1] = c4 - d1;

            d3 = c7 + a5;
            d5 = c2 - d2;
            d2 = d2 + c2;
            a5 = a5 - c7;
            coefs_2a[-2] = a3 * d5 - a2 * d3;
            coefs_2a[-1] = a3 * d3 + a2 * d5;

            d3 = d6 + a1;
            d5 = c5 - d4;
            d4 = d4 + c5;
            a1 = a1 - d6;
            coefs_2a[0] = b2 * d5 - b0 * d3;
            coefs_2a[1] = b2 * d3 + b0 * d5;
            coefs_3a[-2] = c1 * a5 + a0 * d2;
            coefs_3a[-1] = a0 * a5 - c1 * d2;
            coefs_3a[0] = b1 * a1 + b3 * d4;
            coefs_3a[1] = b3 * a1 - b1 * d4;

            d3 = coefs_1b[3] - coefs_3b[3];
            d2 = coefs_1b[2] + coefs_3b[2];
            d6 = coefs_1b[3] + coefs_3b[3];
            d7 = coefs_1b[2] - coefs_3b[2];
            a5 = coefs_4b[0];
            d5 = coefs_1b[1] - coefs_3b[1];
            c4 = coefs_2b[2] + coefs_4b[2];
            c0 = coefs_1b[1] + coefs_3b[1];
            d4 = coefs_2b[2] - coefs_4b[2];
            a1 = coefs_2b[0];
            c7 = coefs_1b[0] + coefs_3b[0];
            c3 = coefs_2b[3] + coefs_4b[3];
            c2 = coefs_2b[3] - coefs_4b[3];
            d8 = coefs_1b[0] - coefs_3b[0];
            d1 = coefs_2b[1] + coefs_4b[1];
            c5 = coefs_2b[1] - coefs_4b[1];
            c6 = a5 + a1;
            a1 = a1 - a5;
            coefs_1b[2] = c4 + d2;
            coefs_1b[0] = c6 + c7;
            coefs_1b[3] = c3 + d6;
            d0 = d7 - c2;
            a5 = d4 + d3;

            coefs_2b[0] = c7 - c6;
            coefs_1b[1] = d1 + c0;
            coefs_2b[1] = c0 - d1;
            coefs_2b[2] = d2 - c4;
            coefs_3b[2] = a2 * d0 - a3 * a5;
            coefs_2b[3] = d6 - c3;
            coefs_3b[3] = a2 * a5 + a3 * d0;

            a3 = d8 - c5;
            a5 = a1 + d5;
            c2 = c2 + d7;
            d3 = d3 - d4;
            d5 = d5 - a1;
            c5 = c5 + d8;

            coefs_3b[0] = b0 * a3 - b2 * a5;
            coefs_3b[1] = b0 * a5 + b2 * a3;
            coefs_4b[0] = b3 * d5 + b1 * c5;
            coefs_4b[1] = b1 * d5 - b3 * c5;
            coefs_4b[2] = a0 * d3 + c1 * c2;
            coefs_4b[3] = c1 * d3 - a0 * c2;

            a5 = b0;
            a1 = b2;
            a0 = b3;

            coefs_0a += 4;
            coefs_1a += 4;
            coefs_1b -= 4;
            coefs_2a += 4;
            coefs_2b -= 4;
            coefs_3a += 4;
            coefs_3b -= 4;
            coefs_4b -= 4;
            table_tmp += 4;
        }
    }

    {
        int i3 = (samples_oct * 3);
        int i5 = (samples_oct * 5);
        int i7 = (samples_oct * 7);

        d1 = (b3 - a6) * a7;
        a7 = (b1 - a6) * a7;
        d5 = (b2 + a6) * a4;
        a5 = coefs[samples_oct - 1] + coefs[i5 - 1];
        a2 = coefs[samples_oct - 2] - coefs[i5 - 2];
        d3 = coefs[samples_oct - 1] - coefs[i5 - 1];
        a1 = coefs[samples_oct - 2] + coefs[i5 - 2];
        b2 = coefs[i3 - 1] + coefs[i7 - 1];
        b3 = coefs[i3 - 2] + coefs[i7 - 2];
        a0 = coefs[i3 - 2] - coefs[i7 - 2];
        a3 = coefs[i3 - 1] - coefs[i7 - 1];
        coefs[samples_oct - 2] = b3 + a1;
        b1 = a0 + d3;
        d3 = d3 - a0;
        a4 = (b0 + a6) * a4;
        coefs[samples_oct - 1] = b2 + a5;
        coefs[i3 - 1] = a5 - b2;
        coefs[i3 - 2] = a1 - b3;
        b0 = a2 - a3;
        a3 = a3 + a2;
        coefs[i5 - 2] = d5 * b0 - a4 * b1;
        coefs[i5 - 1] = d5 * b1 + a4 * b0;
        coefs[i7 - 2] = a7 * d3 + d1 * a3;
        coefs[i7 - 1] = d1 * d3 - a7 * a3;
        b1 = coefs[samples_oct + 0] + coefs[i5 + 0];
        b2 = coefs[samples_oct + 1] + coefs[i5 + 1];
        a0 = coefs[samples_oct + 0] - coefs[i5 + 0];
        a3 = coefs[samples_oct + 1] - coefs[i5 + 1];
        b3 = coefs[i3 + 0] + coefs[i7 + 0];
        a5 = coefs[i3 + 0] - coefs[i7 + 0];
        b0 = coefs[i3 + 1] + coefs[i7 + 1];
        a1 = coefs[i3 + 1] - coefs[i7 + 1];
        coefs[samples_oct + 0] = b3 + b1;
        coefs[samples_oct + 1] = b0 + b2;
        coefs[i3 + 1] = b2 - b0;
        b2 = a5 + a3;
        b0 = a0 - a1;
        a3 = a3 - a5;
        a1 = a1 + a0;
        coefs[i3 + 0] = b1 - b3;
        coefs[i5 + 0] = (b0 - b2) * a6;
        coefs[i5 + 1] = (b2 + b0) * a6;
        coefs[i7 + 0] = (a3 + a1) * -a6;
        coefs[i7 + 1] = (a3 - a1) * -a6;
        b2 = coefs[i3 + 2] + coefs[i7 + 2];
        a1 = coefs[i3 + 2] - coefs[i7 + 2];
        a5 = coefs[samples_oct + 2] + coefs[i5 + 2];
        a6 = coefs[i3 + 3] + coefs[i7 + 3];
        a0 = coefs[i3 + 3] - coefs[i7 + 3];
        b1 = coefs[samples_oct + 3] + coefs[i5 + 3];
        a3 = coefs[samples_oct + 2] - coefs[i5 + 2];
        a2 = coefs[samples_oct + 3] - coefs[i5 + 3];

        b3 = a3 - a0;
        b0 = a1 + a2;
        a2 = a2 - a1;
        a0 = a0 + a3;

        coefs[samples_oct + 3] = a6 + b1;
        coefs[samples_oct + 2] = b2 + a5;
        coefs[i3 + 2] = a5 - b2;
        coefs[i3 + 3] = b1 - a6;
        coefs[i5 + 2] = a4 * b3 - d5 * b0;
        coefs[i5 + 3] = a4 * b0 + d5 * b3;
        coefs[i7 + 3] = a7 * a2 - d1 * a0;
        coefs[i7 + 2] = d1 * a2 + a7 * a0;
    }
}

static void transform_dft_pre(int samples, float* coefs, const float* table) {
    const int samples_oct = samples >> 3;

    float a0, a1, a2, a3, a4, a5, a6, a7;
    {
        int i2 = (samples_oct * 2);
        int i4 = (samples_oct * 4);
        int i6 = (samples_oct * 6);

        a0 = coefs[0] + coefs[i4 + 0];
        a1 = coefs[0] - coefs[i4 + 0];
        a2 = -coefs[1] - coefs[i4 + 1];
        a3 = coefs[i4 + 1] - coefs[1];
        a4 = coefs[i2 + 0] + coefs[i6 + 0];
        a5 = coefs[i2 + 0] - coefs[i6 + 0];
        a6 = coefs[i2 + 1] + coefs[i6 + 1];
        a7 = coefs[i2 + 1] - coefs[i6 + 1];

        coefs[0] = a4 + a0;
        coefs[1] = a2 - a6;
        coefs[i2 + 0] = a0 - a4;
        coefs[i2 + 1] = a6 + a2;
        coefs[i4 + 0] = a7 + a1;
        coefs[i4 + 1] = a5 + a3;
        coefs[i6 + 0] = a1 - a7;
        coefs[i6 + 1] = a3 - a5;
    }

    a6 = table[1];
    a4 = table[2];
    a7 = table[3];

    float b0, b1, b2, b3;
    b0 = 0.0;
    b1 = 0.0;
    b2 = 1.0;
    b3 = 1.0;

    float c0, c1, c2, c3, c4, c5, c6, c7;
    float d0, d1, d2, d3, d4, d5, d6;

    {
        const float* table_tmp = table + 2;

        int i2 = (samples_oct * 2);
        float* coefs_0a = coefs + 4;
        float* coefs_1a = coefs + i2 + 4;
        float* coefs_1b = coefs + i2 - 4;
        float* coefs_2a = coefs + i2 * 2 + 4;
        float* coefs_2b = coefs + i2 * 2 - 4;
        float* coefs_3a = coefs + i2 * 3 + 4;
        float* coefs_3b = coefs + i2 * 3 - 4;
        float* coefs_4b = coefs + i2 * 4 - 4;

        a5 = b0;
        a2 = b2;
        a0 = b3;

        for (int i = ((samples_oct - 5) >> 2) + 1; i > 0; i--) {
            b2 = table_tmp[2];
            b0 = table_tmp[3];
            b3 = table_tmp[4];

            a2 = (b2 + a2) * a4;
            a3 = (b0 + a5) * a4;
            a0 = (b3 + a0) * a7;
            c1 = b1 - table_tmp[5];
            b1 = -table_tmp[5];
            c1 = c1 * a7;

            c0 = coefs_0a[-2] + coefs_2a[-2];
            a5     = coefs_0a[-2] - coefs_2a[-2];
            c2 = -coefs_0a[-1] - coefs_2a[-1];
            a1     = coefs_0a[0] - coefs_2a[0];
            c3 = -coefs_0a[1] - coefs_2a[1];
            c4 = coefs_1a[0] + coefs_3a[0];
            c5 = coefs_1a[0] - coefs_3a[0];
            c6 = coefs_1a[-1] - coefs_3a[-1];
            c7 = coefs_1a[1] + coefs_3a[1];
            d0 = coefs_1a[1] - coefs_3a[1];
            d1 = coefs_1a[-2] - coefs_3a[-2];
            d2 = coefs_2a[0] + coefs_0a[0];
            d3 = coefs_2a[-1] - coefs_0a[-1];
            d4 = coefs_2a[1] - coefs_0a[1];
            d5 = coefs_3a[-1] + coefs_1a[-1];
            d6 = coefs_3a[-2] + coefs_1a[-2];

            coefs_0a[-1] = c2 - d5;
            coefs_0a[-2] = d6 + c0;
            coefs_0a[1] = c3 - c7;
            coefs_0a[0] = c4 + d2;
            coefs_1a[1] = c7 + c3;
            coefs_1a[0] = d2 - c4;
            c7 = d1 + d3;
            coefs_1a[-1] = d5 + c2;
            coefs_1a[-2] = c0 - d6;
            c4 = c5 + d4;
            d5 = c6 + a5;
            d3 = d3 - d1;
            a5 = a5 - c6;
            coefs_2a[-2] = a2 * d5 - a3 * c7;
            coefs_2a[-1] = a2 * c7 + a3 * d5;
            c7 = d0 + a1;
            a1 = a1 - d0;
            d4 = d4 - c5;
            coefs_2a[0] = b2 * c7 - b0 * c4;
            coefs_2a[1] = b2 * c4 + b0 * c7;
            coefs_3a[-2] = c1 * d3 + a0 * a5;
            coefs_3a[-1] = a0 * d3 - c1 * a5;
            coefs_3a[0] = b1 * d4 + b3 * a1;
            coefs_3a[1] = b3 * d4 - b1 * a1;
            c6 = coefs_1b[2] + coefs_3b[2];
            a1 = coefs_1b[2] - coefs_3b[2];
            c3 = coefs_2b[2] + coefs_4b[2];
            c5 = coefs_2b[2] - coefs_4b[2];
            d6 = coefs_2b[3] + coefs_4b[3];
            d0 = coefs_2b[3] - coefs_4b[3];
            a5 = coefs_3b[1];
            c0 = -coefs_1b[3] - coefs_3b[3];
            d4 = coefs_3b[3] - coefs_1b[3];
            c7 = coefs_1b[0] - coefs_3b[0];
            c2 = coefs_1b[0] + coefs_3b[0];
            d5 = coefs_4b[0] + coefs_2b[0];
            d3 = coefs_2b[0] - coefs_4b[0];
            c4 = coefs_2b[1] + coefs_4b[1];
            d1 = coefs_2b[1] - coefs_4b[1];
            coefs_1b[3] = c0 - d6;
            d2 = -coefs_1b[1] - a5;
            a5 = a5 - coefs_1b[1];
            coefs_1b[1] = d2 - c4;
            coefs_1b[2] = c3 + c6;
            coefs_1b[0] = d5 + c2;
            coefs_2b[3] = d6 + c0;
            coefs_2b[1] = c4 + d2;
            d6 = d0 + a1;
            coefs_2b[0] = c2 - d5;
            c4 = c5 + d4;
            coefs_2b[2] = c6 - c3;
            coefs_3b[3] = a3 * c4 + a2 * d6;
            coefs_3b[2] = a3 * d6 - a2 * c4;
            a3 = d1 + c7;
            a2 = d3 + a5;
            coefs_3b[0] = b0 * a3 - b2 * a2;
            a5 = a5 - d3;
            a1 = a1 - d0;
            coefs_3b[1] = b0 * a2 + b2 * a3;
            d4 = d4 - c5;
            c7 = c7 - d1;
            coefs_4b[2] = a0 * d4 + c1 * a1;
            coefs_4b[3] = c1 * d4 - a0 * a1;
            coefs_4b[0] = b3 * a5 + b1 * c7;
            coefs_4b[1] = b1 * a5 - b3 * c7;

            a5 = b0;
            a2 = b2;
            a0 = b3;

            coefs_0a += 4;
            coefs_1a += 4;
            coefs_2a += 4;
            coefs_3a += 4;
            coefs_1b -= 4;
            coefs_2b -= 4;
            coefs_3b -= 4;
            coefs_4b -= 4;
            table_tmp += 4;
        }
    }

    {
        int i3 = (samples_oct * 3);
        int i5 = (samples_oct * 5);
        int i7 = (samples_oct * 7);

        c4 = (b3 - a6) * a7;
        a7 = (b1 - a6) * a7;
        c7 = (b2 + a6) * a4;
        a2 = coefs[samples_oct + -2] + coefs[i5 + -2];
        a1 = coefs[samples_oct + -2] - coefs[i5 + -2];
        b2 = coefs[i3 + -1] + coefs[i7 + -1];
        a5 = coefs[i3 + -1] - coefs[i7 + -1];
        b3 = coefs[i3 + -2] + coefs[i7 + -2];
        a0 = coefs[i3 + -2] - coefs[i7 + -2];
        b1 = -coefs[samples_oct + -1] - coefs[i5 + -1];
        a3 = coefs[i5 + -1] - coefs[samples_oct + -1];
        a4 = (b0 + a6) * a4;
        coefs[samples_oct + -2] = b3 + a2;
        coefs[samples_oct + -1] = b1 - b2;
        coefs[i3 + -1] = b2 + b1;
        b2 = a5 + a1;
        a1 = a1 - a5;
        coefs[i3 + -2] = a2 - b3;
        b0 = a0 + a3;
        a3 = a3 - a0;
        coefs[i5 + -2] = c7 * b2 - a4 * b0;
        coefs[i5 + -1] = c7 * b0 + a4 * b2;
        coefs[i7 + -2] = a7 * a3 + c4 * a1;
        coefs[i7 + -1] = c4 * a3 - a7 * a1;
        a5 = coefs[samples_oct] + coefs[i5];
        b2 = -coefs[samples_oct + 1] - coefs[i5 + 1];
        a3 = coefs[samples_oct] - coefs[i5];
        b0 = coefs[i3 + 1] + coefs[i7 + 1];
        b3 = coefs[i3] + coefs[i7];
        a2 = coefs[i3] - coefs[i7];
        b1 = coefs[i3 + 1] - coefs[i7 + 1];
        a0 = coefs[i5 + 1] - coefs[samples_oct + 1];
        coefs[samples_oct + 1] = b2 - b0;
        coefs[samples_oct] = b3 + a5;
        coefs[i3 + 1] = b0 + b2;
        b0 = b1 + a3;
        b2 = a2 + a0;
        a3 = a3 - b1;
        coefs[i3] = a5 - b3;
        coefs[i5] = (b0 - b2) * a6;
        coefs[i5 + 1] = (b2 + b0) * a6;
        a0 = a0 - a2;
        coefs[i7] = (a0 + a3) * -a6;
        coefs[i7 + 1] = (a0 - a3) * -a6;
        b0 = coefs[i3 + 2] + coefs[i7 + 2];
        a6 = coefs[i3 + 3] + coefs[i7 + 3];
        b1 = coefs[samples_oct + 2] + coefs[i5 + 2];
        a5 = coefs[i3 + 2] - coefs[i7 + 2];
        b3 = coefs[i3 + 3] - coefs[i7 + 3];
        a0 = coefs[samples_oct + 2] - coefs[i5 + 2];
        b2 = -coefs[samples_oct + 3] - coefs[i5 + 3];
        a2 = coefs[i5 + 3] - coefs[samples_oct + 3];

        coefs[samples_oct + 3] = b2 - a6;
        coefs[samples_oct + 2] = b0 + b1;
        coefs[i3 + 3] = a6 + b2;

        b2 = b3 + a0;
        a6 = a5 + a2;
        a2 = a2 - a5;
        a0 = a0 - b3;

        coefs[i3 + 2] = b1 - b0;
        coefs[i5 + 2] = a4 * b2 - c7 * a6;
        coefs[i5 + 3] = a4 * a6 + c7 * b2;
        coefs[i7 + 3] = a7 * a2 - c4 * a0;
        coefs[i7 + 2] = c4 * a2 + a7 * a0;
    }
}

//----------------------

static void rotation_main_a(int samples, float* coefs, const float* table) {
    const int samples_oct = samples >> 3;

    {
        int i0 = (samples_oct * 0);
        int i2 = (samples_oct * 2);
        int i4 = (samples_oct * 4);
        int i6 = (samples_oct * 6);

        float a0, a1, a2, a3, a4, a5, a6, a7;
        a0 = coefs[i0 + 0] + coefs[i4 + 0];
        a1 = coefs[i0 + 0] - coefs[i4 + 0];
        a2 = coefs[i0 + 1] + coefs[i4 + 1];
        a3 = coefs[i0 + 1] - coefs[i4 + 1];
        a4 = coefs[i2 + 0] + coefs[i6 + 0];
        a5 = coefs[i2 + 0] - coefs[i6 + 0];
        a6 = coefs[i2 + 1] + coefs[i6 + 1];
        a7 = coefs[i2 + 1] - coefs[i6 + 1];

        coefs[i0 + 0] = a4 + a0;
        coefs[i0 + 1] = a6 + a2;
        coefs[i2 + 0] = a0 - a4;
        coefs[i2 + 1] = a2 - a6;
        coefs[i4 + 0] = a1 - a7;
        coefs[i4 + 1] = a5 + a3;
        coefs[i6 + 0] = a7 + a1;
        coefs[i6 + 1] = a3 - a5;
    }

    {
        float* coefs_2 = coefs + (samples_oct * 2);
        float* coefs_4 = coefs + (samples_oct * 4);
        float* coefs_6 = coefs + (samples_oct * 6);
        float* coefs_8 = coefs + (samples_oct * 8);

        for (int i = 2, j = 4; i < samples_oct; i += 2, j += 4) {
            const float t0 = table[j + 0];
            const float t1 = table[j + 1];
            const float t2 = table[j + 2];
            const float t3 = -table[j + 3];

            float a0, a1, a2, a3, a4, a5, a6, a7;
            float b0, b1, b2, b3;

            a0 = coefs[i + 0] + coefs_4[i + 0];
            a1 = coefs[i + 0] - coefs_4[i + 0];
            a2 = coefs[i + 1] + coefs_4[i + 1];
            a3 = coefs[i + 1] - coefs_4[i + 1];
            a4 = coefs_2[i + 0] - coefs_6[i + 0];
            a5 = coefs_2[i + 0] + coefs_6[i + 0];
            a6 = coefs_2[i + 1] + coefs_6[i + 1];
            a7 = coefs_2[i + 1] - coefs_6[i + 1];

            coefs[i + 0] = a5 + a0;
            coefs[i + 1] = a6 + a2;
            coefs_2[i + 0] = a0 - a5;
            coefs_2[i + 1] = a2 - a6;

            b0 = a1 + a7;
            b1 = a1 - a7;
            b2 = a3 + a4;
            b3 = a3 - a4;

            coefs_4[i + 0] = b1 * t0 - b2 * t1;
            coefs_4[i + 1] = b2 * t0 + b1 * t1;
            coefs_6[i + 0] = b3 * t3 + b0 * t2;
            coefs_6[i + 1] = b3 * t2 - b0 * t3;

            a0 = coefs_6[0 - i] + coefs_2[0 - i];
            a1 = coefs_2[0 - i] - coefs_6[0 - i];
            a2 = coefs_6[1 - i] + coefs_2[1 - i];
            a3 = coefs_2[1 - i] - coefs_6[1 - i];
            a5 = coefs_8[0 - i] + coefs_4[0 - i];
            a6 = coefs_4[1 - i] + coefs_8[1 - i];
            a7 = coefs_4[1 - i] - coefs_8[1 - i];
            a4 = coefs_4[0 - i] - coefs_8[0 - i];

            coefs_2[0 - i] = a5 + a0;
            coefs_2[1 - i] = a6 + a2;
            coefs_4[0 - i] = a0 - a5;
            coefs_4[1 - i] = a2 - a6;

            b0 = a1 + a7;
            b1 = a1 - a7;
            b2 = a3 + a4;
            b3 = a3 - a4;

            coefs_6[0 - i] = b1 * t1 - b2 * t0;
            coefs_6[1 - i] = b2 * t1 + b1 * t0;
            coefs_8[0 - i] = b3 * t2 + b0 * t3;
            coefs_8[1 - i] = b3 * t3 - b0 * t2;
        }
    }

    {
        int i1 = (samples_oct * 1);
        int i5 = (samples_oct * 5);
        int i3 = (samples_oct * 3);
        int i7 = (samples_oct * 7);

        const float t1 = table[1];

        float a0, a1, a2, a3, a4, a5, a6, a7;
        a0 = coefs[i1 + 0] + coefs[i5 + 0];
        a1 = coefs[i1 + 0] - coefs[i5 + 0];
        a2 = coefs[i1 + 1] + coefs[i5 + 1];
        a3 = coefs[i1 + 1] - coefs[i5 + 1];
        a4 = coefs[i3 + 0] + coefs[i7 + 0];
        a5 = coefs[i3 + 0] - coefs[i7 + 0];
        a6 = coefs[i3 + 1] + coefs[i7 + 1];
        a7 = coefs[i3 + 1] - coefs[i7 + 1];

        float b0, b1, b2, b3;        
        b0 = a5 + a3;
        b1 = a3 - a5;
        b2 = a1 - a7;
        b3 = a7 + a1;

        coefs[i1 + 0] = a4 + a0;
        coefs[i1 + 1] = a6 + a2;
        coefs[i3 + 0] = a0 - a4;
        coefs[i3 + 1] = a2 - a6;
        coefs[i5 + 0] = (b2 - b0) * t1;
        coefs[i5 + 1] = (b0 + b2) * t1;
        coefs[i7 + 1] = (b1 - b3) * -t1;
        coefs[i7 + 0] = (b1 + b3) * -t1;
    }
}

static void rotation_main_b(int samples, float* coefs, const float* table) {
    const int samples_oct = samples >> 3;

    {
        float t1 = table[1];

        int i0 = (samples_oct * 0);
        int i2 = (samples_oct * 2);
        int i4 = (samples_oct * 4);
        int i6 = (samples_oct * 6);

        float a0, a1, a2, a3, a4, a5, a6, a7;
        a0 = coefs[i0 + 0] + coefs[i4 + 1];
        a1 = coefs[i0 + 0] - coefs[i4 + 1];
        a2 = coefs[i0 + 1] + coefs[i4 + 0];
        a3 = coefs[i0 + 1] - coefs[i4 + 0];
        a4 = coefs[i2 + 0] + coefs[i6 + 1];
        a5 = coefs[i2 + 0] - coefs[i6 + 1];
        a6 = coefs[i2 + 1] + coefs[i6 + 0];
        a7 = coefs[i2 + 1] - coefs[i6 + 0];

        float b0, b1, b2, b3;        
        b0 = (a7 + a4) * t1;
        b1 = (a4 - a7) * t1;
        b2 = (a6 + a5) * t1;
        b3 = (a5 - a6) * t1;

        coefs[i0 + 0] = b3 + a1;
        coefs[i0 + 1] = b2 + a2;
        coefs[i2 + 0] = a1 - b3;
        coefs[i2 + 1] = a2 - b2;
        coefs[i4 + 0] = a0 - b0;
        coefs[i4 + 1] = b1 + a3;
        coefs[i6 + 0] = b0 + a0;
        coefs[i6 + 1] = a3 - b1;
    }

    {
        int i4 = samples_oct * 4;

        float* coefs_0 = coefs + (samples_oct * 0);
        float* coefs_2 = coefs + (samples_oct * 2);
        float* coefs_4 = coefs + (samples_oct * 4);
        float* coefs_6 = coefs + (samples_oct * 6);
        float* coefs_8 = coefs + (samples_oct * 8);

        for (int i = 2, j = 4; i < samples_oct; i += 2, j += 4) {
            const float t0 = table[j + 0];
            const float t1 = table[j + 1];
            const float t2 = table[j + 2];
            const float t3 = -table[j + 3];
            const float t4 = table[i4 - j + 0];
            const float t5 = table[i4 - j + 1];
            const float t6 = table[i4 - j + 2];
            const float t7 = -table[i4 - j + 3];

            float a0, a1, a2, a3, a4, a5, a6, a7;
            float b1, b0, b3, b2;
            float c0, c1, c2, c3, c4, c5, c6, c7;

            a0 = coefs_0[i + 0] - coefs_4[i + 1];
            a1 = coefs_0[i + 1] + coefs_4[i + 0];
            a2 = coefs_0[i + 1] - coefs_4[i + 0];
            a3 = coefs_2[i + 0] + coefs_6[i + 1];
            a4 = coefs_2[i + 0] - coefs_6[i + 1];
            a5 = coefs_2[i + 1] + coefs_6[i + 0];
            a6 = coefs_2[i + 1] - coefs_6[i + 0];
            a7 = coefs_4[i + 1] + coefs_0[i + 0];

            b0 = a0 * t1 + a1 * t0;
            b1 = a0 * t0 - a1 * t1;
            b2 = a4 * t4 + a5 * t5;
            b3 = a4 * t5 - a5 * t4;

            coefs_0[i + 0] = b3 + b1;
            coefs_0[i + 1] = b2 + b0;
            coefs_2[i + 0] = b1 - b3;
            coefs_2[i + 1] = b0 - b2;

            b0 = a2 * t3 + a7 * t2;
            a5 = a2 * t2 - a7 * t3;
            a0 = a6 * t6 + a3 * t7;
            a1 = a6 * t7 - a3 * t6;

            coefs_4[i + 0] = a0 + b0;
            coefs_4[i + 1] = a1 + a5;
            coefs_6[i + 1] = a5 - a1;
            coefs_6[i + 0] = b0 - a0;

            b0 = coefs_4[0 - i] - coefs_8[1 - i];
            a2 = coefs_8[1 - i] + coefs_4[0 - i];
            a1 = coefs_2[1 - i] + coefs_6[0 - i];
            a0 = coefs_4[1 - i] + coefs_8[0 - i];
            a6 = coefs_2[1 - i] - coefs_6[0 - i];
            a4 = coefs_4[1 - i] - coefs_8[0 - i];
            b1 = coefs_2[0 - i] - coefs_6[1 - i];
            a3 = coefs_6[1 - i] + coefs_2[0 - i];

            c0 = t4 * b1 - t5 * a1;
            c1 = t4 * a1 + t5 * b1;
            c2 = t1 * b0 - t0 * a0;
            c3 = t1 * a0 + t0 * b0;
            c4 = a6 * t6 - a3 * t7;
            c5 = a6 * t7 + a3 * t6;
            c6 = a4 * t3 - a2 * t2;
            c7 = a4 * t2 + a2 * t3;

            coefs_2[0 - i] = c2 + c0;
            coefs_2[1 - i] = c3 + c1;
            coefs_4[0 - i] = c0 - c2;
            coefs_4[1 - i] = c1 - c3;
            coefs_6[0 - i] = c7 + c5;
            coefs_6[1 - i] = c6 + c4;
            coefs_8[0 - i] = c5 - c7;
            coefs_8[1 - i] = c4 - c6;
        }
    }

    {
        int i1 = (samples_oct * 1);
        int i3 = (samples_oct * 3);
        int i5 = (samples_oct * 5);
        int i7 = (samples_oct * 7);

        int i2 = (samples_oct * 2);
        const float t0 = table[i2 + 0];
        const float t1 = table[i2 + 1];

        float a0 = coefs[i1 + 0] + coefs[i5 + 1];
        float a1 = coefs[i1 + 0] - coefs[i5 + 1];
        float a2 = coefs[i1 + 1] + coefs[i5 + 0];
        float a3 = coefs[i1 + 1] - coefs[i5 + 0];
        float a4 = coefs[i3 + 0] + coefs[i7 + 1];
        float a5 = coefs[i3 + 0] - coefs[i7 + 1];
        float a6 = coefs[i3 + 1] + coefs[i7 + 0];
        float a7 = coefs[i3 + 1] - coefs[i7 + 0];

        float b0 = a1 * t0 - a2 * t1;
        float b1 = a2 * t0 + a1 * t1;
        float b2 = a5 * t1 - t0 * a6;
        float b3 = a6 * t1 + t0 * a5;
        float c0 = a3 * t1 + a0 * t0;
        float c1 = a0 * t1 - a3 * t0;
        float c2 = a7 * t0 + a4 * t1;
        float c3 = a4 * t0 - a7 * t1;

        coefs[i1 + 0] = b2 + b0;
        coefs[i1 + 1] = b3 + b1;
        coefs[i3 + 0] = b0 - b2;
        coefs[i3 + 1] = b1 - b3;
        coefs[i5 + 0] = c1 - c3;
        coefs[i5 + 1] = c0 - c2;
        coefs[i7 + 0] = c3 + c1;
        coefs[i7 + 1] = c2 + c0;
    }
}

static void transform_128_a(int samples, float* coefs, int points, const float* table) {
    if (samples == 128) {
        rotation_32_a(coefs     , table + (points + -8));
        rotation_32_b(coefs + 32, table + (points + -32));
        rotation_32_a(coefs + 64, table + (points + -8));
        rotation_32_a(coefs + 96, table + (points + -8));
    }
    else { // 64
        rotation_16_a(coefs     , table + (points + -16));
        rotation_16_b(coefs + 16, table + (points + -16));
        rotation_16_a(coefs + 32, table + (points + -16));
        rotation_16_a(coefs + 48, table + (points + -16));
    }
}

static void transform_128_b(int samples, float* coefs, int points, const float* table) {
    if (samples == 128) {
        rotation_32_a(coefs     , table + (points + -8));
        rotation_32_b(coefs + 32, table + (points + -32));
        rotation_32_a(coefs + 64, table + (points + -8));
        rotation_32_b(coefs + 96, table + (points + -32));
    }
    else {
        rotation_16_a(coefs     , table + (points + -16));
        rotation_16_b(coefs + 16, table + (points + -16));
        rotation_16_a(coefs + 32, table + (points + -16));
        rotation_16_b(coefs + 48, table + (points + -16));
    }
}


static void transform_256_a(int samples, float* coefs, int points, const float* table) {
    int step;
    int substep;
    int samples_tmp;

    for (samples_tmp = samples >> 2; step = samples_tmp, substep = samples_tmp >> 1, 128 < samples_tmp; samples_tmp = samples_tmp >> 2) {
        for (; step < samples; step *= 4) {
            for (int i = step - samples_tmp; i < samples; i += step * 4) {
                rotation_main_a(samples_tmp, coefs + i, table + (points - substep));
                rotation_main_b(samples_tmp, coefs + i + step, table + (points - samples_tmp));
                rotation_main_a(samples_tmp, coefs + i + step * 2, table + (points - substep));
            }
        }
        rotation_main_a(samples_tmp, coefs + (samples - samples_tmp), table + (points - substep));
    }

    for (; step < samples; step *= 4) {
        for (int i = step - samples_tmp; i < samples; i += step * 4) {
            rotation_main_a(samples_tmp, coefs + i, table + (points - substep));
            transform_128_a(samples_tmp, coefs + i, points, table);

            rotation_main_b(samples_tmp, coefs + i + step, table + (points - samples_tmp));
            transform_128_b(samples_tmp, coefs + i + step, points, table);

            rotation_main_a(samples_tmp, coefs + i + step * 2, table + (points - substep));
            transform_128_a(samples_tmp, coefs + i + step * 2, points, table);
        }
    }

    rotation_main_a(samples_tmp, coefs + (samples - samples_tmp), table + (points - substep));
    transform_128_a(samples_tmp, coefs + (samples - samples_tmp), points, table);
}

static void transform_256_b(int samples, float* coefs, int points, const float* table) {
    const int samples_half = samples >> 1;

    int step;
    int samples_tmp;
    for (samples_tmp = samples >> 2; step = samples_tmp, 128 < samples_tmp; samples_tmp = samples_tmp >> 2) {
        for (; step < samples_half; step = step * 4) {
            for (int i = step - samples_tmp; i < samples_half; i += step * 2) {
                rotation_main_a(samples_tmp, coefs + i, table + (points - (samples_tmp >> 1)));
                rotation_main_a(samples_tmp, coefs + i + samples_half, table + (points - (samples_tmp >> 1)));
            }
            for (int i = step * 2 - samples_tmp; i < samples_half; i += step * 4) {
                rotation_main_b(samples_tmp, coefs + i, table + (points - samples_tmp));
                rotation_main_b(samples_tmp, coefs + i + samples_half, table + (points - samples_tmp));
            }
        }
    }

    for (; step < samples_half; step = step * 4) {
        for (int i = step - samples_tmp; i < samples_half; i += step * 2) {
            rotation_main_a(samples_tmp, coefs + i, table + (points - (samples_tmp >> 1)));
            transform_128_a(samples_tmp, coefs + i, points, table);
            rotation_main_a(samples_tmp, coefs + i + samples_half, table + (points - (samples_tmp >> 1)));
            transform_128_a(samples_tmp, coefs + i + samples_half, points, table);
        }
        for (int i = step * 2 - samples_tmp; i < samples_half; i += step * 4) {
            rotation_main_b(samples_tmp, coefs + i, table + (points - samples_tmp));
            transform_128_b(samples_tmp, coefs + i, points, table);
            rotation_main_b(samples_tmp, coefs + i + samples_half, table + (points - samples_tmp));
            transform_128_b(samples_tmp, coefs + i + samples_half, points, table);
        }
    }
}


static void transform_512_a(int samples, float* coefs, int tx_points, const float* table);
static void transform_512_b(int samples, float* coefs, int tx_points, const float* table);

static void transform_512_a(int samples, float* coefs, int points, const float* table) {
    rotation_main_a(samples, coefs, table + (points + (samples >> 2) * -2));
    while (samples > 512) {
        samples >>= 2; 

        transform_512_a(samples, coefs + samples * 0, points, table);
        transform_512_b(samples, coefs + samples * 1, points, table);
        transform_512_a(samples, coefs + samples * 2, points, table);

        coefs += samples * 3;
        rotation_main_a(samples, coefs, table + (points + (samples >> 2) * -2));
    }

    transform_256_a(samples, coefs, points, table);
}

static void transform_512_b(int samples, float* coefs, int points, const float* table) {
    rotation_main_b(samples, coefs, table + (points - samples));
    while (samples > 512) {
        samples >>= 2; 

        transform_512_a(samples, coefs + samples * 0, points, table);
        transform_512_b(samples, coefs + samples * 1, points, table);
        transform_512_a(samples, coefs + samples * 2, points, table);

        coefs += samples * 3;
        rotation_main_b(samples, coefs, table + (points - samples));
    }

    transform_256_b(samples, coefs, points, table);
}

static void transform_dct_main(int samples, float* coefs, int points, const float* table) {
    const int samples_qrt = samples >> 2;

    if (samples > 32) {
        transform_dct_pre(samples, coefs, table + (points - samples_qrt));
        if (samples > 512) {
            transform_512_a(samples_qrt, coefs + samples_qrt * 0, points, table);
            transform_512_b(samples_qrt, coefs + samples_qrt * 1, points, table);
            transform_512_a(samples_qrt, coefs + samples_qrt * 2, points, table);
            transform_512_a(samples_qrt, coefs + samples_qrt * 3, points, table);
        }
        else if (samples > 128) {
            transform_256_a(samples, coefs, points, table);
        }
        else {
            transform_128_a(samples, coefs, points, table);
        }
        transform_dct_post(samples, coefs);
    }
    else {
        if (samples <= 8) {
            if (samples == 8) {
                transform_8_dct(coefs);
            }
            else if (samples == 4) {
                transform_4(coefs);
            }
        }
        else if (samples == 32) {
            rotation_32_a(coefs, table + (points + -8));
            transform_32_swap(coefs);
        }
        else { //16
            rotation_16_a(coefs, table);
            transform_4_dct(coefs);
        }
    }
}

static void transform_dft_main(int samples, float* coefs, int points, const float* table) {
    const int samples_qrt = samples >> 2;

    if (samples > 32) {
        transform_dft_pre(samples, coefs, table + (points - samples_qrt));
        if (samples > 512) {
            transform_512_a(samples_qrt, coefs + samples_qrt * 0, points, table);
            transform_512_b(samples_qrt, coefs + samples_qrt * 1, points, table);
            transform_512_a(samples_qrt, coefs + samples_qrt * 2, points, table);
            transform_512_a(samples_qrt, coefs + samples_qrt * 3, points, table);
        }
        else if (samples > 128) {
            transform_256_a(samples, coefs, points, table);
        }
        else {
            transform_128_a(samples, coefs, points, table);
        }
        transform_dft_post(samples, coefs);
    }
    else {
        if (samples <= 8) {
            if (samples == 8) {
                transform_8_dft(coefs);
            }
            else if (samples == 4) {
                transform_4(coefs);
            }
        }
        else if (samples == 32) {
            rotation_32_a(coefs, table + (points + -8));
            transform_32_dft(coefs);
        }
        else { //16
            rotation_16_a(coefs, table);
            transform_16_dft(coefs);
        }
    }
}


static void pre_dct(int samples, float* coefs, int points, const float* table) {
    const int samples_half = samples >> 1;
    const int step = (points / samples);

    const float* table_lo = table;
    const float* table_hi = table + points;

    for (int i = 1, j = samples - 1; i < samples_half; i++, j--) {
        table_lo += step;
        table_hi -= step;

        float v0 = coefs[i + 0];
        float v1 = coefs[j + 0];
        float tmp0 = table_lo[0] - table_hi[0];
        float tmp1 = table_hi[0] + table_lo[0];
        coefs[i + 0] = v1 * tmp1 + v0 * tmp0;
        coefs[j + 0] = v0 * tmp1 - v1 * tmp0;
    }

    coefs[samples_half] = coefs[samples_half] * table[0];
}

static void post_dct(int samples, float* coefs) {
    float v0 = coefs[0];
    float v1 = coefs[1];
    coefs[0] = v0 + v1;

    for (int i = 2; i < samples; i+= 2) {
        float vi = coefs[i];
        coefs[i] = coefs[i + 1] + vi;
        coefs[i - 1] = vi - coefs[i + 1];
    }

    coefs[samples - 1] = v0 - v1;
}

static void scramble_dct(int samples, float* coefs, int points, const float* table) {
    const int samples_half = samples >> 1;
    const int step = (points * 2) / samples_half;

    const float* table_lo = table;
    const float* table_hi = table + points;

    for (int i = 2, j = samples - 2; i < samples_half; i += 2, j -= 2) {
        table_lo += step;
        table_hi -= step;

        float v0 = coefs[i + 0] - coefs[j + 0];
        float v1 = coefs[i + 1] + coefs[j + 1];
        float tmp0 = v0 * (0.5 - table_hi[0]) - v1 * table_lo[0];
        float tmp1 = v0 * table_lo[0] + v1 * (0.5 - table_hi[0]);
        coefs[i + 0] = coefs[i + 0] - tmp0;
        coefs[i + 1] = coefs[i + 1] - tmp1;
        coefs[j + 0] = tmp0 + coefs[j + 0];
        coefs[j + 1] = coefs[j + 1] - tmp1;
    }
}

static void scramble_dft(int samples, float* coefs, int points, const float* table) {
    const int samples_half = samples >> 1;
    const int step = (points * 2) / samples_half;

    const float* table_lo = table;
    const float* table_hi = table + points;

    for (int i = 2, j = samples - 2; i < samples_half; i += 2, j -= 2) {
        table_lo += step;
        table_hi -= step;

        float v0 = coefs[i + 0] - coefs[j + 0];
        float v1 = coefs[i + 1] + coefs[j + 1];
        float tmp0 = v1 * table_lo[0] + v0 * (0.5 - table_hi[0]);
        float tmp1 = v1 * (0.5 - table_hi[0]) - v0 * table_lo[0];
        coefs[i + 0] = coefs[i + 0] - tmp0;
        coefs[i + 1] = coefs[i + 1] - tmp1;
        coefs[j + 0] = tmp0 + coefs[j + 0];
        coefs[j + 1] = coefs[j + 1] - tmp1;
    }
}


static void rad_ddctf(int samples, float* coefs, int points, const float* table) {
    const int points_qrt = points >> 2;

    pre_dct(samples, coefs, points, table + points_qrt);
    if (samples > 4) {
        transform_dct_main(samples, coefs, points_qrt, table);
        scramble_dct(samples, coefs, points, table + points_qrt);
    }
    else if (samples == 4) {
        transform_dct_main(samples, coefs, points_qrt, table);
    }
    post_dct(samples, coefs);
}

static void rad_rdft(int samples, int flags, float* coefs, int points, const float* table) {
    const int points_qrt = points >> 2;

    float v = (coefs[0] - coefs[1]) * 0.5;
    coefs[1] = v;
    coefs[0] = coefs[0] - v;

    if (samples > 4) {
        scramble_dft(samples, coefs, points, table + points_qrt);
        transform_dft_main(samples, coefs, points_qrt, table);
    }
    else if (samples == 4) {
        transform_dft_main(4, coefs, points_qrt, table);
    }
}

#if 0
#define MAX_POINTS 2048
#define BINKA_PI 3.14159265358979323846f

/* simple implementation for testing purposes (very slow but equivalent to the above)
 */
static void dct_type_iii(float* coefs, int points) {
    float temp[MAX_POINTS];

    //coefs[0] *= 0.5f; // first coefficient is normally halved but seems unneeded in binka
    for (int n = 0; n < points; n++) {
        float sum = coefs[0]; 
        for (int k = 1; k < points; ++k) {
            sum += coefs[k] * cos(BINKA_PI * k * (2.0f * n + 1.0f) / (2.0f * points));
        }
        temp[n] = sum; // * scale; // normalization factor, applied externally
    }

    // copy result back to original array
    for (int i = 0; i < points; i++) {
        coefs[i] = temp[i];
    }
}
#endif

void transform_dct(float* coefs, int frame_samples, int transform_size, const float* table) {
    rad_ddctf(frame_samples, coefs, transform_size, table);
}

void transform_rdft(float* coefs, int frame_samples, int transform_size, const float* table) {
    rad_rdft(frame_samples, -1, coefs, transform_size, table);
}
