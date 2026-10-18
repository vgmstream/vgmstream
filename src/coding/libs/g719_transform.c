#include "g719_internal.h"
#include <math.h>

//-------------------------------------------------------------------------------------------------
// TRANSFORM

#define NUM_TIME_SWITCHING_BLOCKS   4
#define FREQ_LENGTH                 800

#define  MLT960_LENGTH              960
#define  MLT960_LENGTH_DIV_2        480
#define  MLT960_LENGTH_MINUS_1      959
#define  MLT240_LENGTH              240
#define  MLT240_LENGTH_MINUS_1      239

#define  NUMC_G23       256
#define  NUMC_G1SUB     32
#define  NUMC_G1G2SUB   64
#define  NUMC_G1G2G3SUB 136



// OG: these complex_t helpers are inlined

/*--------------------------------------------------------------------------*/
/*  Add complex_t variables                                                 */
/*--------------------------------------------------------------------------*/
/*  complex_t   var1 (i)  variable 1                                        */
/*  complex_t   var2 (i)  variable 2                                        */
/*  complex_t   *res (o)  result of addition                                */
/*--------------------------------------------------------------------------*/
static inline void cadd(complex_t var1, complex_t var2, complex_t *res) {
    res->r = var1.r + var2.r;
    res->i = var1.i + var2.i;
}

/*--------------------------------------------------------------------------*/
/*  Subtract complex_t variables                                            */
/*--------------------------------------------------------------------------*/
/*  complex_t   var1 (i)  variable 1                                        */
/*  complex_t   var2 (i)  variable 2                                        */
/*  complex_t   *res (o)  result of subtraction                             */
/*--------------------------------------------------------------------------*/
static inline void csub(complex_t var1, complex_t var2, complex_t *res) {
    res->r = var1.r - var2.r;
    res->i = var1.i - var2.i;
}

/*--------------------------------------------------------------------------*/
/*  Multiply complex_t variable with a scalar                               */
/*--------------------------------------------------------------------------*/
/*  complex_t   var1 (i)  variable 1                                        */
/*  float     var2 (i)  variable 2                                          */
/*  complex_t   *res (o)  result                                            */
/*--------------------------------------------------------------------------*/
static inline void cmpys(complex_t var1, float var2, complex_t *res) {
    res->r = var1.r * var2;
    res->i = var1.i * var2;
}

/*--------------------------------------------------------------------------*/
/*  Find orthogonal complex_t variable                                      */
/*--------------------------------------------------------------------------*/
/*  complex_t   *v1  (i/o)  variable                                        */
/*--------------------------------------------------------------------------*/
static inline void cmpyj(complex_t *v1) {
    float a;

    a = v1->r;
    v1->r = -v1->i;
    v1->i = a;
}

/*--------------------------------------------------------------------------*/
/*  Winograd 120 point FFT                                                  */
/*--------------------------------------------------------------------------*/
/*  complex_t      *xi      (i/o)    input and output of the FFT            */
/*--------------------------------------------------------------------------*/
static void wfft120(complex_t *xi) {
    short ind0, n3, n5;
    float t0r, t1r, t2r, t3r, t4r, t5r, t6r, t7r, q1r, x1r, x5r;
    float t0i, t1i, t2i, t3i, t4i, t5i, t6i, t7i, q1i, x1i, x5i;
    float *ps;
    complex_t x[144];
    complex_t *p0, *p1, *p2, *p3, *p4, *p5, *p6, *p7;
    complex_t *pi0, *pi1, *pi2, *pi3, *pi4, *pi5, *pi6, *pi7;

    p0 = x;
    for (ind0 = 0; ind0 < 120; ind0 += 8) {
        pi0 = xi + indxPre[ind0 + 0];
        pi1 = xi + indxPre[ind0 + 1];
        pi2 = xi + indxPre[ind0 + 2];
        pi3 = xi + indxPre[ind0 + 3];
        pi4 = xi + indxPre[ind0 + 4];
        pi5 = xi + indxPre[ind0 + 5];
        pi6 = xi + indxPre[ind0 + 6];
        pi7 = xi + indxPre[ind0 + 7];

        t0r = pi0->r + pi4->r;
        t0i = pi0->i + pi4->i;
        t1r = pi1->r + pi5->r;
        t1i = pi1->i + pi5->i;
        t2r = pi2->r + pi6->r;
        t2i = pi2->i + pi6->i;
        t3r = pi3->r + pi7->r;
        t3i = pi3->i + pi7->i;
        t5r = pi1->r - pi5->r;
        t5i = pi1->i - pi5->i;
        t7r = pi3->r - pi7->r;
        t7i = pi3->i - pi7->i;

        p0->r = t0r + t2r;
        p0->i = t0i + t2i;
        p0++;
        p0->r = t1r + t3r;
        p0->i = t1i + t3i;
        p0++;
        p0->r = t0r - t2r;
        p0->i = t0i - t2i;
        p0++;
        p0->r = t1r - t3r;
        p0->i = t1i - t3i;
        p0++;
        p0->r = pi0->r - pi4->r;
        p0->i = pi0->i - pi4->i;
        p0++;
        p0->r = t5r + t7r;
        p0->i = t5i + t7i;
        p0++;
        p0->r = pi2->r - pi6->r;
        p0->i = pi2->i - pi6->i;
        p0++;
        p0->r = t5r - t7r;
        p0->i = t5i - t7i;
        p0++;
    }

    for (n5 = 0; n5 < 120; n5 += 24) {
        for (ind0 = n5; ind0 < (n5 + 8); ind0++) {
            p0 = x + ind0;
            p1 = x + ind0 + 8;
            p2 = x + ind0 + 16;

            x1r = p1->r;
            x1i = p1->i;
            p1->r = x1r + p2->r;
            p1->i = x1i + p2->i;
            p2->r = x1r - p2->r;
            p2->i = x1i - p2->i;
            cadd(*p0, *p1, p0);
        }
    }

    for (n3 = 0; n3 < 24; n3 += 8) {
        for (ind0 = n3; ind0 < (n3 + 8); ind0++) {
            p0 = x + ind0;
            p1 = x + ind0 + 24;
            p2 = x + ind0 + 48;
            p3 = x + ind0 + 72;
            p4 = x + ind0 + 96;
            p5 = x + ind0 + 120;
            ps = fft120cnst + ind0;

            t3r = p3->r + p2->r;
            t3i = p3->i + p2->i;
            t2r = p3->r - p2->r;
            t2i = p3->i - p2->i;
            t1r = p1->r + p4->r;
            t1i = p1->i + p4->i;
            csub(*p1, *p4, p2);

            p1->r = t1r + t3r;
            p1->i = t1i + t3i;
            p3->r = t1r - t3r;
            p3->i = t1i - t3i;

            x5r = p2->r + t2r;
            x5i = p2->i + t2i;
            cadd(*p0, *p1, p0);

            cmpys(*p0, *ps, p0);
            ps += 24;
            cmpys(*p1, *ps, p1);
            ps += 24;
            cmpys(*p2, *ps, p2);
            ps += 24;
            cmpys(*p3, *ps, p3);
            ps += 24;
            p4->r = t2r * *ps;
            p4->i = t2i * *ps;
            ps += 24;
            p5->r = x5r * *ps;
            p5->i = x5i * *ps;
        }
    }

    for (n3 = 0; n3 < 24; n3 += 8) {
        for (ind0 = n3; ind0 < (n3 + 8); ind0++) {
            p0 = x + ind0;
            p1 = x + ind0 + 24;
            p2 = x + ind0 + 48;
            p3 = x + ind0 + 72;
            p4 = x + ind0 + 96;
            p5 = x + ind0 + 120;

            cmpyj(p2);
            cmpyj(p4);
            cmpyj(p5);

            q1r = p0->r + p1->r;
            q1i = p0->i + p1->i;

            t1r = q1r + p3->r;
            t1i = q1i + p3->i;
            t4r = p2->r + p5->r;
            t4i = p2->i + p5->i;
            t3r = q1r - p3->r;
            t3i = q1i - p3->i;
            t2r = p4->r + p5->r;
            t2i = p4->i + p5->i;

            p1->r = t1r + t4r;
            p1->i = t1i + t4i;
            p4->r = t1r - t4r;
            p4->i = t1i - t4i;
            p3->r = t3r + t2r;
            p3->i = t3i + t2i;
            p2->r = t3r - t2r;
            p2->i = t3i - t2i;
        }
    }

    for (n5 = 0; n5 < 120; n5 += 24) {
        for (ind0 = n5; ind0 < (n5 + 8); ind0++) {
            p0 = x + ind0;
            p1 = x + ind0 + 8;
            p2 = x + ind0 + 16;

            t1r = p0->r + p1->r;
            t1i = p0->i + p1->i;
            cmpyj(p2);

            p1->r = t1r + p2->r;
            p1->i = t1i + p2->i;
            p2->r = t1r - p2->r;
            p2->i = t1i - p2->i;
        }
    }

    for (ind0 = 0; ind0 < 120; ind0 += 8) {
        p0 = x + ind0;
        p1 = x + ind0 + 1;
        p2 = x + ind0 + 2;
        p3 = x + ind0 + 3;
        p4 = x + ind0 + 4;
        p5 = x + ind0 + 5;
        p6 = x + ind0 + 6;
        p7 = x + ind0 + 7;

        pi0 = xi + indxPost[ind0 + 0];
        pi1 = xi + indxPost[ind0 + 1];
        pi2 = xi + indxPost[ind0 + 2];
        pi3 = xi + indxPost[ind0 + 3];
        pi4 = xi + indxPost[ind0 + 4];
        pi5 = xi + indxPost[ind0 + 5];
        pi6 = xi + indxPost[ind0 + 6];
        pi7 = xi + indxPost[ind0 + 7];

        cmpyj(p3);
        cmpyj(p5);
        cmpyj(p6);

        t5r = p4->r + p5->r;
        t5i = p4->i + p5->i;
        t4r = p4->r - p5->r;
        t4i = p4->i - p5->i;
        t7r = p6->r + p7->r;
        t7i = p6->i + p7->i;
        t6r = p6->r - p7->r;
        t6i = p6->i - p7->i;

        cadd(*p0, *p1, pi0);
        csub(*p0, *p1, pi4);
        cadd(*p2, *p3, pi2);
        csub(*p2, *p3, pi6);
        pi1->r = t5r + t7r;
        pi1->i = t5i + t7i;
        pi5->r = t4r + t6r;
        pi5->i = t4i + t6i;
        pi3->r = t5r - t7r;
        pi3->i = t5i - t7i;
        pi7->r = t4r - t6r;
        pi7->i = t4i - t6i;
    }
}

/*--------------------------------------------------------------------------*/
/*  Winograd 480 points FFT                                                 */
/*--------------------------------------------------------------------------*/
/*  complex_t      *pI      (i)    input to the FFT                         */
/*  complex_t      *pO      (o)    output to the FFT                        */
/*--------------------------------------------------------------------------*/
static void wfft480(complex_t *pI, complex_t *pO) {
    short i, j;
    float temp;
    float x0r, x1r, x2r, x3r, t0r, t1r, t2r, t3r;
    float x0i, x1i, x2i, x3i, t0i, t1i, t2i, t3i;
    complex_t *pw1, *pw2, *pw3;
    complex_t *pt0, *pt1, *pt2, *pt3;

    pt0 = pI;
    pt1 = pI + 120;
    pt2 = pI + 240;
    pt3 = pI + 360;
    pw1 = ptwdf + 1;
    pw2 = ptwdf + 2;
    pw3 = ptwdf + 3;

    for (i = 0; i < 120; i++) {
        t0r = pt0->r + pt2->r;
        t0i = pt0->i + pt2->i;
        t2r = pt0->r - pt2->r;
        t2i = pt0->i - pt2->i;
        t1r = pt1->r + pt3->r;
        t1i = pt1->i + pt3->i;
        t3r = pt3->r - pt1->r;
        t3i = pt3->i - pt1->i;

        temp = t3r;
        t3r = -t3i;
        t3i = temp;

        x0r = t0r + t1r;
        x0i = t0i + t1i;
        x1r = t2r + t3r;
        x1i = t2i + t3i;
        x2r = t0r - t1r;
        x2i = t0i - t1i;
        x3r = t2r - t3r;
        x3i = t2i - t3i;

        // twiddle factors
        pt0->r = x0r;
        pt0->i = x0i;
        pt1->r = x1r * pw1->r - x1i * pw1->i;
        pt1->i = x1r * pw1->i + x1i * pw1->r;
        pt2->r = x2r * pw2->r - x2i * pw2->i;
        pt2->i = x2r * pw2->i + x2i * pw2->r;
        pt3->r = x3r * pw3->r - x3i * pw3->i;
        pt3->i = x3r * pw3->i + x3i * pw3->r;

        pt0++;
        pt1++;
        pt2++;
        pt3++;
        pw1 += 4;
        pw2 += 4;
        pw3 += 4;
    }
    for (i = 0; i < 480; i += 120) {
        wfft120(pI + i);
    }

    pt0 = pO;
    for (j = 0; j < 120; j++) {
        pt1 = pI + j;
        for (i = 0; i < 4; i++) {
            *pt0++ = *pt1;
            pt1 += 120;
        }
    }
}

/*--------------------------------------------------------------------------*/
/*  DCT4 960 points                                                         */
/*--------------------------------------------------------------------------*/
/*  float      v[]        (i)    input of the DCT4                          */
/*  float      coefs32[]  (o)    coefficient of the DCT4                    */
/*--------------------------------------------------------------------------*/
static void dct4_960(float v[MLT960_LENGTH], float coefs32[MLT960_LENGTH]) {
    short n;
    float f_int[MLT960_LENGTH];

    for (n = 0; n < MLT960_LENGTH; n += 2) {
        f_int[n] = ((v[n] * dct480_table_1[n >> 1].r - v[(MLT960_LENGTH_MINUS_1 - n)] * dct480_table_1[n >> 1].i));
        f_int[n + 1] = ((v[n] * dct480_table_1[n >> 1].i + v[(MLT960_LENGTH_MINUS_1 - n)] * dct480_table_1[n >> 1].r));
    }

    wfft480((complex_t *)f_int, (complex_t *)v);

    for (n = 0; n < MLT960_LENGTH; n += 2) {
        coefs32[n] = ((v[n] * dct480_table_2[n >> 1].r - v[n + 1] * dct480_table_2[n >> 1].i)) / 4.0f;
        coefs32[MLT960_LENGTH_MINUS_1 - n] = -(v[n] * dct480_table_2[n >> 1].i + v[n + 1] * dct480_table_2[n >> 1].r) / 4.0f;
    }
}

/*--------------------------------------------------------------------------*/
/*  DCT4 240 points                                                         */
/*--------------------------------------------------------------------------*/
/*  float      v[]        (i)    input of the DCT4                          */
/*  float      coefs32[]  (o)    coefficient of the DCT4                    */
/*--------------------------------------------------------------------------*/
static void dct4_240(float v[MLT240_LENGTH], float coefs32[MLT240_LENGTH]) {
    short n;
    float f_int[MLT240_LENGTH];

    for (n = 0; n < MLT240_LENGTH; n += 2) {
        f_int[n] = (v[n] * dct120_table_1[n >> 1].r - v[(MLT240_LENGTH_MINUS_1 - n)] * dct120_table_1[n >> 1].i);
        f_int[n + 1] = (v[n] * dct120_table_1[n >> 1].i + v[(MLT240_LENGTH_MINUS_1 - n)] * dct120_table_1[n >> 1].r);
    }

    wfft120((complex_t *)f_int);

    for (n = 0; n < MLT240_LENGTH; n += 2) {
        coefs32[n] = (f_int[n] * dct120_table_2[n >> 1].r - f_int[n + 1] * dct120_table_2[n >> 1].i) / 2.0f;
        coefs32[MLT240_LENGTH_MINUS_1 - n] = -(f_int[n] * dct120_table_2[n >> 1].i + f_int[n + 1] * dct120_table_2[n >> 1].r) / 2.0f;
    }
}

/*--------------------------------------------------------------------------*/
/*  Inverse time domain aliasing                                            */
/*--------------------------------------------------------------------------*/
/*  float     *in    (i)   input vector                                     */
/*  float     *out   (o)   output vector                                    */
/*--------------------------------------------------------------------------*/
static inline void itda(float *in, float *out) {
    short i;

    for (i = 0; i < MLT960_LENGTH_DIV_2; i++) {
        out[i] = in[MLT960_LENGTH_DIV_2 + i];
        out[MLT960_LENGTH_DIV_2 + i] = -in[MLT960_LENGTH - 1 - i];
        out[MLT960_LENGTH + i] = -in[MLT960_LENGTH_DIV_2 - 1 - i];
        out[3 * MLT960_LENGTH_DIV_2 + i] = -in[i];
    }
}

/*--------------------------------------------------------------------------*/
/*  Inverse MDCT for short frames                                           */
/*--------------------------------------------------------------------------*/
/*  float     *in    (i)   input vector                                     */
/*  float     *out   (o)   output vector                                    */
/*--------------------------------------------------------------------------*/
static void imdct_short(float *in, float *out) {
    float alias[MAX_SEGMENT_LENGTH]; // OG: arena alloc
    short i;

    dct4_240(in, alias);

    for (i = 0; i < MAX_SEGMENT_LENGTH / 4; i++) {
        out[i] = alias[MAX_SEGMENT_LENGTH / 4 + i];
        out[MAX_SEGMENT_LENGTH / 4 + i] = -alias[MAX_SEGMENT_LENGTH / 2 - 1 - i];
        out[MAX_SEGMENT_LENGTH / 2 + i] = -alias[MAX_SEGMENT_LENGTH / 4 - 1 - i];
        out[3 * MAX_SEGMENT_LENGTH / 4 + i] = -alias[i];
    }
}

/*--------------------------------------------------------------------------*/
/*  Inverse transform from the DCT domain to time domain                    */
/*--------------------------------------------------------------------------*/
/*  float     *in_mdct       (i)   input MDCT vector                        */
/*  float     *out           (o)   output vector                            */
/*  int       *is_transient  (o)   transient flag                           */
/*--------------------------------------------------------------------------*/
static void inverse_transform(float *in_mdct, float *out, int is_transient) {
    float out_alias[FRAME_LENGTH];   // OG: arena alloc
    float alias[MAX_SEGMENT_LENGTH]; // OG: arena alloc
    float *in_segment;
    float *out_segment;
    float tmp;

    short ta, seg;

    if (is_transient) {
        for (ta = 0; ta < FRAME_LENGTH; ta++) {
            out_alias[ta] = 0.0f;
        }

        // EXTRA: ref. inits out_segment outside out_alias, which is technically UB and compilers don't like,
        // but doesn't matter since accesses start at + MAX_SEGMENT_LENGTH / 4
        //out_segment = out_alias - MAX_SEGMENT_LENGTH / 4;
        out_segment = out_alias; // EXTRA
        in_segment = in_mdct + 0;

        imdct_short(in_segment, alias);

        for (ta = MAX_SEGMENT_LENGTH / 4; ta < MAX_SEGMENT_LENGTH / 2; ta++) {
            //out_segment[ta] = alias[ta];
            out_segment[ta - MAX_SEGMENT_LENGTH / 4] = alias[ta]; // EXTRA
        }

        for (ta = MAX_SEGMENT_LENGTH / 2; ta < MAX_SEGMENT_LENGTH; ta++) {
            //out_segment[ta] = alias[ta] * short_window[ta];
            out_segment[ta - MAX_SEGMENT_LENGTH / 4] = alias[ta] * short_window[ta]; // EXTRA
        }

        //out_segment = out_segment + MAX_SEGMENT_LENGTH / 2;
        out_segment = out_segment + MAX_SEGMENT_LENGTH / 4; // EXTRA
        in_segment = in_segment + MAX_SEGMENT_LENGTH / 2;

        for (seg = 1; seg < NUM_TIME_SWITCHING_BLOCKS - 1; seg++) {
            imdct_short(in_segment, alias);

            for (ta = 0; ta < MAX_SEGMENT_LENGTH; ta++) {
                out_segment[ta] = out_segment[ta] + alias[ta] * short_window[ta];
            }

            in_segment = in_segment + MAX_SEGMENT_LENGTH / 2;
            out_segment = out_segment + MAX_SEGMENT_LENGTH / 2;
        }

        imdct_short(in_segment, alias);

        for (ta = 0; ta < MAX_SEGMENT_LENGTH / 2; ta++) {
            out_segment[ta] = out_segment[ta] + alias[ta] * short_window[ta];
        }

        for (ta = MAX_SEGMENT_LENGTH / 2;
             ta < MAX_SEGMENT_LENGTH / 4 + MAX_SEGMENT_LENGTH / 2; ta++) {
            out_segment[ta] = alias[ta];
        }

        for (ta = 0; ta < FRAME_LENGTH / 2; ta++) {
            tmp = out_alias[ta];
            out_alias[ta] = out_alias[FRAME_LENGTH - 1 - ta];
            out_alias[FRAME_LENGTH - 1 - ta] = tmp;
        }
    }
    else {
        dct4_960(in_mdct, out_alias);
    }

    // OG: inlined
    itda(out_alias, out);
}

/*--------------------------------------------------------------------------*/
/*  Windowing, Overlap and Add                                              */
/*--------------------------------------------------------------------------*/
/*  float     ImdctOut[]  (i)   input                                       */
/*  short     auOut[]     (o)   output audio                                */
/*  float     OldauOut[]  (i/o) audio from previous frame                   */
/*--------------------------------------------------------------------------*/
static void window_ola(float ImdctOut[], short auOut[], float OldauOut[]) {
    short i;
    float OutputVal;
    float ImdctOutWin[2 * FRAME_LENGTH]; // OG: arena alloc

    for (i = 0; i < 2 * FRAME_LENGTH; i++) {
        ImdctOutWin[i] = ImdctOut[i] * window[i];
    }

    for (i = 0; i < FRAME_LENGTH; i++) {
        OutputVal = ImdctOutWin[i] + OldauOut[i];
        if (OutputVal > 32767.0f) {
            OutputVal = 32767.0f;
        }
        else if (OutputVal < -32768.0f) {
            OutputVal = -32768.0f;
        }
        auOut[i] = (short)OutputVal;
    }

    for (i = 0; i < FRAME_LENGTH; i++) {
        OldauOut[i] = ImdctOutWin[i + FRAME_LENGTH];
    }
}

/*--------------------------------------------------------------------------*/
/*  Deinterleave the spectrum                                               */
/*--------------------------------------------------------------------------*/
/*  float     *coefs  (i/o)   input and output coefficients                 */
/*--------------------------------------------------------------------------*/
static void de_interleave_spectrum(float *coefs) {
    short i, j;
    float coefs_short[STOP_BAND]; // OG: arena alloc
    float *p1a, *p1b, *p2a, *p2b, *p3a, *p3b, *p3c, *p4a, *p4b;
    float *pcoefs, *pcoefs1, *pcoefs2;

    p1a = coefs;
    p1b = coefs + 64;
    p2a = coefs + NUMC_G1;
    p2b = coefs + NUMC_G1 + 64;
    p3a = coefs + NUMC_G23;
    p3b = coefs + NUMC_G23 + 96;
    p3c = coefs + NUMC_G23 + 192;
    p4a = coefs + NUMC_N;
    p4b = coefs + NUMC_N + 128;
    for (i = 0; i < STOP_BAND; i += STOP_BAND4) {
        pcoefs = coefs_short + i;
        pcoefs1 = coefs_short + 16 + i;
        for (j = 0; j < 16; j++) {
            *pcoefs++ = *p1a++;
            *pcoefs1++ = *p1b++;
        }

        pcoefs = coefs_short + NUMC_G1SUB + i;
        pcoefs1 = coefs_short + NUMC_G1SUB + 16 + i;
        for (j = 0; j < 16; j++) {
            *pcoefs++ = *p2a++;
            *pcoefs1++ = *p2b++;
        }

        pcoefs = coefs_short + NUMC_G1G2SUB + i;
        pcoefs1 = coefs_short + NUMC_G1G2SUB + WID_G3 + i;
        pcoefs2 = coefs_short + NUMC_G1G2SUB + 2 * WID_G3 + i;
        for (j = 0; j < WID_G3; j++) {
            *pcoefs++ = *p3a++;
            *pcoefs1++ = *p3b++;
            *pcoefs2++ = *p3c++;
        }

        pcoefs = coefs_short + NUMC_G1G2G3SUB + i;
        pcoefs1 = coefs_short + NUMC_G1G2G3SUB + WID_GX + i;
        for (j = 0; j < WID_GX; j++) {
            *pcoefs++ = *p4a++;
            *pcoefs1++ = *p4b++;
        }
    }

    /* unpack the spectrum */
    p1a = coefs_short;
    pcoefs = coefs;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < STOP_BAND4; j++) {
            pcoefs[j] = p1a[j];
        }
        for (j = STOP_BAND4; j < FRAME_LENGTH / 4; j++) {
            pcoefs[j] = 0;
        }
        p1a += STOP_BAND4;
        pcoefs += FRAME_LENGTH / 4;
    }
}

/*--------------------------------------------------------------------------*/
/*  Fill the spectrum which has been quantized with 0 bits                  */
/*--------------------------------------------------------------------------*/
/*  float     *coeff        (i)   normalized MLT coefficients               */
/*  float     *coeff_out    (o)   MLT coefficients                          */
/*  short     *R            (i)   number of bits per coefficient            */
/*  short     is_transient  (i)   transient flag                            */
/*  short     norm          (i)   quantization indices for norms            */
/*  short     nf_idx        (i)   noise fill index                          */
/*--------------------------------------------------------------------------*/
static void fill_spectrum(float *coeff, float *coeff_out, short *R, short is_transient, short norm[], short nf_idx) {
    float CodeBook[FREQ_LENGTH]; // OG: arena alloc
    float *src, *dst, *end;
    float normq;
    short cb_size, cb_pos;
    short sfm, j, i;

    short last_sfm;
    short first_coeff;
    short low_coeff;

    /* Build codebook */
    cb_size = 0;
    for (sfm = 0; sfm < NB_SFM; sfm++) {
        if (R[sfm] != 0) {
            for (j = sfm_start[sfm]; j < sfm_end[sfm]; j++) {
                CodeBook[cb_size] = coeff[j];
                cb_size++;
            }
        }
    }

    /* detect last sfm */
    last_sfm = NB_SFM - 1;
    if (is_transient == 0) {
        for (sfm = NB_SFM - 1; sfm >= 0; sfm--) {
            if (R[sfm] != 0) {
                last_sfm = sfm;
                break;
            }
        }
    }

    if (cb_size != 0) {
        /* Read from codebook */
        cb_pos = 0;
        for (sfm = 0; sfm <= last_sfm; sfm++) {
            if (R[sfm] == 0) {
                for (j = sfm_start[sfm]; j < sfm_end[sfm]; j++) {
                    coeff[j] = CodeBook[cb_pos];
                    cb_pos++;
                    cb_pos = (cb_pos >= cb_size) ? 0 : cb_pos;
                }
            }
        }

        if (is_transient == 0) {
            low_coeff = sfm_end[last_sfm] >> 1;
            src = coeff + sfm_end[last_sfm] - 1;

            first_coeff = sfm_end[last_sfm];
            dst = coeff + sfm_end[last_sfm];
            end = coeff + sfm_end[NB_SFM - 1];

            while (dst < end) {
                while (dst < end && src >= &coeff[low_coeff]) {
                    *dst++ = *src--;
                }

                src++;

                while (dst < end && src < &coeff[first_coeff]) {
                    *dst++ = *src++;
                }
            }
        }

        for (sfm = 0; sfm <= last_sfm; sfm++) {
            if (R[sfm] == 0) {
                for (j = sfm_start[sfm]; j < sfm_end[sfm]; j++) {
                    /* Scale NoiseFill */
                    coeff[j] = (float)(coeff[j] / pow(2, nf_idx)); // OG: Namco inlines pow
                }
            }
        }
    }

    /*  shape the spectrum */
    for (sfm = 0; sfm < NB_SFM; sfm++) {
        normq = dicn[norm[sfm]];
        for (i = sfm_start[sfm]; i < sfm_end[sfm]; i++) {
            coeff_out[i] = coeff[i] * normq;
        }
    }
}

//-------------------------------------------------------------------------------------------------
// DECODER

/*--------------------------------------------------------------------------*/
/*  Reset decoder tables                                                    */
/*--------------------------------------------------------------------------*/
/*  decoderState  *d          (i)   state of decoder                        */
/*  short         num_bits    (i)   number of bits                          */
/*--------------------------------------------------------------------------*/
static void decoder_reset_tables(DecoderState *d, short num_bits) {
    d->num_bits_spectrum_stationary = num_bits - 3;
    d->num_bits_spectrum_transient = num_bits - 1;
    d->num_bits = num_bits;
}

/*--------------------------------------------------------------------------*/
/*  Initialize the state of the decoder                                     */
/*--------------------------------------------------------------------------*/
/*  DecoderState  *d          (i)   state of decoder                        */
/*  short         num_bits    (i)   number of bits                          */
/*--------------------------------------------------------------------------*/
void decoder_init(DecoderState *d, short num_bits) {
    short i;

    for (i = 0; i < FRAME_LENGTH; i++) {
        d->old_out[i] = 0.0f;
    }

    d->old_is_transient = 0;

    decoder_reset_tables(d, num_bits);
}

/*--------------------------------------------------------------------------*/
/*  Decodes a single frame                                                  */
/*--------------------------------------------------------------------------*/
/*  short         bitstream[]  (i)    bitstream to decode                   */
/*  short         bfi          (i)    bad frame indicator                   */
/*  short         out16[]      (o)    decoded audio                         */
/*  DecoderState  *d           (i/o)  state of decoder                      */
/*--------------------------------------------------------------------------*/
void decode_frame(unsigned char* src, int src_size, short bfi, short out16[], DecoderState *d) {
    short is_transient;

    // OG: arena allocs seems done as needed rather than as declared
    float t_audio_q[FRAME_LENGTH];      // OG: arena alloc
    float wtda_audio[2 * FRAME_LENGTH]; // OG: arena alloc
    short bitalloc[NB_SFM];             // OG: arena alloc
    short ynrm[NB_SFM];                 // OG: arena alloc
    short i;
    float audio_q_norm[FREQ_LENGTH];    // OG: arena alloc
    short nf_idx;
    bitstream_t is_tmp;
    bitstream_t* is = &is_tmp;


    // OG: is22's bitreader new (malloc + init) or global (arena?) init
    bg_init(is, src, src_size);

    if (bfi) {
        // OG: removed BFI handling path but old_coefs/old_is_transient is still updated, maybe compiler optimization?
        for (i = 0; i < FRAME_LENGTH; i++) {
            t_audio_q[i] = d->old_coeffs[i];
            d->old_coeffs[i] = d->old_coeffs[i] / 2;
        }
        is_transient = d->old_is_transient;
    }
    else {
        // OG: new bitstream call (ref. reads bit + advances buf manually)
        if (bg_read1(is) == 1) {
            is_transient = 1;
        }
        else {
            is_transient = 0;
        }

        if (is_transient) {
            flvqdec(is, t_audio_q, audio_q_norm, bitalloc, d->num_bits_spectrum_transient, ynrm, is_transient);
            nf_idx = 0;
        }
        else {
            flvqdec(is, t_audio_q, audio_q_norm, bitalloc, d->num_bits_spectrum_stationary, ynrm, is_transient);
            bits2idxn(is, 2, &nf_idx); // OG also inlines it here
            bg_skip(is, 2);
        }

        for (i = FREQ_LENGTH; i < FRAME_LENGTH; i++) {
            t_audio_q[i] = 0.0f;
        }

        fill_spectrum(audio_q_norm, t_audio_q, bitalloc, is_transient, ynrm, nf_idx);

        if (is_transient) {
            de_interleave_spectrum(t_audio_q);
        }

        for (i = 0; i < FRAME_LENGTH; i++) {
            d->old_coeffs[i] = t_audio_q[i];
        }
        d->old_is_transient = is_transient;
    }

    inverse_transform(t_audio_q, wtda_audio, is_transient);
    window_ola(wtda_audio, out16, d->old_out);

    // OG: arena allocator free
    // OG: bitreader free

    // maybe should control bad reads, but not enforced in OG/ref.
    //if (is->error)
    //    return -1;
}
