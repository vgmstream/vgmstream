/* iDTC/iLOT/etc helper functions.
 */
#ifndef _MIO_ERISA_FILE_H_
#define _MIO_ERISA_FILE_H_

/* custom IO (OG lib uses FILEs) */
#include "../../util/io_callback.h"


typedef struct {
    EMC_FILE_HEADER emcfh;  // base header (EMC = Entis Media Complex)
    ERI_FILE_HEADER erifh;  // common file chunk
    MIO_INFO_HEADER mioih;  // audio audio chunk
    uint32_t start;

    char* desc;             // text info
    int desc_len;           // extra

    MIO_DATA_HEADER miodh;  // packet info
    uint8_t* buf;
    int buf_size; 
    uint8_t* packet; //m_ptrBuffer
    int packet_size;

    void* ptrWaveBuf;       // not part of the original code but to simplify usage
    int ptrWaveBuf_len;
} MIOFile;

MIOFile* MIOFile_Open();
void MIOFile_Close(MIOFile* mf);
ESLError MIOFile_Initialize(MIOFile* mf, io_callback_t* file);
ESLError MIOFile_Reset(MIOFile* mf, io_callback_t* file);
ESLError MIOFile_NextPacket(MIOFile* mf, io_callback_t* file);

int MIOFile_GetTagLoop(MIOFile* mf, const char* tag);
// extra
void* MIOFile_GetCurrentWaveBuffer(MIOFile* mf);
int MIOFile_GetCurrentWaveBufferCount(MIOFile* mf);

#endif
