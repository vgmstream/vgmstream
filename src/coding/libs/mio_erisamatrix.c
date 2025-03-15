/*****************************************************************************
                         E R I S A - L i b r a r y
 ----------------------------------------------------------------------------
         Copyright (C) 2000-2003 Leshade Entis. All rights reserved.
 *****************************************************************************/

#include <stdlib.h>
#include <math.h>
#include "mio_xerisa.h"

static volatile int g_eri_initialized = 0; //extra, not really that important but..

void ERI_eriInitializeLibrary(void) {
    if (g_eri_initialized)
        return;
    EMT_eriInitializeMatrix();
    g_eri_initialized = 1;
}

void ERI_eriCloseLibrary(void) {
}

//

static int round_f32(float r) {
    if (r >= 0.0) {
        return (int)floor(r + 0.5);
    }
    else {
        return (int)ceil(r - 0.5);
    }
}

void EMT_eriRoundR32ToWordArray(SWORD* ptrDst, int nStep, const float* ptrSrc, int nCount) {
    for (int i = 0; i < nCount; i++) {
        int nValue = round_f32(ptrSrc[i]);
        if (nValue <= -0x8000) {
            *ptrDst = -0x8000;
        }
        else if (nValue >= 0x7FFF) {
            *ptrDst = 0x7FFF;
        }
        else {
            *ptrDst = (SWORD)nValue;
        }
        ptrDst += nStep;
    }
}


static const double ERI_PI = 3.141592653589;  // = π
static const float ERI_rHalf = 0.5F;          // = 1/2
//static const float ERI_r2 = 2.0F;           // = 2.0

// revolve 2-point matrix (for MSS)
void EMT_eriRevolve2x2(float* ptrBuf1, float* ptrBuf2, float rSin, float rCos, unsigned int nStep, unsigned int nCount) {
    for (int i = 0; i < nCount; i++) {
        float r1 = *ptrBuf1;
        float r2 = *ptrBuf2;

        *ptrBuf1 = r1 * rCos - r2 * rSin;
        *ptrBuf2 = r1 * rSin + r2 * rCos;

        ptrBuf1 += nStep;
        ptrBuf2 += nStep;
    }
}

// create rotation matrix parameter for LOT transformation
ERI_SIN_COS* EMT_eriCreateRevolveParameter(unsigned int nDegreeDCT) {
    int i, nDegreeNum;
    nDegreeNum = 1 << nDegreeDCT;

    int lc = 1, n = nDegreeNum / 2;
    while (n >= 8) {
        n /= 8;
        ++lc;
    }

    ERI_SIN_COS* ptrRevolve = malloc(lc * 8 * sizeof(ERI_SIN_COS));
    if (!ptrRevolve)
        return NULL;

    double k = ERI_PI / (nDegreeNum * 2);
    ERI_SIN_COS* ptrNextRev = ptrRevolve;
    int nStep = 2;
    do {
        for (i = 0; i < 7; i++) {
            double ws = 1.0;
            double a = 0.0;
            for (int j = 0; j < i; j++) {
                a += nStep;
                ws = ws * ptrNextRev[j].rSin + ptrNextRev[j].rCos * cos(a * k);
            }
            double r = atan2(ws, cos((a + nStep) * k));
            ptrNextRev[i].rSin = (float)sin(r);
            ptrNextRev[i].rCos = (float)cos(r);
        }
        ptrNextRev += 7;
        nStep *= 8;
    }
    while (nStep < nDegreeNum);

    return ptrRevolve;
}

void EMT_eriOddGivensInverseMatrix(float* ptrSrc, const ERI_SIN_COS* ptrRevolve, unsigned int nDegreeDCT) {
    int i, j, k;
    int nDegreeNum = 1 << nDegreeDCT;

    // odd number rotate operation
    int nStep, lc, index;
    index = 1;
    nStep = 2;
    lc = (nDegreeNum / 2) / 8;

    for (;;) {
        ptrRevolve += 7;
        index += nStep * 7;
        nStep *= 8;
        if (lc <= 8)
            break;
        lc /= 8;
    }

    k = index + nStep * (lc - 2);
    for (j = lc - 2; j >= 0; j--) {
        float r1 = ptrSrc[k];
        float r2 = ptrSrc[k + nStep];
        ptrSrc[k] = r1 * ptrRevolve[j].rCos + r2 * ptrRevolve[j].rSin;
        ptrSrc[k + nStep] = r2 * ptrRevolve[j].rCos - r1 * ptrRevolve[j].rSin;
        k -= nStep;
    }

    for (;;) {
        if (lc > (nDegreeNum / 2) / 8)
            break;

        ptrRevolve -= 7;
        nStep /= 8;
        index -= nStep * 7;

        for (i = 0; i < lc; i++) {
            k = i * (nStep * 8) + index + nStep * 6;
            for (j = 6; j >= 0; j--) {
                float r1 = ptrSrc[k];
                float r2 = ptrSrc[k + nStep];
                ptrSrc[k] = r1 * ptrRevolve[j].rCos + r2 * ptrRevolve[j].rSin;
                ptrSrc[k + nStep] = r2 * ptrRevolve[j].rCos - r1 * ptrRevolve[j].rSin;
                k -= nStep;
            }
        }

        lc *= 8;
    }
}

// Inverse Previous LOT
void EMT_eriFastIPLOT(float* ptrSrc, unsigned int nDegreeDCT) {
    unsigned int nDegreeNum = 1 << nDegreeDCT;

    // divide odd and even freqs
    for (int i = 0; i < nDegreeNum; i += 2) {
        float r1 = ptrSrc[i];
        float r2 = ptrSrc[i + 1];
        ptrSrc[i]     = ERI_rHalf * (r1 + r2);
        ptrSrc[i + 1] = ERI_rHalf * (r1 - r2);
    }
}

// Inverse LOT
void EMT_eriFastILOT(float* ptrDst, const float* ptrSrc1, const float* ptrSrc2, unsigned int nDegreeDCT) {
    unsigned int nDegreeNum = 1 << nDegreeDCT;

    // reverse duplication
    for (int i = 0; i < nDegreeNum; i += 2) {
        float r1 = ptrSrc1[i];
        float r2 = ptrSrc2[i + 1];
        ptrDst[i]     = r1 + r2;
        ptrDst[i + 1] = r1 - r2;
    }
}


//////////////////////////////////////////////////////////////////////////////

static float ERI_rCosPI4;                     // = cos(pi/4)
static float ERI_r2CosPI4;                    // = 2*cos(pi/4)

// matrix coefs: k(n,i) = cos( (2*i+1) / (4*n) )
//
static float ERI_DCTofK2[2];        // = cos( (2*i+1) / 8 )
static float ERI_DCTofK4[4];        // = cos( (2*i+1) / 16 )
static float ERI_DCTofK8[8];        // = cos( (2*i+1) / 32 )
static float ERI_DCTofK16[16];      // = cos( (2*i+1) / 64 )
static float ERI_DCTofK32[32];      // = cos( (2*i+1) / 128 )
static float ERI_DCTofK64[64];      // = cos( (2*i+1) / 256 )
static float ERI_DCTofK128[128];    // = cos( (2*i+1) / 512 )
static float ERI_DCTofK256[256];    // = cos( (2*i+1) / 1024 )
static float ERI_DCTofK512[512];    // = cos( (2*i+1) / 2048 )
static float ERI_DCTofK1024[1024];  // = cos( (2*i+1) / 4096 )
static float ERI_DCTofK2048[2048];  // = cos( (2*i+1) / 8192 )

// matrix coefs table
static float* ERI_pMatrixDCTofK[MAX_DCT_DEGREE] = {
    NULL,
    ERI_DCTofK2,
    ERI_DCTofK4,
    ERI_DCTofK8,
    ERI_DCTofK16,
    ERI_DCTofK32,
    ERI_DCTofK64,
    ERI_DCTofK128,
    ERI_DCTofK256,
    ERI_DCTofK512,
    ERI_DCTofK1024,
    ERI_DCTofK2048
};

// initialize tables for DCT matrix operations
void EMT_eriInitializeMatrix(void) {
    // prepare special constants
    ERI_rCosPI4 = (float)cos(ERI_PI * 0.25);
    ERI_r2CosPI4 = 2.0F * ERI_rCosPI4;

    // matrix coefs init
    for (int i = 1; i < MAX_DCT_DEGREE; i++) {
        int n = (1 << i);
        float* pDCTofK = ERI_pMatrixDCTofK[i];
        double nr = ERI_PI / (4.0 * n);
        double dr = nr + nr;
        double ir = nr;

        for (int j = 0; j < n; j++) {
            pDCTofK[j] = (float)cos(ir);
            ir += dr;
        }
    }
}

static void eriFastDCT(float* ptrDst, unsigned int nDstInterval, float* ptrSrc, float* ptrWorkBuf, unsigned int nDegreeDCT) {
    if (nDegreeDCT < MIN_DCT_DEGREE || nDegreeDCT > MAX_DCT_DEGREE)
        return;

    if (nDegreeDCT == MIN_DCT_DEGREE) {
        // 4th order DCT
        float r32Buf[4];

        // cross ops
        r32Buf[0] = ptrSrc[0] + ptrSrc[3];
        r32Buf[2] = ptrSrc[0] - ptrSrc[3];
        r32Buf[1] = ptrSrc[1] + ptrSrc[2];
        r32Buf[3] = ptrSrc[1] - ptrSrc[2];

        // first half: A2 * DCT2
        ptrDst[0] = ERI_rHalf * (r32Buf[0] + r32Buf[1]);
        ptrDst[nDstInterval * 2] = ERI_rCosPI4 * (r32Buf[0] - r32Buf[1]);

        // last half: R2 * 2 * A2 * DCT2 * K2
        r32Buf[2] = ERI_DCTofK2[0] * r32Buf[2];
        r32Buf[3] = ERI_DCTofK2[1] * r32Buf[3];

        r32Buf[0] = r32Buf[2] + r32Buf[3];
        r32Buf[1] = ERI_r2CosPI4 * (r32Buf[2] - r32Buf[3]);

        r32Buf[1] -= r32Buf[0];

        ptrDst[nDstInterval] = r32Buf[0];
        ptrDst[nDstInterval * 3] = r32Buf[1];
    }
    else {
        // regular DCT
        //////////////////////////////////////////////////////////////////////
        //              | I   J |
        // cross ops  = |       |
        //              | I  -J |
        unsigned int nDegreeNum = (1 << nDegreeDCT);
        unsigned int nHalfDegree = (nDegreeNum >> 1);
        for (int i = 0; i < nHalfDegree; i++) {
            ptrWorkBuf[i] = ptrSrc[i] + ptrSrc[nDegreeNum - i - 1];
            ptrWorkBuf[i + nHalfDegree] = ptrSrc[i] - ptrSrc[nDegreeNum - i - 1];
        }

        // first half DCT : A * DCT
        unsigned int nDstStep = (nDstInterval << 1);
        eriFastDCT(ptrDst, nDstStep, ptrWorkBuf, ptrSrc, (nDegreeDCT - 1));

        // last half DCT-IV : R * 2 * A * DCT * K
        float* pDCTofK = ERI_pMatrixDCTofK[nDegreeDCT - 1];
        ptrSrc = ptrWorkBuf + nHalfDegree;
        ptrDst += nDstInterval;

        for (int i = 0; i < nHalfDegree; i++) {
            ptrSrc[i] *= pDCTofK[i];
        }

        eriFastDCT(ptrDst, nDstStep, ptrSrc, ptrWorkBuf, (nDegreeDCT - 1));

        float* ptrNext = ptrDst;
        for (int i = 0; i < nHalfDegree; i++) {
            *ptrNext += *ptrNext;
            ptrNext += nDstStep;
        }

        ptrNext = ptrDst;
        for (int i = 1; i < nHalfDegree; i++) {
            ptrNext[nDstStep] -= *ptrNext;
            ptrNext += nDstStep;
        }
    }
}

void EMT_eriFastIDCT(float* ptrDst, float* ptrSrc, unsigned int nSrcInterval, float* ptrWorkBuf, unsigned int nDegreeDCT) {
    if (nDegreeDCT < MIN_DCT_DEGREE || nDegreeDCT > MAX_DCT_DEGREE)
        return;

    if (nDegreeDCT == MIN_DCT_DEGREE) {
        // 4th order IDCT
        float r32Buf1[2];
        float r32Buf2[4];

        // even rows: IDCT2
        r32Buf1[0] = ptrSrc[0];
        r32Buf1[1] = ERI_rCosPI4 * ptrSrc[nSrcInterval * 2];

        r32Buf2[0] = r32Buf1[0] + r32Buf1[1];
        r32Buf2[1] = r32Buf1[0] - r32Buf1[1];

        // odd rows: R * 2 * A * DCT * K
        r32Buf1[0] = ERI_DCTofK2[0] * ptrSrc[nSrcInterval];
        r32Buf1[1] = ERI_DCTofK2[1] * ptrSrc[nSrcInterval * 3];

        r32Buf2[2] = r32Buf1[0] + r32Buf1[1];
        r32Buf2[3] = ERI_r2CosPI4 * (r32Buf1[0] - r32Buf1[1]);

        r32Buf2[3] -= r32Buf2[2];

        // cross ops
        ptrDst[0] = r32Buf2[0] + r32Buf2[2];
        ptrDst[3] = r32Buf2[0] - r32Buf2[2];
        ptrDst[1] = r32Buf2[1] + r32Buf2[3];
        ptrDst[2] = r32Buf2[1] - r32Buf2[3];
    }
    else {
        // regular IDCT
        //////////////////////////////////////////////////////////////////////

        // even rows: IDCT
        unsigned int nDegreeNum = (1 << nDegreeDCT);
        unsigned int nHalfDegree = (nDegreeNum >> 1);
        unsigned int nSrcStep = (nSrcInterval << 1);
        EMT_eriFastIDCT(ptrDst, ptrSrc, nSrcStep, ptrWorkBuf, (nDegreeDCT - 1));

        // odd rows: R * 2 * A * DCT * K
        float* pDCTofK = ERI_pMatrixDCTofK[nDegreeDCT - 1];
        float* pOddSrc = ptrSrc + nSrcInterval;
        float* pOddDst = ptrDst + nHalfDegree;

        float* ptrNext = pOddSrc;
        for (int i = 0; i < nHalfDegree; i++) {
            ptrWorkBuf[i] = *ptrNext * pDCTofK[i];
            ptrNext += nSrcStep;
        }

        eriFastDCT(pOddDst, 1, ptrWorkBuf, (ptrWorkBuf + nHalfDegree), (nDegreeDCT - 1));

        for (int i = 0; i < nHalfDegree; i++) {
            pOddDst[i] += pOddDst[i];
        }

        for (int i = 1; i < nHalfDegree; i++) {
            pOddDst[i] -= pOddDst[i - 1];
        }

        //             | I   I |
        // cross ops = |       |
        //             | J  -J |
        float r32Buf[4];
        unsigned int nQuadDegree = (nHalfDegree >> 1);
        for (int i = 0; i < nQuadDegree; i++) {
            r32Buf[0] = ptrDst[i] + ptrDst[nHalfDegree + i];
            r32Buf[3] = ptrDst[i] - ptrDst[nHalfDegree + i];
            r32Buf[1] = ptrDst[nHalfDegree - 1 - i] + ptrDst[nDegreeNum - 1 - i];
            r32Buf[2] = ptrDst[nHalfDegree - 1 - i] - ptrDst[nDegreeNum - 1 - i];

            ptrDst[i] = r32Buf[0];
            ptrDst[nHalfDegree - 1 - i] = r32Buf[1];
            ptrDst[nHalfDegree + i] = r32Buf[2];
            ptrDst[nDegreeNum - 1 - i] = r32Buf[3];
        }
    }
}
