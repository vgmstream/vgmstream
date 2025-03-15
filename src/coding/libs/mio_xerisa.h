/*****************************************************************************
                         E R I S A - L i b r a r y
 -----------------------------------------------------------------------------
    Copyright (C) 2002-2004 Leshade Entis, Entis-soft. All rights reserved.
 *****************************************************************************/

#ifndef _MIO_ERISALIB_H_
#define _MIO_ERISALIB_H_

#include <stdint.h>
//#include <stdbool.h>

typedef unsigned char BYTE; //, *PBYTE;
typedef unsigned short int WORD; //, *PWORD;
typedef unsigned int UDWORD;
typedef signed int INT; //, *PINT;
typedef unsigned int UINT;
//typedef signed long int LONG; //, *PLONG;
typedef unsigned long int ULONG;

typedef signed char SBYTE;
typedef signed short int SWORD;
typedef signed int SDWORD;


typedef enum {
    eslErrSuccess = 0,
    eslErrGeneral = 1,
    eslErrEof = 2, //extra
} ESLError;

void ERI_eriInitializeLibrary(void);
void ERI_eriCloseLibrary(void);

typedef struct {
    uint64_t cHeader;           // signature
    UDWORD dwFileID;
    UDWORD dwReserved;          // 0
    char cFormatDesc[0x30];     // format name
} EMC_FILE_HEADER;

typedef struct {
    uint64_t nRecordID;
    uint64_t nRecLength;
} EMC_RECORD_HEADER;

typedef struct {
    UDWORD dwVersion;
    UDWORD dwContainedFlag;
    UDWORD dwKeyFrameCount;
    UDWORD dwFrameCount;
    UDWORD dwAllFrameTime;
} ERI_FILE_HEADER;

/* transformation (either lossless or LOT) */
#define CVTYPE_LOSSLESS_ERI 0x03020000
#define CVTYPE_DCT_ERI 0x00000001 // not used in audio
#define CVTYPE_LOT_ERI 0x00000005
#define CVTYPE_LOT_ERI_MSS 0x00000105

/* architecture (huffman for lossless or gamma/nemesis for LOT) */
#define ERI_ARITHMETIC_CODE 32  // not used in audio?
#define ERI_RUNLENGTH_GAMMA 0xFFFFFFFF //same as huffman with "gamma escape codes"
#define ERI_RUNLENGTH_HUFFMAN 0xFFFFFFFC
#define ERISA_NEMESIS_CODE 0xFFFFFFF0 // arithmetic coding (sliding window?)

// per file
typedef struct {
    UDWORD dwVersion;
    UDWORD fdwTransformation;
    UDWORD dwArchitecture;
    UDWORD dwChannelCount;
    UDWORD dwSamplesPerSec;  // input sample rate (output is fixed)
    UDWORD dwBlocksetCount;
    UDWORD dwSubbandDegree;
    UDWORD dwAllSampleCount;
    UDWORD dwLappedDegree;
    UDWORD dwBitsPerSample;

   // extra from comments
    UDWORD rewindPoint;
} MIO_INFO_HEADER;

// per chunk
typedef struct {
    BYTE bytVersion;
    BYTE bytFlags;
    BYTE bytReserved1;
    BYTE bytReserved2;
    UDWORD dwSampleCount;
}  MIO_DATA_HEADER;

// Block is a "keyframe" and can be user to start decoding after flush/seek.
// Starting to decode in a non-keyframe returns an error.
#define MIO_LEAD_BLOCK 0x01


#include "mio_erisacontext.h"
#include "mio_erisafile.h"
#include "mio_erisamatrix.h"
#include "mio_erisasound.h"

#endif
