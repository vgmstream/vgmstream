/* Handles file ops like parsing header and reading blocks.
 */
#ifndef _MIO_ERISA_MATRIX_H_
#define _MIO_ERISA_MATRIX_H_

#define MIN_DCT_DEGREE 2
#define MAX_DCT_DEGREE 12

typedef struct {
    float rSin;
    float rCos;
}  ERI_SIN_COS;


void EMT_eriInitializeMatrix(void);

void EMT_eriRoundR32ToWordArray(SWORD* ptrDst, int nStep, const float* ptrSrc, int nCount);

void EMT_eriRevolve2x2(float* ptrBuf1, float* ptrBuf2, float rSin, float rCos, unsigned int nStep, unsigned int nCount);

ERI_SIN_COS* EMT_eriCreateRevolveParameter(unsigned int nDegreeDCT);

void EMT_eriOddGivensInverseMatrix(float* ptrSrc, const ERI_SIN_COS* ptrRevolve, unsigned int nDegreeDCT);

void EMT_eriFastIPLOT(float* ptrSrc, unsigned int nDegreeDCT);
void EMT_eriFastILOT(float* ptrDst, const float* ptrSrc1, const float* ptrSrc2, unsigned int nDegreeDCT);

void EMT_eriFastIDCT(float* ptrDst, float* ptrSrc, unsigned int nSrcInterval, float* ptrWorkBuf, unsigned int nDegreeDCT);

#endif
