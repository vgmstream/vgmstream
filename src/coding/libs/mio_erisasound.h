/* Decodes audio context data into samples.
 */
#ifndef _MIO_ERISA_SOUND_H_
#define _MIO_ERISA_SOUND_H_

typedef struct {
    MIO_INFO_HEADER m_mioih;    // voice info header (current file)

    unsigned int m_nBufLength;  // block's sample count

    // various internal (alloc'd) bufs
    void* m_ptrBuffer1;         // processing
    void* m_ptrBuffer2;         // reordering
    void* m_ptrBuffer3;         // interleave

    BYTE* m_ptrDivisionTable;   // block's division codes
    BYTE* m_ptrRevolveCode;     // block's revolve codes
    SDWORD* m_ptrWeightCode;    // block's quant weights
    INT* m_ptrCoefficient;      // block's scales

    float* m_ptrMatrixBuf;      // matrix ops
    float* m_ptrInternalBuf;    // ops helper
    float* m_ptrWorkBuf;        // DCT work buf
    float* m_ptrWeightTable;    // freq weights
    float* m_ptrLastDCT;        // previous DCT values

    // temp pointers for the above (current pos)
    BYTE* m_ptrNextDivision;
    BYTE* m_ptrNextRevCode;
    SDWORD* m_ptrNextWeight;
    INT* m_ptrNextCoefficient;
    INT* m_ptrNextSource;
    float* m_ptrLastDCTBuf;

    // file degree config
    unsigned int m_nSubbandDegree;  // matrix size (N)
    unsigned int m_nDegreeNum;      // derived matrix size (2^N)
    ERI_SIN_COS* m_pRevolveParam;   // 

    int m_nFrequencyPoint[7];       // freq center points
} MIODecoder;

MIODecoder* MIODecoder_Open();
void MIODecoder_Close(MIODecoder* dec);

ESLError MIODecoder_Initialize(MIODecoder* dec, const MIO_INFO_HEADER* infhdr);
ESLError MIODecoder_DecodeSound(MIODecoder* dec, MIOContext* context, const MIO_DATA_HEADER* datahdr, void* ptrWaveBuf);

#endif
