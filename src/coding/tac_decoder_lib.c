#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* tri-Ace PS2 DCT-style codec.
 *
 * Adapted from Nisto's decoder w/ VU1 emulation:
 * - https://github.com/Nisto/pk3dec
 * Info:
 * - https://psi-rockin.github.io/ps2tek/
 * - https://github.com/PCSX2/pcsx2/blob/master/pcsx2/VUops.cpp
 *
 * Codec has no apparent name, but most functions mention "St" (stream?) and "Sac" (sound audio
 * compression?) and main lib is called "Sac" (Sac.cpp/Sac.dsm), also set in a "Csd" ELF. Looks inspired
 * by MPEG (much simplified) with bits from other codecs (per-file codebook and 1024 samples).
 *
 * Original decoder is mainly implemented in the PS2's VU1, a coprocessor specialized in vector/SIMD
 * and parallel instructions. As VU1 works with many 128 bit registers (typically x4 floats) algorithm
 * was tailored to do multiple ops at once. This code tries to simplify it into standard C to a point,
 * but keeps this vector style in main decoding to ease porting (since tables are made with SIMD in
 * mind it would need some transposing around) and for PS2 float simulation.
 *
 * Codec returns float samples then converted to PCM16. Output samples are +-1 vs Nisto's/PCSX2's
 * results, due to various quirks:
 * - simplified PS2 float handling (PS2 VU floats don't map 1:1 to PC IEEE floats), can be re-enabled (slow)
 * - various heisenbugs (PC 80b register floats <> 32b memory floats conversions, see transform()).
 *
 * Files are divided into blocks (size 0x4E000). At file start is a simple header and huffman codebook
 * then N VBR frames (of size around 0x200~300) containing huffman codes of spectral data. A frame has
 * codes for 2 channels, decoded separatedly (first all L then all R), then handle joint stereo.
 * Channel spectrum coefs are processeed, then MDCT(?) + window overlap to get final samples. When a
 * "block end frame" is found, handler must get next block and resume decoding (blocks may be pre/post
 * padded, for looping porposes). Game reads a couple of blocks at once though.
 */

/**********************************************************************************/
/* DEFINITIONS                                                                    */
/**********************************************************************************/
#include "tac_decoder_lib_data.h"
#include "tac_decoder_lib_ops.h"
#include "tac_decoder_lib.h"

//#define TAC_MAX_FRAME_SIZE  0x300 /* typically around ~0x1d0, observed max is ~0x2e2 */
#define TAC_CODED_BANDS     27
#define TAC_CODED_COEFS     32
#define TAC_TOTAL_POINTS    32 /* not sure about this term */
#define TAC_SCALE_TABLE_MAX_INDEX 511


struct tac_handle_t {
    /* base header */
    tac_header_t header;

    /* general state */
    int data_start;     /* first frame after huffman tables, within first block */
    int frame_offset;   /* current position within block */
    int frame_number;   /* frames must be sequential */

    /* decoding huffman tree state */
    int16_t huff_table_1[257]; /* init once */
    int16_t huff_table_2[TAC_CHANNELS][32]; /* saved between (some) frames */
    int16_t huff_table_3[258]; /* init once */
    uint8_t huff_table_4[16383]; /* init once */

    int16_t codes[TAC_CHANNELS][TAC_FRAME_SAMPLES];

    /* decoding vector state */
    REG_VF spectrum[TAC_CHANNELS][TAC_FRAME_SAMPLES / 4]; /* temp huffman-to-coefs */
    REG_VF wave[TAC_CHANNELS][TAC_FRAME_SAMPLES / 4]; /* final samples, in vector form */
    REG_VF hist[TAC_CHANNELS][TAC_FRAME_SAMPLES / 4]; /* saved between frames */
};


/**********************************************************************************/
/* MAIN DECODE                                                                    */
/**********************************************************************************/

/* similar to MP3's alias reduction step with pre-made SIMD tables:
 *  lo_out.M = (lo_in.M * AT[0x0+j].N) - (AT[0xF-j].M * lo_hi.N)
 *  hi_out.N = (lo_hi.N * AT[0x7-j].M) + (AT[0x8+j].N * lo_in.M) */
static void unpack_antialias(REG_VF* spectrum) {
    const REG_VF* AT = ANTIALIASING_TABLE;
    int i;
    int pos_lo = 0x7;
    int pos_hi = 0x8;

    for (i = 0; i < TAC_CODED_BANDS; i++) {
        for (int j = 0; j < 4; j++) {
            REG_VF lo_in, hi_in, lo_out, hi_out;

            LOAD (_xyzw, &lo_in, spectrum, pos_lo - j);
            LOAD (_xyzw, &hi_in, spectrum, pos_hi + j);

            MULx (____w, &lo_out, &lo_in,     &AT[0x0+j]);
            MSUBx(____w, &lo_out, &AT[0xF-j], &hi_in);
            MULw (_x___, &hi_out, &hi_in,     &AT[0x7-j]);
            MADDw(_x___, &hi_out, &AT[0x8+j], &lo_in);

            MULy (___z_, &lo_out, &lo_in,     &AT[0x0+j]);
            MSUBy(___z_, &lo_out, &AT[0xF-j], &hi_in);
            MULz (__y__, &hi_out, &hi_in,     &AT[0x7-j]);
            MADDz(__y__, &hi_out, &AT[0x8+j], &lo_in);

            MULz (__y__, &lo_out, &lo_in,     &AT[0x0+j]);
            MSUBz(__y__, &lo_out, &AT[0xF-j], &hi_in);
            MULy (___z_, &hi_out, &hi_in,     &AT[0x7-j]);
            MADDy(___z_, &hi_out, &AT[0x8+j], &lo_in);

            MULw (_x___, &lo_out, &lo_in,  &AT[0x0+j]);
            MSUBw(_x___, &lo_out, &AT[0xF-j], &hi_in);
            MULx (____w, &hi_out, &hi_in,  &AT[0x7-j]);
            MADDx(____w, &hi_out, &AT[0x8+j], &lo_in);

            STORE(_xyzw, spectrum, &lo_out, pos_lo - j);
            STORE(_xyzw, spectrum, &hi_out, pos_hi + j);
        }

        pos_lo += 0x8;
        pos_hi += 0x8;
    }
}


static inline int16_t clamp_s16(int16_t value, int16_t min, int16_t max) {
    if (value < min)
        return min;
    else if (value > max)
        return max;
    else
        return value;
}


/* converts 4 huffman codes to 4 spectrums coefs */
//SUB_1188 (Pass1_Start?)
static void unpack_code4(REG_VF* spectrum, const REG_VF* spc1, const REG_VF* spc2, const REG_VF* code, const REG_VF* idx, int out_pos) {
    const REG_VF* ST = SCALE_TABLE;
    REG_VF tbc1, tbc2, out;

    /* copy table coefs .N, unless huffman code was 0 */
    if (code->f.x != 0) {
        MOVEx(_x___, &tbc1, &ST[idx->i.x + 0]);
        MOVEx(_x___, &tbc2, &ST[idx->i.x + 1]);
    } else {
        MOVEx(_x___, &tbc1, &VECTOR_ZERO);
        MOVEx(_x___, &tbc2, &VECTOR_ZERO);
    }

    if (code->f.y != 0) {
        MOVEx(__y__, &tbc1, &ST[idx->i.y + 0]);
        MOVEx(__y__, &tbc2, &ST[idx->i.y + 1]);
    } else {
        MOVEx(__y__, &tbc1, &VECTOR_ZERO);
        MOVEx(__y__, &tbc2, &VECTOR_ZERO);
    }

    if (code->f.z != 0) {
        MOVEx(___z_, &tbc1, &ST[idx->i.z + 0]);
        MOVEx(___z_, &tbc2, &ST[idx->i.z + 1]);
    } else {
        MOVEx(___z_, &tbc1, &VECTOR_ZERO);
        MOVEx(___z_, &tbc2, &VECTOR_ZERO);
    }

    if (code->f.w != 0) {
        MOVEx(____w, &tbc1, &ST[idx->i.w + 0]);
        MOVEx(____w, &tbc2, &ST[idx->i.w + 1]);
    } else {
        MOVEx(____w, &tbc1, &VECTOR_ZERO);
        MOVEx(____w, &tbc2, &VECTOR_ZERO);
    }

    /* out = [signed] (scp1/scp2) * (tbc2 - tbc1) + tbc1 */
    DIV  (_xyzw, &out, spc1, spc2);
    SUB  (_xyzw, &tbc2, &tbc2, &tbc1);
    MUL  (_xyzw, &out, &out, &tbc2);
    ADD  (_xyzw, &out, &out, &tbc1);
    SIGN (_xyzw, &out, code);

    STORE(_xyzw, spectrum, &out, out_pos);
}


/* Unpacks huffman codes in one band into 32 spectrum coefs, using selected scales for that band. */
// SUB_C88
static void unpack_band(REG_VF* spectrum, const int16_t* codes, int band_pos, int* code_pos, int out_pos) {
    const REG_VF* ST = SCALE_TABLE;
    int i;
    int16_t base_index = codes[0]; /* table index, max ~35 */
    int16_t band_index = codes[band_pos]; /* table too */
    REG_VF scale;

    /* bad values should be caught by CRC check but for completeness */
    base_index = clamp_s16(base_index, 0, TAC_SCALE_TABLE_MAX_INDEX);
    band_index = clamp_s16(band_index, 0, TAC_SCALE_TABLE_MAX_INDEX-128);


    /* index zero = band is not coded and all of its coefs are 0 */
    if (band_index == 0) {
        for (i = 0; i < (TAC_CODED_COEFS / 4); i++) {
            STORE(_xyzw, spectrum, &VECTOR_ZERO, out_pos+i);
        }
        return;
    }

    /* put final band scale at .y */
    MULy (__y__, &scale, &ST[128 + band_index], &ST[base_index]);

    /* unpack coefs */
    for (i = 0; i < 8; i++) {
        REG_VF code, idx, tm01, tm02, tm03;
        REG_VF spc1, spc2;

        COPY (_xyzw, &code, &codes[(*code_pos)]);
        (*code_pos) += 4;

        /* scale coef then round down to int to get table indexes (!!!) */
        ABS  (_xyzw, &tm01, &code);
        MULy (_xyzw, &tm01, &tm01, &scale);
        FMUL (_xyzw, &tm02, &tm01, 512.0); /* 512 = SCALE_TABLE max */
        ADD  (_xyzw, &tm03, &tm02, &VECTOR_ONE);

        FTOI0(_xyzw, &idx, &tm02); /* keep idx as int for later (probably could use (int)f.N too) */
        ITOF0(_xyzw, &tm02, &idx);
        FMULf(_xyzw, &tm02, 0.00195313);

        FTOI0(_xyzw, &tm03, &tm03);
        ITOF0(_xyzw, &tm03, &tm03);
        FMULf(_xyzw, &tm03, 0.00195313);

        SUB  (_xyzw, &spc1, &tm01, &tm02);
        SUB  (_xyzw, &spc2, &tm03, &tm02);

        /* Also just in case. In rare cases index may access 511+1 but table takes this into account */
        idx.i.x = clamp_s16(idx.i.x, 0, TAC_SCALE_TABLE_MAX_INDEX);
        idx.i.y = clamp_s16(idx.i.y, 0, TAC_SCALE_TABLE_MAX_INDEX);
        idx.i.z = clamp_s16(idx.i.z, 0, TAC_SCALE_TABLE_MAX_INDEX);
        idx.i.w = clamp_s16(idx.i.w, 0, TAC_SCALE_TABLE_MAX_INDEX);

        unpack_code4(spectrum, &spc1, &spc2, &code, &idx, out_pos + i);
    }
}

/* Unpacks channel's huffman codes to spectrum coefs. Also done in the VU1 (uses VIFcode UNPACK V4-16
 * to copy 16b huffman codes to VU1 memory as 32b first) but it's simplified a bit here. */
// SUB_6E0
static void unpack_channel(REG_VF* spectrum, const int16_t* codes) {
    int i;

    /* Huffman codes has 1 base scale + 27 bands scales + N coefs (up to 27*32).
     * Not all bands store codes so an index is needed, after scales */
    int code_pos = TAC_CODED_BANDS + 1;
    int out_pos = 0x00;

    /* unpack bands */
    for (i = 1; i < TAC_CODED_BANDS + 1; i++) {
        unpack_band(spectrum, codes, i, &code_pos, out_pos);
        out_pos += (TAC_CODED_COEFS / 4); /* 8 vectors of 4 coefs, per band */
    }

    /* memset rest up to max (27*32/4)..(32*32/4) */
    for (i = 0xD8; i < 0x100; i++) {
       STORE (_xyzw, spectrum, &VECTOR_ZERO, i);
    }

    /* tweak spectrum */
    unpack_antialias(spectrum);
}


/* in GCC this function seems to cause heisenbugs, copy x4 below to get original results */
static void transform_dot_product(REG_VF* mac, const REG_VF* spectrum, const REG_VF* TT, int pos_i, int pos_t) {
    MUL  (_xyzw, mac, &spectrum[pos_i+0], &TT[pos_t+0]); /* resets mac */
    MADD (_xyzw, mac, &spectrum[pos_i+1], &TT[pos_t+1]);
    MADD (_xyzw, mac, &spectrum[pos_i+2], &TT[pos_t+2]);
    MADD (_xyzw, mac, &spectrum[pos_i+3], &TT[pos_t+3]);
    MADD (_xyzw, mac, &spectrum[pos_i+4], &TT[pos_t+4]);
    MADD (_xyzw, mac, &spectrum[pos_i+5], &TT[pos_t+5]);
    MADD (_xyzw, mac, &spectrum[pos_i+6], &TT[pos_t+6]);
    MADD (_xyzw, mac, &spectrum[pos_i+7], &TT[pos_t+7]);
}

/* take spectrum coefs and, ahem, transform somehow, possibly using a SIMD'd DCT table. */
//SUB_1410 (Idct_Start?)
static void transform(REG_VF* wave, const REG_VF* spectrum) {
    const REG_VF* TT = TRANSFORM_TABLE;
    int i, j;

    int pos_t = 0;
    int pos_o = 0;

    for (i = 0; i < TAC_TOTAL_POINTS; i++) {
        int pos_i = 0;
        REG_VF mac, ror, out;

        for (j = 0; j < 8; j++) {
            transform_dot_product(&mac, spectrum, TT, pos_i, pos_t);
            pos_i += 8;
            MR32 (_xyzw, &ror, &mac);
            ADD  (_x_z_, &ror, &ror, &mac);
            ADDz (_x___, &out, &ror, &ror);

            transform_dot_product(&mac, spectrum, TT, pos_i, pos_t);
            pos_i += 8;
            MR32 (_xyzw, &ror, &mac);
            ADD  (__y_w, &ror, &ror, &mac);
            ADDw (__y__, &out, &ror, &ror);

            transform_dot_product(&mac, spectrum, TT, pos_i, pos_t);
            pos_i += 8;
            MR32 (_xyzw, &ror, &mac);
            ADD  (_x_z_, &ror, &ror, &mac);
            ADDx (___z_, &out, &ror, &ror);

            transform_dot_product(&mac, spectrum, TT, pos_i, pos_t);
            pos_i += 8;
            MR32 (_xyzw, &ror, &mac);
            ADD  (__y_w, &ror, &ror, &mac);
            ADDy (____w, &out, &ror, &ror);

            FMULf(_xyzw, &out, 0.25);
            STORE(_xyzw, wave, &out, pos_o++);
        }

        pos_t += 0x08;
    }
}


/* process and apply window/overlap. Similar to MP3's synth granule function. */
//SUB_1690 (Pass3_Start?)
static void process(REG_VF* wave, REG_VF* hist) {
    const REG_VF* ST = SYNTH_TABLE;
    int i, j;

    int pos_o = 0;
    int pos_w = 0;
    int pos_h;
    int pos_r = 0x200; /* rolls down to 0, becoming 0x10 steps (0x00, 0xF0, 0xE0, ..., 0x00, 0xF0, ...) */

    for (i = 0; i < TAC_TOTAL_POINTS; i++) {
        REG_VF zero, neg1;
        /* Sorry... hopefully compiler optimizes. Probably could be simplified with some rearranging below */
        REG_VF tm00, tm01, tm02, tm03, tm04, tm05, tm06, tm07,
               tm10, tm11, tm12, tm13, tm14, tm15, tm16, tm17,
               tm20, tm21, tm22, tm23, tm24, tm25, tm26, tm27,
               tm30, tm31, tm32, tm33, tm34;
        /* tmp calcs, meant to be used nearby */
        REG_VF tmpZ;
        /* output temps, could STORE as calc'd (like VU1) but thought would be easier to read at the end */
        REG_VF out0, out1, out2, out3, out4, out5, out6, out7,
               out8, out9, outA, outB, outC, outD, outE, outF;

        pos_h = pos_r & 0xFF;
        pos_r = pos_r - 0x10;

        LOAD (_xyzw, &tm00, wave, pos_w+0);
        LOAD (_xyzw, &tm01, wave, pos_w+1);
        LOAD (_xyzw, &tm02, wave, pos_w+2);
        LOAD (_xyzw, &tm03, wave, pos_w+3);
        LOAD (_xyzw, &tm04, wave, pos_w+4);
        LOAD (_xyzw, &tm05, wave, pos_w+5);
        LOAD (_xyzw, &tm06, wave, pos_w+6);
        LOAD (_xyzw, &tm07, wave, pos_w+7);
        pos_w += 8;

        MOVE (_xyzw, &zero, &VECTOR_ZERO);  /* always 0, used for copying */
        MOVE (_xyzw, &neg1, &VECTOR_M_ONE);  /* always -1, used for negating */


        /* WTF is going on here? Yeah, no clue. Probably some multi-step FFT/DCT twiddle thing.
         * Remember all those separate ops are left as-is to allow PS2 float simulation (disabled though).
         * Tried cleaning up some more but... */
        ADDw (_x___, &tm10, &tm01, &tm00);
        ADDx (____w, &tm10, &tm01, &tm02);
        ADDx (____w, &tm11, &tm02, &tm03);
        ADDw (_x___, &tm12, &tm04, &tm03);
        ADDw (_x___, &tm13, &tm05, &tm04);
        ADDx (____w, &tm13, &tm05, &tm06);
        ADDx (____w, &tm12, &tm06, &tm07);
        ADDx (__y__, &tm11, &zero, &tm10);
        ADDw (___z_, &tm11, &zero, &tm10);
        ADDx (__y__, &tm12, &zero, &tm13);
        ADDw (___z_, &tm12, &zero, &tm13);
        ADDw (_x___, &tm14, &tm00, &tm07);
        SUBw (_x___, &tm15, &tm00, &tm07);
        ADDz (___z_, &tm16, &tm11, &tm12);
        ADDx (___z_, &tm17, &zero, &tm14);
        ADDx (____w, &tm16, &zero, &tm15);
        SUBz (___z_, &tm10, &tm11, &tm12);
        ADDz (_x___, &tm16, &tm12, &tm17);
        MULx (___z_, &tm13, &tm10, &ST[0x4]);
        ADDz (_x___, &tm17, &tm16, &tm16);
        ADDz (__y__, &tm16, &zero, &tm13);
        SUBx (___z_, &tm17, &tm17, &tm12);
        ADDy (____w, &tm14, &tm11, &tm12);
        ADDw (__y__, &tm17, &tm16, &tm16);
        SUBy (____w, &tm17, &tm16, &tm16);
        SUBz (_x___, &tm20, &tm16, &tm16);
        ADDw (__y__, &tm16, &tm11, &tm11);
        ADDw (___z_, &tm16, &zero, &tm14);
        ADDy (____w, &tm16, &tm12, &tm12);
        ADDw (__y__, &tm10, &tm11, &tm12);
        SUBw (__y__, &tm13, &tm11, &tm12);
        ADDw (__y__, &tm21, &tm11, &tm12);
        SUBw (__y__, &tm14, &tm16, &tm16);

        ADDy (___z_, &tm22, &tm16, &tm10);
        ADDy (____w, &tm22, &zero, &tm13);
        SUBz (__y__, &tm21, &tm21, &tm16);
        MULx (__y__, &tm16, &tm14, &ST[0x4]);
        ADDz (_x___, &tm23, &tm17, &tm22);
        ADDx (_x___, &tm24, &zero, &tm20);
        MULx (__y__, &tm21, &tm21, &ST[0x4]);
        ADDy (____w, &tm10, &tm22, &tm16);
        SUBy (____w, &tm14, &tm22, &tm16);
        SUBz (_x___, &tm25, &tm17, &tm22);
        ADDy (___z_, &tm23, &tm17, &tm21);
        MULw (_x___, &tm21, &ST[0x2], &tm10);
        MULx (____w, &tm14, &tm14, &ST[0x6]);

        ADDz (___z_, &tm20, &zero, &tm23);
        ADDx (__y__, &tm23, &tm17, &tm21);
        ADDw (___z_, &tm21, &zero, &tm14);
        ADDy (____w, &tm20, &zero, &tm23);
        ADDz (____w, &tm23, &tm17, &tm21);
        SUBz (____w, &tm25, &tm17, &tm21);
        SUBy (___z_, &tm24, &tm17, &tm21);
        SUBx (__y__, &tm25, &tm17, &tm21);
        ADDw (__y__, &tm20, &zero, &tm23);
        ADDw (__y__, &tm24, &zero, &tm25);
        ADDz (___z_, &tm25, &zero, &tm24);
        ADDy (____w, &tm24, &zero, &tm25);
        ADDz (__y__, &tm10, &tm00, &tm00);
        ADDz (__y__, &tm13, &tm03, &tm03);
        ADDz (__y__, &tm11, &tm01, &tm01);
        ADDy (___z_, &tm11, &tm02, &tm02);
        ADDy (_x___, &tm11, &zero, &tm10);
        ADDy (____w, &tm11, &zero, &tm13);
        ADDz (__y__, &tm10, &tm04, &tm04);
        ADDz (__y__, &tm13, &tm07, &tm07);
        ADDz (__y__, &tm12, &tm05, &tm05);
        ADDy (___z_, &tm12, &tm06, &tm06);
        ADDy (_x___, &tm12, &zero, &tm10);
        ADDy (____w, &tm12, &zero, &tm13);
        ADDz (__y__, &tm26, &tm11, &tm11);
        ADDz (__y__, &tm15, &tm12, &tm12);
        ADDx (____w, &tm14, &tm11, &tm12);
        ADDw (_x___, &tm27, &tm11, &tm12);
        SUBw (_x___, &tm10, &tm11, &tm12);
        ADDy (____w, &tm26, &zero, &tm15);
        ADDw (___z_, &tm26, &zero, &tm14);
        ADDy (_x___, &tm16, &tm11, &tm11);
        ADDx (____w, &tm16, &zero, &tm10);
        ADDw (__y__, &tm22, &tm26, &tm26);
        ADDz (_x___, &tm22, &tm27, &tm26);
        SUBw (__y__, &tm16, &tm26, &tm26);
        SUBz (_x___, &tm14, &tm27, &tm26);
        ADDw (___z_, &tm15, &tm11, &tm11);
        ADDy (_x___, &tm17, &tm22, &tm22);
        MULx (__y__, &tm16, &tm16, &ST[0x4]);
        ADDx (___z_, &tm17, &zero, &tm14);

        ADDy (_x___, &tm10, &tm12, &tm12);
        ADDw (__y__, &tm17, &tm16, &tm16);
        SUBy (____w, &tm17, &tm16, &tm16);
        ADDz (____w, &tm16, &tm12, &tm12);
        ADDx (___z_, &tm16, &zero, &tm10);
        ADDz (__y__, &tm16, &zero, &tm15);
        ADDx (_x___, &tm30, &tm23, &tm17);
        FMULf(_x___, &tm30, -1.0);
        MOVE (_x___, &outC, &tm30);
        ADDw (_x___, &tm27, &tm16, &tm16);
        ADDz (__y__, &tm27, &tm16, &tm16);
        SUBw (_x___, &tm13, &tm16, &tm16);
        ADDy (_x___, &tm14, &tm16, &tm16);
        ADDz (____w, &tm27, &tm16, &tm16);
        ADDx (____w, &tm22, &zero, &tm13);
        ADDx (___z_, &tm27, &zero, &tm14);
        SUBy (_x___, &tm13, &tm27, &tm27);
        SUBw (___z_, &tm15, &tm27, &tm27);
        MULx (_x___, &tm13, &tm13, &ST[0x4]);
        MULx (___z_, &tm15, &tm15, &ST[0x4]);
        ADDx (__y__, &tm21, &zero, &tm13);
        ADDz (____w, &tm27, &zero, &tm15);
        ADDw (____w, &tm14, &tm22, &tm27);
        SUBw (____w, &tm13, &tm22, &tm27);
        MULx (____w, &tm14, &tm14, &ST[0x2]);
        MULx (____w, &tm13, &tm13, &ST[0x6]);
        ADDw (_x___, &tm21, &zero, &tm14);
        ADDw (___z_, &tm21, &zero, &tm13);

        ADDx (__y__, &tmpZ, &tm17, &tm21);
        ADDy (___z_, &tmpZ, &tm17, &tm21);
        ADDz (____w, &tmpZ, &tm17, &tm21);
        SUBy (_x___, &tmpZ, &tm22, &tm22);
        MULx (__y__, &tm10, &tmpZ, &ST[0x1]);
        MULx (___z_, &tm10, &tmpZ, &ST[0x2]);
        MULx (____w, &tm10, &tmpZ, &ST[0x3]);
        MULx (_x___, &tm10, &tmpZ, &ST[0x4]);

        SUBx (_x___, &tm23, &tm23, &tm17); /* .x not used after this */
        MULx (_x___, &out0, &tm23, &ST[0x4]);

        SUBy (__y__, &tm23, &tm23, &tm10);
        SUBz (___z_, &tm23, &tm23, &tm10);
        SUBw (____w, &tm23, &tm23, &tm10);
        ADDw (__y__, &tm20, &tm20, &tm10);
        ADDz (___z_, &tm20, &tm20, &tm10);
        ADDy (____w, &tm20, &tm20, &tm10);
        ADDx (_x___, &tm20, &tm20, &tm10);
        SUBx (_x___, &tm24, &tm24, &tm10);

        SUBx (__y__, &tmpZ, &tm17, &tm21);
        SUBy (___z_, &tmpZ, &tm17, &tm21);
        SUBz (____w, &tmpZ, &tm17, &tm21);
        MULx (__y__, &tm10, &tmpZ, &ST[0x7]);
        MULx (___z_, &tm10, &tmpZ, &ST[0x6]);
        MULx (____w, &tm10, &tmpZ, &ST[0x5]);

        SUBw (__y__, &tm24, &tm24, &tm10);
        SUBz (___z_, &tm24, &tm24, &tm10);
        SUBy (____w, &tm24, &tm24, &tm10);

        ADDy (__y__, &tm25, &tm25, &tm10);
        ADDz (___z_, &tm25, &tm25, &tm10);
        ADDw (____w, &tm25, &tm25, &tm10);

        ADDy (_x___, &tm00, &tm00, &tm00);
        FMULf(_x___, &tm00, -1.0);
        ADDw (___z_, &tm10, &tm00, &tm00);
        ADDy (_x___, &tm10, &tm01, &tm01);
        FMULf(_x___, &tm10, -1.0);
        ADDz (____w, &tm00, &tm01, &tm01);
        ADDz (__y__, &tm00, &zero, &tm10);
        ADDy (_x___, &tm01, &tm02, &tm02);
        FMULf(_x___, &tm01, -1.0);
        ADDw (___z_, &tm10, &tm02, &tm02);
        ADDy (_x___, &tm13, &tm03, &tm03);
        FMULf(_x___, &tm13, -1.0);
        ADDx (___z_, &tm00, &zero, &tm10);
        ADDz (__y__, &tm01, &zero, &tm10);
        ADDz (____w, &tm01, &tm03, &tm03);
        ADDy (_x___, &tm02, &tm04, &tm04);
        FMULf(_x___, &tm02, -1.0);
        ADDw (___z_, &tm10, &tm04, &tm04);
        ADDx (___z_, &tm01, &zero, &tm13);
        ADDy (_x___, &tm13, &tm05, &tm05);
        FMULf(_x___, &tm13, -1.0);
        ADDz (__y__, &tm02, &zero, &tm10);
        ADDz (____w, &tm02, &tm05, &tm05);
        ADDy (_x___, &tm03, &tm06, &tm06);
        FMULf(_x___, &tm03, -1.0);
        ADDw (___z_, &tm10, &tm06, &tm06);
        ADDy (_x___, &tm14, &tm07, &tm07);
        FMULf(_x___, &tm14, -1.0);
        ADDx (___z_, &tm02, &zero, &tm13);
        ADDz (__y__, &tm03, &zero, &tm10);
        ADDz (____w, &tm03, &tm07, &tm07);

        ADDx (___z_, &tm03, &zero, &tm14);
        ADDz (__y__, &tm11, &tm00, &tm00);
        ADDx (____w, &tm10, &tm00, &tm01);
        ADDz (__y__, &tm10, &tm01, &tm01);
        ADDw (_x___, &tm12, &tm02, &tm01);
        ADDz (__y__, &tm12, &tm02, &tm02);
        ADDw (___z_, &tm11, &zero, &tm10);
        ADDy (____w, &tm11, &zero, &tm10);
        ADDx (____w, &tm10, &tm02, &tm03);
        ADDz (__y__, &tm10, &tm03, &tm03);
        ADDy (__y__, &tm13, &tm11, &tm12);
        ADDw (__y__, &tm26, &tm11, &tm11);
        ADDw (___z_, &tm12, &zero, &tm10);
        ADDy (____w, &tm12, &zero, &tm10);
        SUBw (__y__, &tm13, &tm13, &tm11);
        ADDy (____w, &tm10, &tm11, &tm12);
        ADDy (____w, &tm26, &tm12, &tm12);
        SUBz (___z_, &tm14, &tm11, &tm12);
        SUBw (__y__, &tm13, &tm13, &tm12);
        ADDw (___z_, &tm26, &zero, &tm10);
        ADDw (__y__, &tm10, &tm26, &tm26);
        MULx (__y__, &tm10, &tm10, &ST[0x4]);
        ADDz (__y__, &tm21, &zero, &tm14);
        ADDy (_x___, &tm31, &zero, &tm13);
        SUBw (__y__, &tm13, &tm26, &tm26);
        MULx (__y__, &tm13, &tm13, &ST[0x4]);

        ADDy (___z_, &tm21, &zero, &tm10);
        ADDy (___z_, &tm17, &zero, &tm13);
        ADDz (___z_, &tm10, &tm21, &tm26);
        SUBz (___z_, &tm13, &tm21, &tm26);
        ADDz (___z_, &tm30, &tm11, &tm12);
        MULx (___z_, &tm10, &tm10, &ST[0x2]);
        MULx (___z_, &tm13, &tm13, &ST[0x6]);
        MULx (___z_, &tm30, &tm30, &ST[0x4]);
        ADDz (__y__, &tm17, &zero, &tm10);
        ADDz (____w, &tm17, &zero, &tm13);
        ADDz (____w, &tm22, &zero, &tm30);
        SUBx (____w, &tm10, &tm22, &tm12);
        ADDw (_x___, &tm21, &tm12, &tm22);

        ADDw (___z_, &tm21, &zero, &tm10);
        ADDx (__y__, &tm32, &tm17, &tm21);
        ADDy (___z_, &tm32, &tm17, &tm21);
        FMUL (_x___, &tm33, &tm31, -1.0);
        ADDz (____w, &tm32, &tm17, &tm21);
        FMUL (__y__, &tm10, &tm32, -1.0);
        FMUL (___z_, &tm33, &tm32, -1.0);
        SUBz (____w, &tm14, &tm17, &tm21);
        FMUL (____w, &tm13, &tm32, -1.0);
        ADDy (____w, &tm33, &zero, &tm10);
        SUBy (___z_, &tm31, &tm17, &tm21);
        ADDw (__y__, &tm31, &zero, &tm14);
        ADDw (__y__, &tm33, &zero, &tm13);
        SUBx (__y__, &tm13, &tm17, &tm21);
        FMUL (___z_, &tm34, &tm31, -1.0);
        FMUL (__y__, &tm10, &tm31, -1.0);
        ADDy (____w, &tm31, &zero, &tm13);
        ADDy (____w, &tm34, &zero, &tm10);
        FMUL (____w, &tm10, &tm31, -1.0);
        ADDw (__y__, &tm34, &zero, &tm10);
        ADDy (_x___, &tm11, &tm00, &tm00);
        ADDw (___z_, &tm10, &tm00, &tm00);
        ADDy (_x___, &tm13, &tm01, &tm01);
        ADDz (____w, &tm11, &tm01, &tm01);
        ADDy (_x___, &tm12, &tm02, &tm02);
        ADDz (__y__, &tm11, &zero, &tm10);
        ADDx (___z_, &tm11, &zero, &tm13);
        ADDw (___z_, &tm10, &tm02, &tm02);
        ADDy (_x___, &tm13, &tm03, &tm03);
        ADDz (____w, &tm12, &tm03, &tm03);

        ADDz (__y__, &tm12, &zero, &tm10);
        ADDx (___z_, &tm12, &zero, &tm13);
        SUBy (_x___, &tm34, &tm11, &tm11);
        SUBw (___z_, &tm10, &tm11, &tm11);
        SUBy (_x___, &tm13, &tm12, &tm12);
        SUBw (___z_, &tm14, &tm12, &tm12);
        ADDz (__y__, &tm15, &tm12, &tm12);
        ADDz (_x___, &tm34, &tm34, &tm10);
        ADDx (____w, &tm10, &tm11, &tm12);
        ADDz (_x___, &tm13, &tm13, &tm14);
        ADDy (____w, &tm16, &zero, &tm15);
        ADDz (__y__, &tm16, &tm11, &tm11);
        ADDw (___z_, &tm16, &zero, &tm10);
        ADDx (_x___, &tm34, &tm34, &tm13);
        ADDz (____w, &tm26, &tm12, &tm12);
        ADDy (_x___, &tm14, &tm12, &tm12);
        ADDw (___z_, &tm15, &tm11, &tm11);
        ADDy (_x___, &tm26, &tm11, &tm11);
        ADDx (___z_, &tm26, &zero, &tm14);
        ADDz (__y__, &tm26, &zero, &tm15);
        ADDw (___z_, &tm13, &tm26, &tm26);
        ADDy (_x___, &tm27, &tm26, &tm26);
        ADDz (__y__, &tm10, &tm26, &tm26);
        ADDz (__y__, &tm27, &zero, &tm13);
        ADDy (____w, &tm27, &zero, &tm10);
        ADDy (_x___, &tm10, &tm27, &tm27);
        SUBy (_x___, &tm14, &tm27, &tm27);
        ADDy (____w, &tm22, &tm16, &tm16);
        SUBw (__y__, &tm21, &tm16, &tm16);
        MULx (_x___, &tm10, &tm10, &ST[0x4]);
        MULx (_x___, &tm14, &tm14, &ST[0x4]);
        MULx (____w, &tm22, &tm22, &ST[0x4]);
        ADDx (___z_, &tm21, &zero, &tm10);
        ADDx (___z_, &tm17, &zero, &tm14);
        SUBz (____w, &tm30, &tm22, &tm16);
        ADDw (___z_, &tm10, &tm21, &tm27);
        SUBw (___z_, &tm14, &tm21, &tm27);
        ADDz (____w, &tmpZ, &tm22, &tm16);
        ADDw (_x___, &tm21, &zero, &tmpZ);
        ADDw (___z_, &tm21, &zero, &tm30);
        MULx (___z_, &tm10, &tm10, &ST[0x2]);
        MULx (___z_, &tm14, &tm14, &ST[0x6]);
        ADDz (__y__, &tm17, &zero, &tm10);
        ADDz (____w, &tm17, &zero, &tm14);
        SUBy (_x___, &tm30, &tm26, &tm26);
        ADDx (__y__, &tm10, &tm17, &tm21);
        ADDy (___z_, &tm10, &tm17, &tm21);
        ADDz (____w, &tm10, &tm17, &tm21);
        ADDz (_x___, &tm30, &tm30, &tm26);
        MULx (__y__, &tm10, &tm10, &ST[0x1]);
        MULx (___z_, &tm10, &tm10, &ST[0x2]);
        MULx (____w, &tm10, &tm10, &ST[0x3]);
        SUBw (_x___, &tm30, &tm30, &tm26);
        MULx (_x___, &tm30, &tm30, &ST[0x4]);

        ADDy (__y__, &tmpZ, &tm32, &tm10);
        ADDz (___z_, &tmpZ, &tm32, &tm10);
        ADDw (____w, &tmpZ, &tm32, &tm10);
        MULz (__y__, &tm32, &tmpZ, &ST[0x0]);
        MULx (___z_, &tm32, &tmpZ, &ST[0x1]);
        MULz (____w, &tm32, &tmpZ, &ST[0x1]);
        MOVE (_x___, &tm32, &zero);

        ADDy (____w, &tm33, &tm33, &tm10);
        ADDz (___z_, &tm33, &tm33, &tm10);
        ADDw (__y__, &tm33, &tm33, &tm10);
        ADDx (_x___, &tm33, &tm33, &tm30);
        MULx (_x___, &tm33, &tm33, &ST[0x6]);

        SUBz (____w, &tmpZ, &tm17, &tm21);
        SUBy (___z_, &tmpZ, &tm17, &tm21);
        SUBx (__y__, &tmpZ, &tm17, &tm21);
        MULx (____w, &tm10, &tmpZ, &ST[0x5]);
        MULx (___z_, &tm10, &tmpZ, &ST[0x6]);
        MULx (__y__, &tm10, &tmpZ, &ST[0x7]);

        ADDx (_x___, &tmpZ, &tm31, &tm30);
        ADDw (__y__, &tmpZ, &tm31, &tm10);
        ADDz (___z_, &tmpZ, &tm31, &tm10);
        ADDy (____w, &tmpZ, &tm31, &tm10);
        MULx (_x___, &tm31, &tmpZ, &ST[0x2]);
        MULz (__y__, &tm31, &tmpZ, &ST[0x2]);
        MULx (___z_, &tm31, &tmpZ, &ST[0x3]);
        MULz (____w, &tm31, &tmpZ, &ST[0x3]);

        ADDy (__y__, &tmpZ, &tm23, &tm32);
        ADDz (___z_, &tmpZ, &tm23, &tm32);
        ADDw (____w, &tmpZ, &tm23, &tm32);
        ADDx (_x___, &tmpZ, &tm24, &tm31);
        MULy (__y__, &out0, &tmpZ, &ST[0x4]);
        MULz (___z_, &out0, &tmpZ, &ST[0x4]);
        MULw (____w, &out0, &tmpZ, &ST[0x4]);
        MULx (_x___, &out1, &tmpZ, &ST[0x5]);
        MULy (____w, &out7, &neg1, &out0);
        MULz (___z_, &out7, &neg1, &out0);
        MULw (__y__, &out7, &neg1, &out0);
        MULx (_x___, &out7, &neg1, &out1);

        SUBy (__y__, &tmpZ, &tm32, &tm23);
        SUBz (___z_, &tmpZ, &tm32, &tm23);
        SUBw (____w, &tmpZ, &tm32, &tm23);
        SUBx (_x___, &tmpZ, &tm31, &tm24);
        MULy (____w, &outF, &ST[0x3], &tmpZ);
        MULz (___z_, &outF, &ST[0x3], &tmpZ);
        MULw (__y__, &outF, &ST[0x3], &tmpZ);
        MULx (_x___, &outF, &ST[0x3], &tmpZ);
        ADDw (__y__, &out8, &zero, &outF);
        ADDz (___z_, &out8, &zero, &outF);
        ADDy (____w, &out8, &zero, &outF);
        ADDx (_x___, &out9, &zero, &outF);

        ADDw (____w, &tmpZ, &tm34, &tm10);
        ADDz (___z_, &tmpZ, &tm34, &tm10);
        ADDy (__y__, &tmpZ, &tm34, &tm10);
        MULz (__y__, &tm34, &tmpZ, &ST[0x4]);
        MULx (___z_, &tm34, &tmpZ, &ST[0x5]);
        MULz (____w, &tm34, &tmpZ, &ST[0x5]);
        MULx (_x___, &tm34, &tm34, &ST[0x4]);

        ADDy (__y__, &tmpZ, &tm24, &tm31);
        ADDz (___z_, &tmpZ, &tm24, &tm31);
        ADDw (____w, &tmpZ, &tm24, &tm31);
        ADDx (_x___, &tmpZ, &tm25, &tm34);
        MULy (__y__, &out1, &tmpZ, &ST[0x5]);
        MULz (___z_, &out1, &tmpZ, &ST[0x5]);
        MULw (____w, &out1, &tmpZ, &ST[0x5]);
        MULx (_x___, &out2, &tmpZ, &ST[0x6]);
        MULy (____w, &out6, &neg1, &out1);
        MULz (___z_, &out6, &neg1, &out1);
        MULw (__y__, &out6, &neg1, &out1);
        MULx (_x___, &out6, &neg1, &out2);

        SUBy (__y__, &tmpZ, &tm31, &tm24);
        SUBz (___z_, &tmpZ, &tm31, &tm24);
        SUBw (____w, &tmpZ, &tm31, &tm24);
        SUBx (_x___, &tmpZ, &tm34, &tm25);
        MULw (__y__, &outE, &ST[0x2], &tmpZ);
        MULz (___z_, &outE, &ST[0x2], &tmpZ);
        MULy (____w, &outE, &ST[0x2], &tmpZ);
        MULx (_x___, &outE, &ST[0x2], &tmpZ);
        ADDw (__y__, &out9, &zero, &outE);
        ADDz (___z_, &out9, &zero, &outE);
        ADDy (____w, &out9, &zero, &outE);
        ADDx (_x___, &outA, &zero, &outE);

        ADDy (__y__, &tmpZ, &tm25, &tm34);
        ADDz (___z_, &tmpZ, &tm25, &tm34);
        ADDw (____w, &tmpZ, &tm25, &tm34);
        ADDx (_x___, &tmpZ, &tm20, &tm33);
        MULy (__y__, &out2, &tmpZ, &ST[0x6]);
        MULz (___z_, &out2, &tmpZ, &ST[0x6]);
        MULw (____w, &out2, &tmpZ, &ST[0x6]);
        MULx (_x___, &out3, &tmpZ, &ST[0x7]);
        MULy (____w, &out5, &neg1, &out2);
        MULz (___z_, &out5, &neg1, &out2);
        MULw (__y__, &out5, &neg1, &out2);
        MULx (_x___, &out5, &neg1, &out3);

        SUBy (__y__, &tmpZ, &tm34, &tm25);
        SUBz (___z_, &tmpZ, &tm34, &tm25);
        SUBw (____w, &tmpZ, &tm34, &tm25);
        SUBx (_x___, &tmpZ, &tm33, &tm20);
        MULw (__y__, &outD, &ST[0x1], &tmpZ);
        MULz (___z_, &outD, &ST[0x1], &tmpZ);
        MULy (____w, &outD, &ST[0x1], &tmpZ);
        MULx (_x___, &outD, &ST[0x1], &tmpZ);
        ADDw (__y__, &outA, &zero, &outD);
        ADDz (___z_, &outA, &zero, &outD);
        ADDy (____w, &outA, &zero, &outD);
        ADDx (_x___, &outB, &zero, &outD);

        MULz (__y__, &tm33, &tm33, &ST[0x6]);
        MULx (___z_, &tm33, &tm33, &ST[0x7]);
        MULz (____w, &tm33, &tm33, &ST[0x7]);

        ADDy (__y__, &tmpZ, &tm20, &tm33);
        ADDz (___z_, &tmpZ, &tm20, &tm33);
        ADDw (____w, &tmpZ, &tm20, &tm33);
        MULw (____w, &out3, &tmpZ, &ST[0x7]);
        MULz (___z_, &out3, &tmpZ, &ST[0x7]);
        MULy (__y__, &out3, &tmpZ, &ST[0x7]);
        MULw (__y__, &out4, &neg1, &out3);
        MULz (___z_, &out4, &neg1, &out3);
        MULy (____w, &out4, &neg1, &out3);
        MOVE (_x___, &out4, &zero);

        SUBy (__y__, &tmpZ, &tm33, &tm20);
        SUBz (___z_, &tmpZ, &tm33, &tm20);
        SUBw (____w, &tmpZ, &tm33, &tm20);
        MULw (__y__, &outC, &ST[0x0], &tmpZ);
        MULz (___z_, &outC, &ST[0x0], &tmpZ);
        MULy (____w, &outC, &ST[0x0], &tmpZ);
        ADDw (__y__, &outB, &zero, &outC);
        ADDz (___z_, &outB, &zero, &outC);
        ADDy (____w, &outB, &zero, &outC);
        MULx (_x___, &out8, &neg1, &out0);

        /* current output */
        STORE(_xyzw, hist, &out0, pos_h + 0x0);
        STORE(_xyzw, hist, &out1, pos_h + 0x1);
        STORE(_xyzw, hist, &out2, pos_h + 0x2);
        STORE(_xyzw, hist, &out3, pos_h + 0x3);
        STORE(_xyzw, hist, &out4, pos_h + 0x4);
        STORE(_xyzw, hist, &out5, pos_h + 0x5);
        STORE(_xyzw, hist, &out6, pos_h + 0x6);
        STORE(_xyzw, hist, &out7, pos_h + 0x7);
        STORE(_xyzw, hist, &out8, pos_h + 0x8);
        STORE(_xyzw, hist, &out9, pos_h + 0x9);
        STORE(_xyzw, hist, &outA, pos_h + 0xA);
        STORE(_xyzw, hist, &outB, pos_h + 0xB);
        STORE(_xyzw, hist, &outC, pos_h + 0xC);
        STORE(_xyzw, hist, &outD, pos_h + 0xD);
        STORE(_xyzw, hist, &outE, pos_h + 0xE);
        STORE(_xyzw, hist, &outF, pos_h + 0xF);

        /* hist/window overlap and update final wave */
        for (j = 0; j < 8; j++) {
            const REG_VF* WT = WINDOW_TABLE;
            REG_VF out, rnd;

            MUL  (_xyzw, &out, &hist[(pos_h + 0x00) & 0xFF], &WT[0x00+j]);
            MADD (_xyzw, &out, &hist[(pos_h + 0x18) & 0xFF], &WT[0x08+j]);
            MADD (_xyzw, &out, &hist[(pos_h + 0x20) & 0xFF], &WT[0x10+j]);
            MADD (_xyzw, &out, &hist[(pos_h + 0x38) & 0xFF], &WT[0x18+j]);
            MADD (_xyzw, &out, &hist[(pos_h + 0x40) & 0xFF], &WT[0x20+j]);
            MADD (_xyzw, &out, &hist[(pos_h + 0x58) & 0xFF], &WT[0x28+j]);
            MADD (_xyzw, &out, &hist[(pos_h + 0x60) & 0xFF], &WT[0x30+j]);
            MADD (_xyzw, &out, &hist[(pos_h + 0x78) & 0xFF], &WT[0x38+j]);
            MADD (_xyzw, &out, &hist[(pos_h + 0x80) & 0xFF], &WT[0x40+j]);
            MADD (_xyzw, &out, &hist[(pos_h + 0x98) & 0xFF], &WT[0x48+j]);
            MADD (_xyzw, &out, &hist[(pos_h + 0xA0) & 0xFF], &WT[0x50+j]);
            MADD (_xyzw, &out, &hist[(pos_h + 0xB8) & 0xFF], &WT[0x58+j]);
            MADD (_xyzw, &out, &hist[(pos_h + 0xC0) & 0xFF], &WT[0x60+j]);
            MADD (_xyzw, &out, &hist[(pos_h + 0xD8) & 0xFF], &WT[0x68+j]);
            MADD (_xyzw, &out, &hist[(pos_h + 0xE0) & 0xFF], &WT[0x70+j]);
            MADD (_xyzw, &out, &hist[(pos_h + 0xF8) & 0xFF], &WT[0x78+j]);

            pos_h++;

            /* base volume and +-0.5 to final sample (+-32767.0) */
            MUL  (_xyzw, &out, &out, &VECTOR_VOLUME);

            MOVE (_xyzw, &rnd, &VECTOR_ROUND);
            SIGN (_xyzw, &rnd, &out);
            ADD  (_xyzw, &out, &out, &rnd);

            STORE(_xyzw, wave, &out, pos_o++);
        }
    }
}


///////////////////////////////////////////////////////////////////////////////

/* main decoding in the VU1 coprocessor */
static void decode_vu1(tac_handle_t* h) {
    int ch;

    for (ch = 0; ch < TAC_CHANNELS; ch++) {
        unpack_channel(h->spectrum[ch], h->codes[ch]);

        transform(h->wave[ch], h->spectrum[ch]);

        process(h->wave[ch], h->hist[ch]);
    }

    /* Decoded data is originally stored in VUMem1 as clamped ints, though final step
     * seems may be done done externally (StMakeFinalOut/StFlushWriteBuffer) */
}

/* Create final output samples */
// StMakeFinalOut
static void finalize_output(tac_handle_t* h) {
    int i;

    /* original code copies + clamps to PCM buffer here instead of modifying wave,
     * but we do it later to potentially allow float output. It also sets total output:
     * - type 1 (at loop frame): start_sample = loop_discard, frame_samples = 1024 - loop_discard
     * - type 2 (at last frame): start_sample = 0, frame_samples = frame_last + 1
     * - other: start_sample = 0, frame_samples = 1024
     * (only copies or does joint stereo from start_sample) */

    if (h->header.joint_stereo) {
        REG_VF* wave_l = h->wave[0];
        REG_VF* wave_r = h->wave[1];

        /* Combine joint stereo channels that encode diffs in L/R ("MS stereo"). In pseudo-mono files R has */
        /* all samples as 0 (R only saves 28 huffman codes, signalling no coefs per 1+27 bands) */
        for (i = 0; i < TAC_TOTAL_POINTS * 8; i++) {
            REG_VF samples_l, samples_r;

            ADD  (_xyzw, &samples_l, &wave_l[i], &wave_r[i]); /* L = L + R */
            SUB  (_xyzw, &samples_r, &wave_l[i], &wave_r[i]); /* R = L - R */
            MOVE (_xyzw, &wave_l[i], &samples_l);
            MOVE (_xyzw, &wave_r[i], &samples_r);
        }
    }
}

/* read huffman codes for all channels (max per channel 27*32 = 864 + 27 + 1 = 892) */
static int read_codes(tac_handle_t* h, const uint8_t* ptr, uint16_t huff_flag, uint32_t huff_cfg) {
    int huff_count = 0;
    int ch;
    uint32_t unkA = 0;
    uint32_t unkB = huff_cfg;
    uint32_t unkC = 0;
    uint32_t unkD = 0xFFFFFFFF;

    for (ch = 0; ch < TAC_CHANNELS; ch++) {
        int huff_done = 0;
        int huff_todo = 28;
        int16_t huff_val = 0;

        for (; huff_done < huff_todo; huff_done++) {
            unkD = unkD >> 14;
            unkA = h->huff_table_4[(unkB - unkC) / unkD];
            unkC += h->huff_table_3[unkA] * unkD;
            unkD *= h->huff_table_1[unkA];

            while (0xFFFFFF >= (unkC ^ (unkC + unkD))) {
                unkB = (unkB << 8) | (*ptr++);
                unkD = (unkD << 8);
                unkC = (unkC << 8);
            }

            while (0xFFFF >= unkD) {
                unkD = (((~unkC) + 1) & 0xFFFF) << 8;
                unkB = (unkB << 8) | (*ptr++);
                unkC = (unkC << 8);
            }

            if (unkA >= 0xFE) {
                uint32_t unkT;
                unkT = unkA == 0xFE;
                unkD = unkD >> (unkT ? 8 : 13);
                unkA = (unkB - unkC) / unkD;
                unkC = unkC + (unkA * unkD);
                if (unkT)
                    unkA += 0xFE;

                while (0xFFFFFF >= (unkC ^ (unkC + unkD))) {
                    unkB = (unkB << 8) | (*ptr++);
                    unkD = (unkD << 8);
                    unkC = (unkC << 8);
                }

                while (0xFFFF >= unkD) {
                    unkD = (((~unkC) + 1) & 0xFFFF) << 8;
                    unkB = (unkB << 8) | (*ptr++);
                    unkC = (unkC << 8);
                }
            }

            if (unkA & 1) {
                huff_val = -((((int16_t)unkA) + 1) / 2);
            } else {
                huff_val = unkA / 2;
            }

            if (huff_done < 28) {
                if (huff_flag) {
                    huff_val += h->huff_table_2[ch][huff_done];
                }

                h->huff_table_2[ch][huff_done] = huff_val;

                if (huff_done != 0 && (huff_val << 16) != 0) {
                    huff_todo += 32;
                }
            }

            h->codes[ch][huff_done] = huff_val;
        }

        huff_count += huff_done;
    }

    return huff_count;
}


/* CRC-16/GENIBUS implementation */
/* https://reveng.sourceforge.io/crc-catalogue/16.htm#crc.cat.crc-16-genibus */

#define CRC16_INIT     0xFFFF
#define CRC16_POLY     0x1021
#define CRC16_XOR_OUT  0xFFFF

static uint16_t crc16(const uint8_t* data, int length) {
    uint16_t i, crc = CRC16_INIT;

    while (length--) {
        for (crc ^= *data++ << 8, i = 0; i < 8; i++) {
            crc = (crc & 0x8000) ? crc << 1 ^ CRC16_POLY : crc << 1;
        }
    }

    return crc ^ CRC16_XOR_OUT;
}

/* ************************************************************************* */
/* SETUP                                                                     */
/* ************************************************************************* */

static uint32_t get_u32be(const uint8_t* mem) {
    return ((uint32_t)mem[0] << 24) | ((uint32_t)mem[1] << 16) | ((uint32_t)mem[2] << 8) | (uint32_t)mem[3];
}

static uint32_t get_u32le(const uint8_t* mem) {
    return ((uint32_t)mem[3] << 24) | ((uint32_t)mem[2] << 16) | ((uint32_t)mem[1] << 8) | (uint32_t)mem[0];
}

static uint16_t get_u16le(const uint8_t* mem) {
    return ((uint16_t)mem[1] << 8) | (uint16_t)mem[0];
}

static int init_header(tac_header_t* header, const uint8_t* buf) {
    header->huffman_offset  = get_u32le(buf+0x00);
    header->unknown         = get_u32le(buf+0x04);
    header->loop_frame      = get_u16le(buf+0x08);
    header->loop_discard    = get_u16le(buf+0x0A);
    header->frame_count     = get_u16le(buf+0x0C);
    header->frame_last      = get_u16le(buf+0x0E);
    header->loop_offset     = get_u32le(buf+0x10);
    header->file_size       = get_u32le(buf+0x14);
    header->joint_stereo    = get_u32le(buf+0x18);
    header->empty           = get_u32le(buf+0x1c);

    /* huffman table offset should make sense */
    if (header->huffman_offset < 0x20 || header->huffman_offset > TAC_BLOCK_SIZE)
        return TAC_PROCESS_HEADER_ERROR;
    /* header size ia block-aligned (but actual size can be smaller, ex. VP 00000715) */
    if (header->file_size % TAC_BLOCK_SIZE != 0)
        return TAC_PROCESS_HEADER_ERROR;
    /* loop_discard over max makes game crash, while frame_last seems to ignore it */
    if (header->loop_discard > TAC_FRAME_SAMPLES || header->frame_last + 1 > TAC_FRAME_SAMPLES)
        return TAC_PROCESS_HEADER_ERROR;
    /* looping makes sense */
    if (header->loop_frame > header->frame_count || header->loop_offset > header->file_size)
        return TAC_PROCESS_HEADER_ERROR;
    /* just in case */
    if ((header->joint_stereo != 0 && header->joint_stereo != 1) || header->empty != 0)
        return TAC_PROCESS_HEADER_ERROR;

    return TAC_PROCESS_OK;
}


/* AKA RangeDecodeInit (Csd module) */
static int init_huffman(tac_handle_t* h, const uint8_t* buf) {
    uint8_t idx = 0;
    int offset = 0;
    int i, j;

    /* initialize huff_table_1 with values from header */
    for (i = 0; i < 256; i++) {
        int16_t n = buf[offset++];

        if (n & 0x80) {
            n &= 0x7F;
            n |= buf[offset++] << 7;
        }

        h->huff_table_1[i] = n;
    }

    /* zero-initialize huff_table_2 */
    for (i = 0; i < TAC_CHANNELS; i++) {
        for (j = 0; j < 32; j++) {
            h->huff_table_2[i][j] = 0;
        }
    }

    /* initialize huff_table_3 */
    h->huff_table_1[256] = 1;
    h->huff_table_3[0] = 0;
    for (i = 1, j = 0; i < 258; i++, j++) {
        h->huff_table_3[i] = h->huff_table_3[j] + h->huff_table_1[j];
    }

    /* initialize huff_table_4 */
    for (idx = 0; !h->huff_table_1[idx]; idx++) {
        ;
    }

    for (i = 0; i < 16383; i++) {
        if (i >= h->huff_table_3[idx+1]) {
            while (!h->huff_table_1[++idx]) {
                ;
            }
        }
        h->huff_table_4[i] = idx;
    }

    return offset;
}

/* ************************************************************************* */
/* API                                                                       */
/* ************************************************************************* */

tac_handle_t* tac_init(const uint8_t* buf, int buf_size) {
    tac_handle_t* handle = NULL;

    /* assumes 1 block */
    if (buf_size < TAC_BLOCK_SIZE)
        goto fail;

    handle = malloc(sizeof(tac_handle_t));
    if (!handle) goto fail;

    {
        int res, pos;

        res = init_header(&handle->header, buf);
        if (res != TAC_PROCESS_OK) goto fail;

        pos = init_huffman(handle, &buf[handle->header.huffman_offset]);
        if (pos <= 0) goto fail;

        handle->data_start = handle->header.huffman_offset + pos;
        if (handle->data_start > TAC_BLOCK_SIZE)
            goto fail;
    }

    tac_reset(handle);

    return handle;
fail:
    tac_free(handle);
    return NULL;
}

const tac_header_t* tac_get_header(tac_handle_t* handle) {
    if (!handle)
        return NULL;
    return &handle->header;
}

void tac_free(tac_handle_t* handle) {
    if (!handle)
        return;
    free(handle);
}

void tac_reset(tac_handle_t* handle) {
    if (!handle)
        return;

    handle->frame_offset = handle->data_start;
    handle->frame_number = 1;

    memset(handle->hist, 0, sizeof(REG_VF) * TAC_CHANNELS * (TAC_FRAME_SAMPLES / 4));
    memset(handle->huff_table_2, 0, sizeof(int16_t) * TAC_CHANNELS * 32);
}


int tac_decode_frame(tac_handle_t* handle, const uint8_t* block) {
    int pos = handle->frame_offset;

    if (handle->frame_number > handle->header.frame_count)
        return TAC_PROCESS_DONE;

    if (pos + 0x04 > TAC_BLOCK_SIZE)
        return TAC_PROCESS_ERROR_SIZE;

    /* new block marker (may be right at block's end) */
    if (get_u32le(block + pos) == 0xFFFFFFFF) {
        handle->frame_offset = 0; /* start at the beginning of next block */
        return TAC_PROCESS_NEXT_BLOCK;
    }

    if (pos + 0x0C > TAC_BLOCK_SIZE)
        return TAC_PROCESS_ERROR_SIZE;


    /* read new frame */
    {
        const uint8_t* buf = &block[pos]; /* current frame */
        uint16_t frame_crc  = get_u16le(buf + 0x00); /* checksum of data starting at 0x04 */
        uint16_t huff_flag  = get_u16le(buf + 0x02) >> 15; /* 0 every 64th frame, 1 for others */
        uint16_t frame_size = get_u16le(buf + 0x02) & 0x7FFF; /* not including base header (0x08) */
        uint16_t frame_id   = get_u16le(buf + 0x04); /* current number */
        uint16_t huff_count = get_u16le(buf + 0x06); /* huffman decoded length */

        uint32_t huff_cfg   = get_u32be(buf + 0x08); /* huffman table setup */
        uint16_t crc_calc, huff_read;

        if (frame_id != handle->frame_number)
            return TAC_PROCESS_ERROR_ID;

        if (pos + 0x08 + frame_size > TAC_BLOCK_SIZE)
            return TAC_PROCESS_ERROR_SIZE;

        /* from tests seems CRC errors cause current frame to be skipped, so change values before validations */
        handle->frame_number++;
        handle->frame_offset += 0x08 + frame_size;

        crc_calc = crc16(buf + 0x04, 0x04 + frame_size);
        if (frame_crc != crc_calc)
            return TAC_PROCESS_ERROR_CRC;

        /* extract huffman frame codes */
        huff_read = read_codes(handle, buf + 0x0C, huff_flag, huff_cfg);
        if (huff_read != huff_count)
            return TAC_PROCESS_ERROR_HUFFMAN;

        /* main decode */
        decode_vu1(handle);

        /* post process */
        finalize_output(handle);
    }

    /* current frame decoded and samples can be requested */
    return TAC_PROCESS_OK;
}


static inline int16_t clamp16f(float sample) {
    if (sample > 32767.0)
        return 32767;
    else if (sample < -32768.0)
        return -32768;
    return (int16_t)sample;
}

void tac_get_samples_pcm16(tac_handle_t* handle, int16_t* dst) {
    int ch, i;
    int chs = TAC_CHANNELS;

    for (ch = 0; ch < chs; ch++) {
        int s = 0;
        for (i = 0; i < TAC_FRAME_SAMPLES / 4; i++) {
            dst[(s+0)*chs + ch] = clamp16f(handle->wave[ch][i].f.x);
            dst[(s+1)*chs + ch] = clamp16f(handle->wave[ch][i].f.y);
            dst[(s+2)*chs + ch] = clamp16f(handle->wave[ch][i].f.z);
            dst[(s+3)*chs + ch] = clamp16f(handle->wave[ch][i].f.w);
            s += 4;
        }
    }
}


void tac_set_loop(tac_handle_t* handle) {
    handle->frame_number = handle->header.loop_frame;
    handle->frame_offset = 0;
}
