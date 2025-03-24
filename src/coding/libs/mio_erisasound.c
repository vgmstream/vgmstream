/*****************************************************************************
                         E R I S A - L i b r a r y
 -----------------------------------------------------------------------------
    Copyright (C) 2002-2003 Leshade Entis, Entis-soft. All rights reserved.
 *****************************************************************************/

#include <stdlib.h>
#include <math.h>
#include "mio_xerisa.h"

static const double ERI_PI = 3.141592653589;
#define MIO_MAX_CHANNELS 2


/*******************/
/* audio converter */
/*******************/

MIODecoder* MIODecoder_Open() {
    MIODecoder* dec = calloc(1, sizeof(MIODecoder));
    if (!dec)
        return NULL;

    dec->m_nBufLength = 0;
    dec->m_ptrBuffer1 = NULL;
    dec->m_ptrBuffer2 = NULL;
    dec->m_ptrBuffer3 = NULL;
    dec->m_ptrDivisionTable = NULL;
    dec->m_ptrRevolveCode = NULL;
    dec->m_ptrWeightCode = NULL;
    dec->m_ptrCoefficient = NULL;
    dec->m_ptrMatrixBuf = NULL;
    dec->m_ptrInternalBuf = NULL;
    dec->m_ptrWorkBuf = NULL;
    dec->m_ptrWeightTable = NULL;
    dec->m_ptrLastDCT = NULL;
    dec->m_pRevolveParam = NULL;
    return dec;
}

static void MIODecoder_Delete(MIODecoder* dec) {
    free(dec->m_ptrBuffer1);
    dec->m_ptrBuffer1 = NULL;
    free(dec->m_ptrBuffer2);
    dec->m_ptrBuffer2 = NULL;
    free(dec->m_ptrBuffer3);
    dec->m_ptrBuffer3 = NULL;
    free(dec->m_ptrDivisionTable);
    dec->m_ptrDivisionTable = NULL;
    free(dec->m_ptrRevolveCode);
    dec->m_ptrRevolveCode = NULL;
    free(dec->m_ptrWeightCode);
    dec->m_ptrWeightCode = NULL;
    free(dec->m_ptrCoefficient);
    dec->m_ptrCoefficient = NULL;
    free(dec->m_ptrMatrixBuf);
    dec->m_ptrMatrixBuf = NULL;
    free(dec->m_ptrInternalBuf);
    dec->m_ptrInternalBuf = NULL;
    free(dec->m_ptrWorkBuf);
    dec->m_ptrWorkBuf = NULL;
    free(dec->m_ptrWeightTable);
    dec->m_ptrWeightTable = NULL;
    free(dec->m_ptrLastDCT);
    dec->m_ptrLastDCT = NULL;
    free(dec->m_pRevolveParam);
    dec->m_pRevolveParam = NULL;

    dec->m_nBufLength = 0;
}

void MIODecoder_Close(MIODecoder* dec) {
    if (!dec)
        return;
    MIODecoder_Delete(dec);
    free(dec);
}

// recalculate matrix size when params change
static int MIODecoder_InitializeWithDegree(MIODecoder* dec, unsigned int nSubbandDegree) {
    free(dec->m_pRevolveParam);
    dec->m_pRevolveParam = EMT_eriCreateRevolveParameter(nSubbandDegree);
    if (!dec->m_pRevolveParam) return eslErrGeneral;

    // params for inverse quantization
    static const int freq_width[7] = { -6, -6, -5, -4, -3, -2, -1 };
    for (int i = 0, j = 0; i < 7; i++) {
        int nFrequencyWidth = 1 << (nSubbandDegree + freq_width[i]);
        dec->m_nFrequencyPoint[i] = j + (nFrequencyWidth / 2);
        j += nFrequencyWidth;
    }

    dec->m_nSubbandDegree = nSubbandDegree;
    dec->m_nDegreeNum = (1 << nSubbandDegree);

    return eslErrSuccess;
}

ESLError MIODecoder_Initialize(MIODecoder* dec, const MIO_INFO_HEADER* infhdr) {
    MIODecoder_Delete(dec);

    dec->m_mioih = *infhdr; /* copy */

    if (dec->m_mioih.fdwTransformation == CVTYPE_LOSSLESS_ERI) {
        if (dec->m_mioih.dwArchitecture != ERI_RUNLENGTH_HUFFMAN)
            return eslErrGeneral;

        if ((dec->m_mioih.dwChannelCount != 1) && (dec->m_mioih.dwChannelCount != 2))
            return eslErrGeneral;

        if ((dec->m_mioih.dwBitsPerSample != 8) && (dec->m_mioih.dwBitsPerSample != 16))
            return eslErrGeneral;
    }
    else if ((dec->m_mioih.fdwTransformation == CVTYPE_LOT_ERI) || (dec->m_mioih.fdwTransformation == CVTYPE_LOT_ERI_MSS)) {

        if ((dec->m_mioih.dwArchitecture != ERI_RUNLENGTH_GAMMA) && 
            (dec->m_mioih.dwArchitecture != ERI_RUNLENGTH_HUFFMAN) &&
            (dec->m_mioih.dwArchitecture != ERISA_NEMESIS_CODE))
            return eslErrGeneral; /* unknown code format */

        if ((dec->m_mioih.dwChannelCount != 1) && (dec->m_mioih.dwChannelCount != 2))
            return eslErrGeneral;

        if (dec->m_mioih.dwBitsPerSample != 16)
            return eslErrGeneral;

        if ((dec->m_mioih.dwSubbandDegree < 8) || (dec->m_mioih.dwSubbandDegree > MAX_DCT_DEGREE))
            return eslErrGeneral;

        if (dec->m_mioih.dwLappedDegree != 1)
            return eslErrGeneral;

        // DCT buffers
        {
            int channel_size = sizeof(float) << dec->m_mioih.dwSubbandDegree;
            int buf_size = dec->m_mioih.dwChannelCount * channel_size;
            dec->m_ptrBuffer1 = malloc(buf_size);
            dec->m_ptrMatrixBuf = malloc(buf_size);
            dec->m_ptrInternalBuf = malloc(buf_size);
            dec->m_ptrWorkBuf = malloc(channel_size);
            if (!dec->m_ptrBuffer1 || !dec->m_ptrMatrixBuf || !dec->m_ptrInternalBuf || !dec->m_ptrWorkBuf)
                return eslErrGeneral;
        }

        // dequantization buffers
        {
            int channel_size = sizeof(float) << dec->m_mioih.dwSubbandDegree;
            dec->m_ptrWeightTable = malloc(channel_size);
            if (!dec->m_ptrWeightTable)
                return eslErrGeneral;
        }

        // LOT buffers
        {
            int nBlocksetSamples = dec->m_mioih.dwChannelCount << dec->m_mioih.dwSubbandDegree;
            int nLappedSamples = nBlocksetSamples * dec->m_mioih.dwLappedDegree;
            if (nLappedSamples > 0) {
                dec->m_ptrLastDCT = malloc(sizeof(float) * nLappedSamples);
                if (!dec->m_ptrLastDCT)
                    return eslErrGeneral;

                for (int i = 0; i < nLappedSamples; i++) {
                    dec->m_ptrLastDCT[i] = 0.0F;
                }
            }
        }

        if (MIODecoder_InitializeWithDegree(dec, dec->m_mioih.dwSubbandDegree))
            return eslErrGeneral;
    }
    else {
        return eslErrGeneral;
    }

    return eslErrSuccess;
}

static ESLError MIODecoder_DecodeSoundPCM8(MIODecoder* dec, MIOContext* context, const MIO_DATA_HEADER* datahdr, void* ptrWaveBuf) {
    unsigned int nSampleCount = datahdr->dwSampleCount;
    unsigned int nChannelCount = dec->m_mioih.dwChannelCount;
    unsigned int nAllSampleCount = nSampleCount * nChannelCount;
    ULONG nBytes = nAllSampleCount * sizeof(BYTE);

    if (nSampleCount > dec->m_nBufLength) {
        free(dec->m_ptrBuffer1);
        dec->m_ptrBuffer1 = malloc(nBytes);
        if (!dec->m_ptrBuffer1)
            return eslErrGeneral;

        dec->m_nBufLength = nSampleCount;
    }

    // prepare huffman codes
    if (datahdr->bytFlags & MIO_LEAD_BLOCK) {
        ESLError err = MIOContext_PrepareToDecodeERINACode(context, ERINAEncodingFlag_efERINAOrder1);
        if (err != eslErrSuccess) return err;
    }

    if (MIOContext_DecodeSymbolBytes(context, (SBYTE*)dec->m_ptrBuffer1, nBytes) < nBytes) {
        return eslErrGeneral;
    }

    // differential processing output
    BYTE* ptrSrcBuf = dec->m_ptrBuffer1;
    BYTE* ptrDstBuf;
    unsigned int nStep = nChannelCount;
    unsigned int i, j;
    for (i = 0; i < dec->m_mioih.dwChannelCount; i++) {
        ptrDstBuf = ptrWaveBuf;
        ptrDstBuf += i;

        BYTE bytValue = 0;
        for (j = 0; j < nSampleCount; j++) {
            bytValue += *(ptrSrcBuf++);
            *ptrDstBuf = bytValue;
            ptrDstBuf += nStep;
        }
    }

    return eslErrSuccess;
}

static ESLError MIODecoder_DecodeSoundPCM16(MIODecoder* dec, MIOContext* context, const MIO_DATA_HEADER* datahdr, void* ptrWaveBuf) {
    unsigned int i, j;
    unsigned int nSampleCount = datahdr->dwSampleCount;
    unsigned int nChannelCount = dec->m_mioih.dwChannelCount;
    unsigned int nAllSampleCount = nSampleCount * nChannelCount;
    ULONG nBytes = nAllSampleCount * sizeof(SWORD);

    if (nSampleCount > dec->m_nBufLength) {
        free(dec->m_ptrBuffer1);
        free(dec->m_ptrBuffer2);
        dec->m_ptrBuffer1 = malloc(nBytes);
        dec->m_ptrBuffer2 = malloc(nBytes);
        if (!dec->m_ptrBuffer1 || !dec->m_ptrBuffer2)
            return eslErrGeneral;

        dec->m_nBufLength = nSampleCount;
    }

    // prepare huffman codes
    if (datahdr->bytFlags & MIO_LEAD_BLOCK) {
        ESLError err = MIOContext_PrepareToDecodeERINACode(context, ERINAEncodingFlag_efERINAOrder1);
        if (err != eslErrSuccess) return err;
    }

    if (MIOContext_DecodeSymbolBytes(context, (SBYTE*)dec->m_ptrBuffer1, nBytes) < nBytes) {
        return eslErrGeneral;
    }

    // pack hi/lo bytes
    BYTE* pbytSrcBuf1;
    BYTE* pbytSrcBuf2;
    BYTE* pbytDstBuf;
    for (i = 0; i < nChannelCount; i++) {
        unsigned int nOffset = i * nSampleCount * sizeof(SWORD);
        pbytSrcBuf1 = ((BYTE*)dec->m_ptrBuffer1) + nOffset;
        pbytSrcBuf2 = pbytSrcBuf1 + nSampleCount;
        pbytDstBuf = ((BYTE*)dec->m_ptrBuffer2) + nOffset;

        for (j = 0; j < nSampleCount; j++) {
            SBYTE bytLow = pbytSrcBuf2[j];
            SBYTE bytHigh = pbytSrcBuf1[j];
            pbytDstBuf[j * sizeof(SWORD) + 0] = bytLow;
            pbytDstBuf[j * sizeof(SWORD) + 1] = bytHigh ^ (bytLow >> 7);
        }
    }

    // differential processing output
    SWORD* ptrSrcBuf = dec->m_ptrBuffer2;
    SWORD* ptrDstBuf;
    unsigned int nStep = dec->m_mioih.dwChannelCount;
    for (i = 0; i < dec->m_mioih.dwChannelCount; i++) {
        ptrDstBuf = ptrWaveBuf;
        ptrDstBuf += i;

        SWORD wValue = 0;
        SWORD wDelta = 0;
        for (j = 0; j < nSampleCount; j++) {
            wDelta += *(ptrSrcBuf++);
            wValue += wDelta;
            *ptrDstBuf = wValue;
            ptrDstBuf += nStep;
        }
    }

    return eslErrSuccess;
}

//////////////////////////////////////////////////////////////////////////////

static void MIODecoder_IQuantumize(MIODecoder* dec, float* ptrDestination, const INT* ptrQuantumized, int nDegreeNum, SDWORD nWeightCode, int nCoefficient) {
    int i, j;
    double rMatrixScale = sqrt(2.0 / nDegreeNum);
    double rCoefficient = rMatrixScale * nCoefficient;

    // generate weight table
    double rAvgRatio[7];
    for (i = 0; i < 6; i++) {
        rAvgRatio[i] = 1.0 / pow(2.0, (((nWeightCode >> (i * 5)) & 0x1F) - 15) * 0.5);
    }
    rAvgRatio[6] = 1.0;

    for (i = 0; i < dec->m_nFrequencyPoint[0]; i++) {
        dec->m_ptrWeightTable[i] = (float)rAvgRatio[0];
    }

    for (j = 1; j < 7; j++) {
        double a = rAvgRatio[j - 1];
        double k = (rAvgRatio[j] - a) / (dec->m_nFrequencyPoint[j] - dec->m_nFrequencyPoint[j - 1]);
        while (i < dec->m_nFrequencyPoint[j]) {
            dec->m_ptrWeightTable[i] = (float)(k * (i - dec->m_nFrequencyPoint[j - 1]) + a);
            i++;
        }
    }

    while (i < nDegreeNum) {
        dec->m_ptrWeightTable[i] = (float)rAvgRatio[6];
        i++;
    }

    float rOddWeight = (float)((((nWeightCode >> 30) & 0x03) + 0x02) / 2.0);
    for (i = 15; i < nDegreeNum; i += 16) {
        dec->m_ptrWeightTable[i] *= rOddWeight;
    }
    dec->m_ptrWeightTable[nDegreeNum - 1] = (float)nCoefficient;

    for (i = 0; i < nDegreeNum; i++) {
        dec->m_ptrWeightTable[i] = 1.0F / dec->m_ptrWeightTable[i];
    }

    // dequantize
    for (i = 0; i < nDegreeNum; i++) {
        ptrDestination[i] = (float)(rCoefficient * dec->m_ptrWeightTable[i] * ptrQuantumized[i]);
    }
}

//////////////////////////////////////////////////////////////////////////////

/* Dequantizes and transforms block frames. Lib divides them into "lead" (keyframes),
 * "internal" (standard) and "post" blocks, handled slightly differently. All of them can be MSS
 * blocks, which OG code handles in a separate function (presumably as a minor optimization to
 * avoid setting extra FORs), but here are handled with a flag since they are very similar. */

static ESLError MIODecoder_DecodeLeadBlock_All(MIODecoder* dec, int is_mss) {
    unsigned int ch, i;
    unsigned int nHalfDegree = dec->m_nDegreeNum / 2;
    int internal_channels = is_mss ? 2 : 1;
    //int output_channels = is_mss ? 2 : dec->m_mioih.dwChannelCount;

    SDWORD nWeightCode = *(dec->m_ptrNextWeight++);
    int nCoefficient = *(dec->m_ptrNextCoefficient++);

    float* ptrLapBuf;


    // dequantize
    ptrLapBuf = dec->m_ptrLastDCTBuf;
    for (ch = 0; ch < internal_channels; ch++) {
        INT* ptrTempBuf = dec->m_ptrBuffer1;
        for (i = 0; i < nHalfDegree; i++) {
            ptrTempBuf[i * 2] = 0;
            ptrTempBuf[i * 2 + 1] = *(dec->m_ptrNextSource++);
        }
        MIODecoder_IQuantumize(dec, ptrLapBuf, ptrTempBuf, dec->m_nDegreeNum, nWeightCode, nCoefficient);

        ptrLapBuf += dec->m_nDegreeNum;
    }

    // revolve
    if (is_mss) {
        float rSin, rCos;
        int nRevCode = *(dec->m_ptrNextRevCode++);

        float* ptrLapBuf1 = dec->m_ptrLastDCT;
        float* ptrLapBuf2 = dec->m_ptrLastDCT + dec->m_nDegreeNum;

        rSin = (float)sin(nRevCode * ERI_PI / 8);
        rCos = (float)cos(nRevCode * ERI_PI / 8);
        EMT_eriRevolve2x2(ptrLapBuf1, ptrLapBuf2, rSin, rCos, 1, dec->m_nDegreeNum);
    }

    // set duplicate parameters
    ptrLapBuf = dec->m_ptrLastDCTBuf;
    for (ch = 0; ch < internal_channels; ch++) {
        EMT_eriOddGivensInverseMatrix(ptrLapBuf, dec->m_pRevolveParam, dec->m_nSubbandDegree);

        for (i = 0; i < dec->m_nDegreeNum; i += 2) {
            ptrLapBuf[i] = ptrLapBuf[i + 1];
        }

        EMT_eriFastIPLOT(ptrLapBuf, dec->m_nSubbandDegree);

        ptrLapBuf += dec->m_nDegreeNum;
    }

    return eslErrSuccess;
}

static ESLError MIODecoder_DecodeInternalBlock_All(MIODecoder* dec, SWORD* ptrDst, unsigned int nSamples, int is_mss) {
    unsigned int ch, i;
    int internal_channels = is_mss ? 2 : 1;
    int output_channels = is_mss ? 2 : dec->m_mioih.dwChannelCount;

    SDWORD nWeightCode = *(dec->m_ptrNextWeight++);
    int nCoefficient = *(dec->m_ptrNextCoefficient++);

    float* ptrLapBuf;
    float* ptrSrcBuf;

    // dequantize
    ptrSrcBuf = dec->m_ptrMatrixBuf;
    for (ch = 0; ch < internal_channels; ch++) {
        MIODecoder_IQuantumize(dec, ptrSrcBuf, dec->m_ptrNextSource, dec->m_nDegreeNum, nWeightCode, nCoefficient);
        dec->m_ptrNextSource += dec->m_nDegreeNum;

        ptrSrcBuf += dec->m_nDegreeNum;
    }

    // revolve
    if (is_mss) {
        float rSin, rCos;
        int nRevCode = *(dec->m_ptrNextRevCode++);
        int nRevCode1 = (nRevCode >> 2) & 0x03;
        int nRevCode2 = (nRevCode & 0x03);

        float* ptrSrcBuf1 = dec->m_ptrMatrixBuf;
        float* ptrSrcBuf2 = dec->m_ptrMatrixBuf + dec->m_nDegreeNum;

        rSin = (float)sin(nRevCode1 * ERI_PI / 8);
        rCos = (float)cos(nRevCode1 * ERI_PI / 8);
        EMT_eriRevolve2x2(ptrSrcBuf1, ptrSrcBuf2, rSin, rCos, 2, dec->m_nDegreeNum / 2);

        rSin = (float)sin(nRevCode2 * ERI_PI / 8);
        rCos = (float)cos(nRevCode2 * ERI_PI / 8);
        EMT_eriRevolve2x2(ptrSrcBuf1 + 1, ptrSrcBuf2 + 1, rSin, rCos, 2, dec->m_nDegreeNum / 2);
    }

    // inverse LOT + DCT
    ptrLapBuf = dec->m_ptrLastDCTBuf;
    ptrSrcBuf = dec->m_ptrMatrixBuf;
    for (ch = 0; ch < internal_channels; ch++) {
        EMT_eriOddGivensInverseMatrix(ptrSrcBuf, dec->m_pRevolveParam, dec->m_nSubbandDegree);

        EMT_eriFastIPLOT(ptrSrcBuf, dec->m_nSubbandDegree);
        EMT_eriFastILOT(dec->m_ptrWorkBuf, ptrLapBuf, ptrSrcBuf, dec->m_nSubbandDegree);

        for (i = 0; i < dec->m_nDegreeNum; i++) {
            ptrLapBuf[i] = ptrSrcBuf[i];
            ptrSrcBuf[i] = dec->m_ptrWorkBuf[i];
        }

        EMT_eriFastIDCT(dec->m_ptrInternalBuf, ptrSrcBuf, 1, dec->m_ptrWorkBuf, dec->m_nSubbandDegree);

        EMT_eriRoundR32ToWordArray(ptrDst + ch, output_channels, dec->m_ptrInternalBuf, nSamples);

        ptrLapBuf += dec->m_nDegreeNum;
        ptrSrcBuf += dec->m_nDegreeNum;
    }

    return eslErrSuccess;
}

static ESLError MIODecoder_DecodePostBlock_All(MIODecoder* dec, SWORD* ptrDst, unsigned int nSamples, int is_mss) {
    unsigned int ch, i;
    unsigned int nHalfDegree = dec->m_nDegreeNum / 2;
    int internal_channels = is_mss ? 2 : 1;
    int output_channels = is_mss ? 2 : dec->m_mioih.dwChannelCount;

    SDWORD nWeightCode = *(dec->m_ptrNextWeight++);
    int nCoefficient = *(dec->m_ptrNextCoefficient++);

    float* ptrLapBuf;
    float* ptrSrcBuf;


    // dequantize
    ptrSrcBuf = dec->m_ptrMatrixBuf;
    for (ch = 0; ch < internal_channels; ch++) {
        INT* ptrTempBuf = dec->m_ptrBuffer1;
        for (i = 0; i < nHalfDegree; i++) {
            ptrTempBuf[i * 2] = 0;
            ptrTempBuf[i * 2 + 1] = *(dec->m_ptrNextSource++);
        }
        MIODecoder_IQuantumize(dec, ptrSrcBuf, ptrTempBuf, dec->m_nDegreeNum, nWeightCode, nCoefficient);

        ptrSrcBuf += dec->m_nDegreeNum;
    }

    // revolve
    if (is_mss) {
        float rSin, rCos;
        int nRevCode = *(dec->m_ptrNextRevCode++);

        float* ptrSrcBuf1 = dec->m_ptrMatrixBuf; //L
        float* ptrSrcBuf2 = dec->m_ptrMatrixBuf + dec->m_nDegreeNum; //R

        rSin = (float)sin(nRevCode * ERI_PI / 8);
        rCos = (float)cos(nRevCode * ERI_PI / 8);
        EMT_eriRevolve2x2(ptrSrcBuf1, ptrSrcBuf2, rSin, rCos, 1, dec->m_nDegreeNum);
    }

    // inverse LOT + DCT
    ptrLapBuf = dec->m_ptrLastDCTBuf;
    ptrSrcBuf = dec->m_ptrMatrixBuf;
    for (ch = 0; ch < internal_channels; ch++) {
        EMT_eriOddGivensInverseMatrix(ptrSrcBuf, dec->m_pRevolveParam, dec->m_nSubbandDegree);

        for (i = 0; i < dec->m_nDegreeNum; i += 2) {
            ptrSrcBuf[i] = -ptrSrcBuf[i + 1];
        }

        EMT_eriFastIPLOT(ptrSrcBuf, dec->m_nSubbandDegree);
        EMT_eriFastILOT(dec->m_ptrWorkBuf, ptrLapBuf, ptrSrcBuf, dec->m_nSubbandDegree);

        for (i = 0; i < dec->m_nDegreeNum; i++) {
            ptrSrcBuf[i] = dec->m_ptrWorkBuf[i];
        }

        EMT_eriFastIDCT(dec->m_ptrInternalBuf, ptrSrcBuf, 1, dec->m_ptrWorkBuf, dec->m_nSubbandDegree);

        EMT_eriRoundR32ToWordArray(ptrDst + ch, output_channels, dec->m_ptrInternalBuf, nSamples);

        ptrLapBuf += dec->m_nDegreeNum;
        ptrSrcBuf += dec->m_nDegreeNum;
    }

    return eslErrSuccess;
}

//////////////////////////////////////////////////////////////////////////////

static ESLError MIODecoder_DecodeSoundDCT_Std(MIODecoder* dec, MIOContext* context, const MIO_DATA_HEADER* datahdr, void* ptrWaveBuf) {
    unsigned int i, j, k;
    unsigned int nDegreeWidth = (1 << dec->m_mioih.dwSubbandDegree);
    unsigned int nSampleCount = (datahdr->dwSampleCount + nDegreeWidth - 1) & ~(nDegreeWidth - 1);
    unsigned int nSubbandCount = (nSampleCount >> dec->m_mioih.dwSubbandDegree);
    unsigned int nChannelCount = dec->m_mioih.dwChannelCount;
    unsigned int nAllSampleCount = nSampleCount * nChannelCount;
    unsigned int nAllSubbandCount = nSubbandCount * nChannelCount;

    if (nSampleCount > dec->m_nBufLength) {
        free(dec->m_ptrBuffer2);
        free(dec->m_ptrBuffer3);
        free(dec->m_ptrDivisionTable);
        free(dec->m_ptrWeightCode);
        free(dec->m_ptrCoefficient);

        dec->m_ptrBuffer2 = malloc(nAllSampleCount * sizeof(INT));
        dec->m_ptrBuffer3 = malloc(nAllSampleCount * sizeof(SWORD));
        dec->m_ptrDivisionTable = malloc(nAllSubbandCount * sizeof(BYTE));
        dec->m_ptrWeightCode = malloc(nAllSubbandCount * 5 * sizeof(SDWORD));
        dec->m_ptrCoefficient = malloc(nAllSubbandCount * 5 * sizeof(INT));
        if (!dec->m_ptrBuffer2 || !dec->m_ptrBuffer3 || !dec->m_ptrDivisionTable || !dec->m_ptrWeightCode || !dec->m_ptrCoefficient)
            return eslErrGeneral;

        dec->m_nBufLength = nSampleCount;
    }

    // decode quantization table
    if (MIOContext_GetABit(context) != 0) {
        return eslErrGeneral;
    }

    unsigned int pLastDivision[MIO_MAX_CHANNELS];
    for (i = 0; i < nChannelCount; i++) {
        pLastDivision[i] = -1;
    }

    dec->m_ptrNextDivision = dec->m_ptrDivisionTable;
    dec->m_ptrNextWeight = dec->m_ptrWeightCode;
    dec->m_ptrNextCoefficient = dec->m_ptrCoefficient;

    for (i = 0; i < nSubbandCount; i++) {
        for (j = 0; j < nChannelCount; j++) {
            unsigned int nDivisionCode = MIOContext_GetNBits(context, 2);
            *(dec->m_ptrNextDivision++) = (BYTE)nDivisionCode;

            if (nDivisionCode != pLastDivision[j]) {
                if (i != 0) {
                    *(dec->m_ptrNextWeight++) = MIOContext_GetNBits(context, 32);
                    *(dec->m_ptrNextCoefficient++) = MIOContext_GetNBits(context, 16);
                }
                pLastDivision[j] = nDivisionCode;
            }

            unsigned int nDivisionCount = (1 << nDivisionCode);
            for (k = 0; k < nDivisionCount; k++) {
                *(dec->m_ptrNextWeight++) = MIOContext_GetNBits(context, 32);
                *(dec->m_ptrNextCoefficient++) = MIOContext_GetNBits(context, 16);
            }
        }
    }
    if (nSubbandCount > 0) {
        for (i = 0; i < nChannelCount; i++) {
            *(dec->m_ptrNextWeight++) = MIOContext_GetNBits(context, 32);
            *(dec->m_ptrNextCoefficient++) = MIOContext_GetNBits(context, 16);
        }
    }

    /* sync? */
    if (MIOContext_GetABit(context) != 0) {
        return eslErrGeneral;
    }

    // init code model if needed
    if (datahdr->bytFlags & MIO_LEAD_BLOCK) {
        if (dec->m_mioih.dwArchitecture != ERISA_NEMESIS_CODE) {
            ESLError err = MIOContext_PrepareToDecodeERINACode(context, ERINAEncodingFlag_efERINAOrder1);
            if (err != eslErrSuccess) return err;
        }
        else {
            ESLError err = MIOContext_PrepareToDecodeERISACode(context);
            if (err != eslErrSuccess) return err;
        }
    }
    else if (dec->m_mioih.dwArchitecture == ERISA_NEMESIS_CODE) {
        MIOContext_InitializeERISACode(context);
    }

    // decode and deinterleave
    if (dec->m_mioih.dwArchitecture != ERISA_NEMESIS_CODE) {
        if (MIOContext_DecodeSymbolBytes(context, dec->m_ptrBuffer3, nAllSampleCount * 2) < nAllSampleCount * 2) {
            return eslErrGeneral;
        }

        SBYTE* ptrHBuf = dec->m_ptrBuffer3;
        SBYTE* ptrLBuf = ptrHBuf + nAllSampleCount;

        for (i = 0; i < nDegreeWidth; i++) {
            INT* ptrQuantumized = ((INT*)dec->m_ptrBuffer2) + i;

            for (j = 0; j < nAllSubbandCount; j++) {
                INT nLow = *(ptrLBuf++);
                INT nHigh = *(ptrHBuf++) ^ (nLow >> 8);
                *ptrQuantumized = (nLow & 0xFF) | (nHigh << 8);
                ptrQuantumized += nDegreeWidth;
            }
        }
    }
    else {
        if (MIOContext_DecodeERISACodeWords(context, (SWORD*)dec->m_ptrBuffer3, nAllSampleCount) < nAllSampleCount) {
            return eslErrGeneral;
        }
        SWORD* ptrTmp = dec->m_ptrBuffer3;
        for (i = 0; i < nAllSampleCount; i++) {
            ((INT*)dec->m_ptrBuffer2)[i] = (ptrTmp)[i];
        }
    }

    // apply iDCT to subband units
    unsigned int nSamples;
    unsigned int pRestSamples[MIO_MAX_CHANNELS];
    SWORD* ptrDstBuf[MIO_MAX_CHANNELS];

    dec->m_ptrNextDivision = dec->m_ptrDivisionTable;
    dec->m_ptrNextWeight = dec->m_ptrWeightCode;
    dec->m_ptrNextCoefficient = dec->m_ptrCoefficient;
    dec->m_ptrNextSource = dec->m_ptrBuffer2;

    for (i = 0; i < nChannelCount; i++) {
        pLastDivision[i] = -1;
        pRestSamples[i] = datahdr->dwSampleCount;
        ptrDstBuf[i] = ((SWORD*)ptrWaveBuf) + i;
    }
    unsigned int nCurrentDivision = -1;

    for (i = 0; i < nSubbandCount; i++) {
        for (j = 0; j < nChannelCount; j++) {
            // get division/decomposition code
            unsigned int nDivisionCode = *(dec->m_ptrNextDivision++);
            unsigned int nDivisionCount = (1 << nDivisionCode);

            // get buffer for duplicate processing
            int nChannelStep = nDegreeWidth * dec->m_mioih.dwLappedDegree * j;
            dec->m_ptrLastDCTBuf = dec->m_ptrLastDCT + nChannelStep;

            // processing when matrix size changes
            int bLeadBlock = 0;

            if (pLastDivision[j] != nDivisionCode) {
                // complete until last moment
                if (i != 0) {
                    if (nCurrentDivision != pLastDivision[j]) {
                        if (MIODecoder_InitializeWithDegree(dec, dec->m_mioih.dwSubbandDegree - pLastDivision[j]))
                            return eslErrGeneral;
                        nCurrentDivision = pLastDivision[j];
                    }
                    nSamples = pRestSamples[j];
                    if (nSamples > dec->m_nDegreeNum) {
                        nSamples = dec->m_nDegreeNum;
                    }
                    if (MIODecoder_DecodePostBlock_All(dec, ptrDstBuf[j], nSamples, 0)) {
                        return eslErrGeneral;
                    }
                    pRestSamples[j] -= nSamples;
                    ptrDstBuf[j] += nSamples * nChannelCount;
                }

                // set params to change matrix size
                pLastDivision[j] = nDivisionCode;
                bLeadBlock = 1;
            }
            if (nCurrentDivision != nDivisionCode) {
                if (MIODecoder_InitializeWithDegree(dec, dec->m_mioih.dwSubbandDegree - nDivisionCode))
                    return eslErrGeneral;
                nCurrentDivision = nDivisionCode;
            }

            // perform sequential iLOT
            for (k = 0; k < nDivisionCount; k++) {
                if (bLeadBlock) {
                    // decode lead block
                    if (MIODecoder_DecodeLeadBlock_All(dec, 0)) {
                        return eslErrGeneral;
                    }

                    bLeadBlock = 0;
                }
                else {
                    // decode regular block
                    nSamples = pRestSamples[j];
                    if (nSamples > dec->m_nDegreeNum) {
                        nSamples = dec->m_nDegreeNum;
                    }
                    if (MIODecoder_DecodeInternalBlock_All(dec, ptrDstBuf[j], nSamples, 0)) {
                        return eslErrGeneral;
                    }
                    pRestSamples[j] -= nSamples;
                    ptrDstBuf[j] += nSamples * nChannelCount;
                }
            }
        }
    }

    // complete the matrix
    if (nSubbandCount > 0) {
        for (i = 0; i < nChannelCount; i++) {
            int nChannelStep = nDegreeWidth * dec->m_mioih.dwLappedDegree * i;
            dec->m_ptrLastDCTBuf = dec->m_ptrLastDCT + nChannelStep;

            if (nCurrentDivision != pLastDivision[i]) {
                if (MIODecoder_InitializeWithDegree(dec, dec->m_mioih.dwSubbandDegree - pLastDivision[i]))
                    return eslErrGeneral;
                nCurrentDivision = pLastDivision[i];
            }
            nSamples = pRestSamples[i];
            if (nSamples > dec->m_nDegreeNum) {
                nSamples = dec->m_nDegreeNum;
            }
            if (MIODecoder_DecodePostBlock_All(dec, ptrDstBuf[i], nSamples, 0)) {
                return eslErrGeneral;
            }
            pRestSamples[i] -= nSamples;
            ptrDstBuf[i] += nSamples * nChannelCount;
        }
    }

    return eslErrSuccess;
}

static ESLError MIODecoder_DecodeSoundDCT_MSS(MIODecoder* dec, MIOContext* context, const MIO_DATA_HEADER* datahdr, void* ptrWaveBuf) {
    unsigned int i, j, k;
    unsigned int nDegreeWidth = (1 << dec->m_mioih.dwSubbandDegree);
    unsigned int nSampleCount = (datahdr->dwSampleCount + nDegreeWidth - 1) & ~(nDegreeWidth - 1);
    unsigned int nSubbandCount = (nSampleCount >> dec->m_mioih.dwSubbandDegree);
    unsigned int nChannelCount = dec->m_mioih.dwChannelCount;  // usually 2
    unsigned int nAllSampleCount = nSampleCount * nChannelCount;
    unsigned int nAllSubbandCount = nSubbandCount;

    if (nSampleCount > dec->m_nBufLength) {
        free(dec->m_ptrBuffer2);
        free(dec->m_ptrBuffer3);
        free(dec->m_ptrDivisionTable);
        free(dec->m_ptrRevolveCode);
        free(dec->m_ptrWeightCode);
        free(dec->m_ptrCoefficient);

        dec->m_ptrBuffer2 = malloc(nAllSampleCount * sizeof(INT));
        dec->m_ptrBuffer3 = malloc(nAllSampleCount * sizeof(SWORD));
        dec->m_ptrDivisionTable = malloc(nAllSubbandCount * sizeof(BYTE));
        dec->m_ptrRevolveCode = malloc(nAllSubbandCount * 10 * sizeof(BYTE));
        dec->m_ptrWeightCode = malloc(nAllSubbandCount * 10 * sizeof(SDWORD));
        dec->m_ptrCoefficient = malloc(nAllSubbandCount * 10 * sizeof(INT));
        if (!dec->m_ptrBuffer2 || !dec->m_ptrBuffer3 || !dec->m_ptrDivisionTable || !dec->m_ptrRevolveCode || !dec->m_ptrWeightCode || !dec->m_ptrCoefficient)
            return eslErrGeneral;

        dec->m_nBufLength = nSampleCount;
    }

    // decode quantization table
    if (MIOContext_GetABit(context) != 0) {
        return eslErrGeneral;
    }

    unsigned int nLastDivision = -1;

    dec->m_ptrNextDivision = dec->m_ptrDivisionTable;
    dec->m_ptrNextRevCode = dec->m_ptrRevolveCode;
    dec->m_ptrNextWeight = dec->m_ptrWeightCode;
    dec->m_ptrNextCoefficient = dec->m_ptrCoefficient;

    for (i = 0; i < nSubbandCount; i++) {
        unsigned int nDivisionCode = MIOContext_GetNBits(context, 2);
        *(dec->m_ptrNextDivision++) = (BYTE)nDivisionCode;

        int bLeadBlock = 0;
        if (nDivisionCode != nLastDivision) {
            if (i != 0) {
                *(dec->m_ptrNextRevCode++) = MIOContext_GetNBits(context, 2);
                *(dec->m_ptrNextWeight++) = MIOContext_GetNBits(context, 32);
                *(dec->m_ptrNextCoefficient++) = MIOContext_GetNBits(context, 16);
            }
            bLeadBlock = 1;
            nLastDivision = nDivisionCode;
        }

        unsigned int nDivisionCount = (1 << nDivisionCode);
        for (k = 0; k < nDivisionCount; k++) {
            if (bLeadBlock) {
                *(dec->m_ptrNextRevCode++) = MIOContext_GetNBits(context, 2);
                bLeadBlock = 0;
            }
            else {
                *(dec->m_ptrNextRevCode++) = MIOContext_GetNBits(context, 4);
            }
            *(dec->m_ptrNextWeight++) = MIOContext_GetNBits(context, 32);
            *(dec->m_ptrNextCoefficient++) = MIOContext_GetNBits(context, 16);
        }
    }
    if (nSubbandCount > 0) {
        *(dec->m_ptrNextRevCode++) = MIOContext_GetNBits(context, 2);
        *(dec->m_ptrNextWeight++) = MIOContext_GetNBits(context, 32);
        *(dec->m_ptrNextCoefficient++) = MIOContext_GetNBits(context, 16);
    }

    if (MIOContext_GetABit(context) != 0) {
        return eslErrGeneral;
    }

    if (datahdr->bytFlags & MIO_LEAD_BLOCK) {
        if (dec->m_mioih.dwArchitecture != ERISA_NEMESIS_CODE) {
            ESLError err = MIOContext_PrepareToDecodeERINACode(context, ERINAEncodingFlag_efERINAOrder1);
            if (err != eslErrSuccess) return err;
        }
        else {
            ESLError err = MIOContext_PrepareToDecodeERISACode(context);
            if (err != eslErrSuccess) return err;
        }
    }
    else if (dec->m_mioih.dwArchitecture == ERISA_NEMESIS_CODE) {
        MIOContext_InitializeERISACode(context);
    }

    // decode and deinterleave
    if (dec->m_mioih.dwArchitecture != ERISA_NEMESIS_CODE) {
        if (MIOContext_DecodeSymbolBytes(context, dec->m_ptrBuffer3, nAllSampleCount * 2) < nAllSampleCount * 2) {
            return eslErrGeneral;
        }

        SBYTE* ptrHBuf = dec->m_ptrBuffer3;
        SBYTE* ptrLBuf = ptrHBuf + nAllSampleCount;

        for (i = 0; i < nDegreeWidth * 2; i++) {
            INT* ptrQuantumized = ((INT*)dec->m_ptrBuffer2) + i;

            for (j = 0; j < nAllSubbandCount; j++) {
                INT nLow = *(ptrLBuf++);
                INT nHigh = *(ptrHBuf++) ^ (nLow >> 8);
                *ptrQuantumized = (nLow & 0xFF) | (nHigh << 8);
                ptrQuantumized += nDegreeWidth * 2;
            }
        }
    }
    else {
        if (MIOContext_DecodeERISACodeWords(context, (SWORD*)dec->m_ptrBuffer3, nAllSampleCount) < nAllSampleCount) {
            return eslErrGeneral;
        }
        SWORD* ptrTmp = dec->m_ptrBuffer3;
        for (i = 0; i < nAllSampleCount; i++) {
            ((INT*)dec->m_ptrBuffer2)[i] = (ptrTmp)[i];
        }
    }

    // apply iDCT to subband units
    unsigned int nSamples;
    unsigned int nRestSamples = datahdr->dwSampleCount;
    SWORD* ptrDstBuf = ptrWaveBuf;

    nLastDivision = -1;
    dec->m_ptrNextDivision = dec->m_ptrDivisionTable;
    dec->m_ptrNextRevCode = dec->m_ptrRevolveCode;
    dec->m_ptrNextWeight = dec->m_ptrWeightCode;
    dec->m_ptrNextCoefficient = dec->m_ptrCoefficient;
    dec->m_ptrNextSource = dec->m_ptrBuffer2;

    for (i = 0; i < nSubbandCount; i++) {
        // get division/decomposition code
        unsigned int nDivisionCode = *(dec->m_ptrNextDivision++);
        unsigned int nDivisionCount = (1 << nDivisionCode);

        // processing when matrix size changes
        int bLeadBlock = 0;
        dec->m_ptrLastDCTBuf = dec->m_ptrLastDCT;

        if (nLastDivision != nDivisionCode) {
            // complete until last moment
            if (i != 0) {
                nSamples = nRestSamples;
                if (nSamples > dec->m_nDegreeNum) {
                    nSamples = dec->m_nDegreeNum;
                }
                if (MIODecoder_DecodePostBlock_All(dec, ptrDstBuf, nSamples, 1)) {
                    return eslErrGeneral;
                }
                nRestSamples -= nSamples;
                ptrDstBuf += nSamples * nChannelCount;
            }

            // set params to change matrix size
            if (MIODecoder_InitializeWithDegree(dec, dec->m_mioih.dwSubbandDegree - nDivisionCode))
                return eslErrGeneral;
            nLastDivision = nDivisionCode;
            bLeadBlock = 1;
        }

        // perform sequential iLOT
        for (k = 0; k < nDivisionCount; k++) {
            if (bLeadBlock) {
                // decode lead block
                if (MIODecoder_DecodeLeadBlock_All(dec, 1)) {
                    return eslErrGeneral;
                }

                bLeadBlock = 0;
            }
            else {
                // decode regular block
                nSamples = nRestSamples;
                if (nSamples > dec->m_nDegreeNum) {
                    nSamples = dec->m_nDegreeNum;
                }
                if (MIODecoder_DecodeInternalBlock_All(dec, ptrDstBuf, nSamples, 1)) {
                    return eslErrGeneral;
                }
                nRestSamples -= nSamples;
                ptrDstBuf += nSamples * nChannelCount;
            }
        }
    }

    // complete the matrix
    if (nSubbandCount > 0) {
        dec->m_ptrLastDCTBuf = dec->m_ptrLastDCT;

        nSamples = nRestSamples;
        if (nSamples > dec->m_nDegreeNum)
            nSamples = dec->m_nDegreeNum;

        if (MIODecoder_DecodePostBlock_All(dec, ptrDstBuf, nSamples, 1)) {
            return eslErrGeneral;
        }
        nRestSamples -= nSamples;
        ptrDstBuf += nSamples * nChannelCount;
    }

    return eslErrSuccess;
}

ESLError MIODecoder_DecodeSound(MIODecoder* dec, MIOContext* context, const MIO_DATA_HEADER* datahdr, void* ptrWaveBuf) {
    MIOContext_FlushBuffer(context);

    if (dec->m_mioih.fdwTransformation == CVTYPE_LOSSLESS_ERI) {
        if (dec->m_mioih.dwBitsPerSample == 8) {
            return MIODecoder_DecodeSoundPCM8(dec, context, datahdr, ptrWaveBuf);
        }
        else if (dec->m_mioih.dwBitsPerSample == 16) {
            return MIODecoder_DecodeSoundPCM16(dec, context, datahdr, ptrWaveBuf);
        }
    }
    else if ((dec->m_mioih.fdwTransformation == CVTYPE_LOT_ERI) || (dec->m_mioih.fdwTransformation == CVTYPE_LOT_ERI_MSS)) {
        if ((dec->m_mioih.dwChannelCount != 2) || (dec->m_mioih.fdwTransformation == CVTYPE_LOT_ERI)) {
            return MIODecoder_DecodeSoundDCT_Std(dec, context, datahdr, ptrWaveBuf);
        }
        else {
            return MIODecoder_DecodeSoundDCT_MSS(dec, context, datahdr, ptrWaveBuf);
        }
    }

    return eslErrGeneral;
}
