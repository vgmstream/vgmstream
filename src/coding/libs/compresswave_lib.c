#include "compresswave_lib.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


/* Decodes CWav (CompressWave) audio codec, based on original delphi/pascal source code by Ko-Ta:
 * - http://kota.dokkoisho.com/
 * - http://kota.dokkoisho.com/library/CompressWave.zip
 * - https://web.archive.org/web/20180819144937/http://d.hatena.ne.jp/Ko-Ta/20070318/p1
 *  (no license given)
 * Apparently found in few Japanese (doujin?) games around 1995-2002, most notably RADIO ZONDE.
 *
 * This is mostly a simple re-implementation following original code, basically Pascal-classes-to-plain-C
 * because why not. Only decoder part is replicated (some writting/loading/etc stuff removed or cleaned up).
 * Results should be byte-exact (all is int math).
 * **some parts like internal looping weren't tested (no valid files)
 *
 * Codec is basically huffman-coded ADPCM, that includes diff table and huffman tree setup in
 * CWav header. Described by the author as being "big, heavy and with bad sound quality".
 * An oddity is that mono files are output as fake stereo (repeats L/R), this is correct and agrees
 * with PCM totals in header. Output sample rate is always 44100 and files marked as 22050 or mono just
 * decode slightly differently. Curiously PCM output size in header may be not be multiple of 4, meaning
 * files that end with garbage half-a-sample (L sample = 0, nothing for R).
 */


/* ************************************************************************* */
/* common */
/* ************************************************************************* */
// pascal reader simulated in C
typedef struct {
    STREAMFILE* File;
    int64_t Position;
    int64_t Size;
} TStream;

static void TStream_Read_Uint32(TStream* self, uint32_t* value) {
    uint8_t buf[0x4] = {0};

    read_streamfile(buf, self->Position, sizeof(buf), self->File);
    self->Position += 0x4;

    *value = get_u32le(buf);
}


/* ************************************************************************* */
/* HuffLib.pas */
/* ************************************************************************* */

#define CW_TRUE 1
#define CW_FALSE 0

typedef enum { nsEmpty, nsBranch, nsLeaf, nsRoot } TNodeState;

//------------------------------------------------------------------------------
//structure declaration

//node structure for huffman tree
typedef struct {
    uint8_t Value;                      //value (0..255)
    int32_t Weight;                     //weight value used during huffman tree creation
    TNodeState State;                   //state
    int32_t Link[2];                    //bidimensional tree L/R path (-1: unused, >=0: index)
} THuffTreeNode;

//header info for file writting
typedef struct {
    char HedChar[4];                    //head info
    int32_t Version;                    //Version
    uint32_t HistGraph[256];            //appearance rate
    int64_t FileSize;                   //file size
} THuffHedState;

//for jumping to arbitrary places (^^), various usages
typedef struct {
    uint32_t BitBuf;
    int32_t BitCount;
    int64_t StreamPos;
    uint32_t CipherBuf;
} THuffPositionData;

//------------------------------------------------------------------------------
//huffman encoding class
//handled values are 0~255 of 1 byte.
//takes appearance rate and makes a huffman tree for encoding

//EXTRA: external lib part, but not really needed so all is static

typedef struct {
    //control
    TStream Buff;           // target stream
    int64_t BeginPos;       // encoded area
    int Mode;               // (0=initial, 1=read, 2=write)

    //bit IO
    int IoCount;            // 0..initial state, 1..read, 2..write
    uint32_t BitBuf;        // held buffer
    int BitCount;           // processed bit count
#if 0
    int64_t BitWriteLen;    // written size
#endif
    uint32_t CipherBuf;

    //huffman
    THuffTreeNode Node[512];    //tree structure
    uint8_t Code[256][256];     //fork support
    int32_t Root;               //root

    //huffman cipher bits
    uint32_t CipherList[16];

    //header info
    THuffHedState Hed;
} THuff;


//related to huffman encoding
static void THuff_InitHuffTree(THuff* self); //initializes tree
static int THuff_InsertHuffNode(THuff* self, int v, int w, TNodeState s, int b1, int b2); //add node to tree
static void THuff_MakeHuffTree(THuff* self);

//related to single bit IO
static void THuff_BeginBitIO(THuff* self);
static void THuff_EndBitIO(THuff* self);
static int THuff_ReadBit(THuff* self);
static uint32_t THuff__ROR(uint32_t src, uint32_t shift);

static THuff* THuff_Create(TStream* buf); // creation
static void THuff_Free(THuff* self); // release the power
static void THuff_SetCipherCode(THuff* self, uint32_t msk); // encryption mask bits
//functions for reading
static void THuff_BeginRead(THuff* self);
static int THuff_Read(THuff* self);

#if 0
static int64_t THuff_GetFileSize(THuff* self); // get file size before encoding
static int THuff_GetEOF(THuff* self); // EOF detection
#endif
static void THuff_MoveBeginPosition(THuff* self); // return to initial state
static void THuff_GetPositionData(THuff* self, THuffPositionData* s);   // secret
static void THuff_SetPositionData(THuff* self, THuffPositionData* s);

//------------------------------------------------------------------------------
//create
static THuff* THuff_Create(TStream* buf) {
    THuff* self = calloc(1, sizeof(THuff));
    if (!self) return NULL;

    //define stream
    self->Buff = *buf;

    //initialization
    THuff_InitHuffTree(self);
    memcpy(self->Hed.HedChar, "HUF\0", 0x4);
    self->Hed.Version  = 1;
    self->Hed.FileSize = 0;

    //set cipher bits
    self->CipherBuf = 0;
    THuff_SetCipherCode(self, 0x0);

    //mode
    self->Mode = 0;

    return self;
}

//------------------------------------------------------------------------------
//free
static void THuff_Free(THuff* self) {
    if (self == NULL) return;
    if (self->Mode == 2)
        THuff_EndBitIO(self);
    free(self);
}

//------------------------------------------------------------------------------
//init tree structure (unused state)
static void THuff_InitHuffTree(THuff* self) {
    int i;

    for (i = 0; i < 512; i++) {
        self->Node[i].State = nsEmpty;
    }
}

//------------------------------------------------------------------------------
//add node to huffman tree
static int THuff_InsertHuffNode(THuff* self, int v, int w, TNodeState s, int b1, int b2) {
    int result = 0;
    int i;

    i = 0;
    while ((self->Node[i].State != nsEmpty) && (i < 512)) {
        i++;
    }

    if (i == 512) {
        result = -1;
        return result; //exit;
    }

    self->Node[i].Value = v & 0xFF; //BYTE(v);
    self->Node[i].Weight = w;
    self->Node[i].State  = s;
    self->Node[i].Link[0] = b1;
    if (self->Node[i].Link[0] > 511) {
        return -1;//? //halt;
    }
    self->Node[i].Link[1] = b2;
    if (self->Node[i].Link[1] > 511) {
        return -1;//? //halt;
    }
    //return entry number
    result = i;
    return result;
}

//------------------------------------------------------------------------------
//reads and expands huffman-encoded data
static int THuff_Read(THuff* self) {
    int i;

    i = self->Root;
    while (self->Node[i].State != nsLeaf) {
        i = self->Node[i].Link[THuff_ReadBit(self)];
    }

    return self->Node[i].Value;
}

//------------------------------------------------------------------------------
//creates fork code from tree

//finds node of lowest weight
static int THuff_MakeHuffTree_SerchMinNode(THuff* self, int* tNode) {
    int ii, aaa1, aaa2;

    aaa1 = 0xFFFFFFF;
    aaa2 = 0;
    for (ii = 0 ; ii < 256; ii++) {
        if (tNode[ii] != -1) {
            if (self->Node[tNode[ii]].Weight < aaa1) {
                aaa2 = ii;
                aaa1 = self->Node[tNode[ii]].Weight;
            }
        }
    }
    return aaa2;
}

//finds closest node
static int THuff_MakeHuffTree_SerchNearNode(THuff* self, int* tNode, int pos) {
    int ii, aaa1, aaa2;

    aaa1 = 0xFFFFFFF;
    aaa2 = 0;
    for (ii = 0 ; ii < 256; ii++) {
        if (tNode[ii] != -1) {
            if ((abs(self->Node[tNode[ii]].Weight - self->Node[tNode[pos]].Weight) < aaa1) && (pos != ii)) {
                aaa2 = ii;
                aaa1 = self->Node[tNode[ii]].Weight;
            }
        }
    }
    return aaa2;
}

static void THuff_MakeHuffTree_MakeHuffCodeFromTree(THuff* self, uint8_t* tCode1, int* tCodePos, int pos) {
    int ii, aaa1;

    if (self->Node[pos].State == nsLeaf) { //found
        tCode1[*tCodePos] = 0xFF;
        aaa1 = self->Node[pos].Value;
        for (ii = 0; ii < 256; ii++) {
            self->Code[aaa1][ii] = tCode1[ii];
        }
    }
    else { //not
        if (self->Node[pos].Link[0] != -1) {
            tCode1[*tCodePos] = 0;
            (*tCodePos)++;
            THuff_MakeHuffTree_MakeHuffCodeFromTree(self, tCode1, tCodePos, self->Node[pos].Link[0]);
        }

        if (self->Node[pos].Link[1] != -1) {
            tCode1[*tCodePos] = 1;
            (*tCodePos)++;
            THuff_MakeHuffTree_MakeHuffCodeFromTree(self, tCode1, tCodePos, self->Node[pos].Link[1]);
        }
    }

    (*tCodePos)--;
}

// creates huffman tree/codes from apparance rate (0..255)
static void THuff_MakeHuffTree(THuff* self) {
    int i, aa1, aa2, aa3;
    int tCodePos;
    uint8_t tCode1[257];
#if 0
    uint8_t tCode2[257];
#endif
    int tNode[257];

    //initializes huffman tree
    THuff_InitHuffTree(self);
    for (i = 0; i < 256; i++) {
        tNode[i] = -1;
        tCode1[i] = 0;
#if 0
        tCode2[i] = 0;
#endif
    }

    //adds child nodes + comparison target nodes
    for (i = 0; i < 256; i++) {
        tNode[i] = THuff_InsertHuffNode(self, i, self->Hed.HistGraph[i], nsLeaf, -1, -1);
    }

    //creates optimal tree
    for (i = 0; i < 256 - 1; i++) {
        //find smallest node
        aa1 = THuff_MakeHuffTree_SerchMinNode(self, tNode);
        //find value closest to smallest node
        aa2 = THuff_MakeHuffTree_SerchNearNode(self, tNode, aa1);
        //make new node joining both together
        aa3 = THuff_InsertHuffNode(self, -1, self->Node[tNode[aa1]].Weight + self->Node[tNode[aa2]].Weight, nsBranch, tNode[aa1], tNode[aa2]);
        //remove aa1/2 from comparison target nodes.
        tNode[aa1] = -1;
        tNode[aa2] = -1;
        //add created node to comparison target nodes
        tNode[aa1] = aa3;
    }

    //finally make added node top of the tree
    self->Root = aa3;

    //create stack for data expansion from tree info
    tCodePos = 0;
    THuff_MakeHuffTree_MakeHuffCodeFromTree(self, tCode1, &tCodePos, self->Root);
}

//------------------------------------------------------------------------------
//bit IO start process
static void THuff_BeginBitIO(THuff* self) {
    self->IoCount = 0;
    self->BitBuf = 0;
    self->BitCount = 32;
#if 0
    self->BitWriteLen = 0;
#endif
    self->CipherBuf = 0;
}

//------------------------------------------------------------------------------
//bit IO end process
static void THuff_EndBitIO(THuff* self) {
#if 0
    if (self->IoCount == 2 && self->BitCount > 0) {
        self->BitBuf = self->BitBuf ^ self->CipherBuf;
        TStream_Write(self->Buff, BitBuf,4);
    }
#endif
    THuff_BeginBitIO(self);
}

//------------------------------------------------------------------------------
//read 1 bit from file
static int THuff_ReadBit(THuff* self) {
    int result;
    uint32_t aaa;

    if (self->BitCount == 32) {
        self->IoCount = 1; //ReadMode
        if (self->Buff.Position < self->Buff.Size) {
            //read
            TStream_Read_Uint32(&self->Buff, &aaa); //Buff.Read(aaa,sizeof(DWORD));
            self->BitBuf = aaa ^ self->CipherBuf;

            //decryption phase
            self->CipherBuf = THuff__ROR(self->CipherBuf, aaa & 7);
            self->CipherBuf = self->CipherBuf ^ self->CipherList[aaa & 7];
        }
        self->BitCount = 0;
    }

    //return 1 bit
    result = self->BitBuf & 1;
    self->BitBuf = self->BitBuf >> 1;

    //advance BitCount
    self->BitCount++;

    return result;
}

//------------------------------------------------------------------------------
//starts reading encoded data from stream

static void TStream_Read_THuffHedState(TStream* self, THuffHedState* Hed) {
    uint8_t buf[0x410];
    int i;

    read_streamfile(buf, self->Position, sizeof(buf), self->File);
    self->Position += sizeof(buf);

    /* 0x00: string size (always 3) */
    memcpy(Hed->HedChar, buf+0x01, 0x03);
    Hed->Version = get_u32le(buf+0x04);
    for (i = 0; i < 256; i++) {
        Hed->HistGraph[i] = get_u32le(buf+0x08 + i*0x04);
    }
    Hed->FileSize = get_u64le(buf+0x408); /* seems always 0 */
}

static void THuff_BeginRead(THuff* self) {
    TStream_Read_THuffHedState(&self->Buff, &self->Hed); //Buff.Read(Hed,sizeof(THuffHedState));
    THuff_MakeHuffTree(self);
    self->BeginPos = self->Buff.Position;
    THuff_BeginBitIO(self);
    self->Mode = 1;
}

#if 0
//------------------------------------------------------------------------------
//get file size before encoding
static int64_t THuff_GetFileSize(THuff* self) {
    return self->Hed.FileSize;
}

//------------------------------------------------------------------------------
//EOF detection
static int THuff_GetEOF(THuff* self) {
    if (self->Buff.Position < self->Buff.Size)
        return CW_FALSE;
    else
        return CW_TRUE;
}
#endif
//------------------------------------------------------------------------------
//return to initial positon
static void THuff_MoveBeginPosition(THuff* self) {
    THuff_EndBitIO(self);
    self->Buff.Position = self->BeginPos;
    THuff_BeginBitIO(self);
}

//------------------------------------------------------------------------------
static void THuff_GetPositionData(THuff* self, THuffPositionData* s) {
    s->BitBuf    = self->BitBuf;
    s->BitCount  = self->BitCount;
    s->StreamPos = self->Buff.Position;
    s->CipherBuf = self->CipherBuf;
}

//------------------------------------------------------------------------------
static void THuff_SetPositionData(THuff* self, THuffPositionData* s) {
    self->BitBuf   = s->BitBuf;
    self->BitCount = s->BitCount;
    self->Buff.Position = s->StreamPos;
    self->CipherBuf = s->CipherBuf;
}

//------------------------------------------------------------------------------
static void THuff_SetCipherCode(THuff* self, uint32_t msk) {
    //creates mask list
    self->CipherList[0] = msk / 3;
    self->CipherList[1] = msk / 17;
    self->CipherList[2] = msk / 7;
    self->CipherList[3] = msk / 5;
    self->CipherList[4] = msk / 3;
    self->CipherList[5] = msk / 11;
    self->CipherList[6] = msk / 13;
    self->CipherList[7] = msk / 19;
}

//------------------------------------------------------------------------------
static uint32_t THuff__ROR(uint32_t src, uint32_t shift) {
    uint8_t num = shift % 0xFF;
    return ((uint32_t)src >> num) | ((uint32_t)src << (32 - num));
}


//-------------------------------------------------------------------
// CompressWaveLib.pas
//-------------------------------------------------------------------

#define PW_MAXVOLUME   0xFFFFFFF   //don't change


//proprietary compression file header
typedef struct {
  //RIFF chunk
  char HedChar[8];          // 'CmpWave'
  uint32_t Channel;         // 2(STEREO) / 1(MONO)
  uint32_t Sample;          // 44100Hz / 22050Hz
  uint32_t Bit;             // 16bit
  int32_t Tbl[256];         // conversion table value
  int64_t UnPressSize;      // decompressed data size
  int64_t LoopStart;        // loop start/end position
  int64_t LoopEnd;
  uint8_t LoopCount;        // loop times
  char MusicTitle[128*2];   // song name
  char MusicArtist[128*2];  // composer
} PRESSWAVEDATAHED;

//for writting
typedef struct {
    short RBuf;
    short LBuf;
} TLRWRITEBUFFER;


//compression data class
struct TCompressWaveData {
    //rendering flag (sets during rendering)
    int NowRendering;
    //flag for playback
    int32_t Faa1;
    int32_t Faa2;
    int32_t Fvv1;
    int32_t Fvv2;

    int32_t FVolume;
    int32_t Ffade;
    int32_t FSetVolume;

    int FEndLoop;
    int32_t FLoop;
    int FPlay;
    int64_t FWavePosition;
    int64_t FWaveLength;
    //flag for 22050kHz
    int32_t LBackBuf;
    int32_t RBackBuf;
    //for restoration
    THuffPositionData PosData;
    int32_t LPFaa1;
    int32_t LPFaa2;
    int32_t LPFvv1;
    int32_t LPFvv2;
    //cipher code
    uint32_t CipherCode;
    //hafu-hafu-hafuman
    THuff* RH;

#if 0
    TMemoryStream Data;
#endif
    PRESSWAVEDATAHED Hed;

};

//-----------------------------------------------------------
//create
TCompressWaveData* TCompressWaveData_Create(void) {
    TCompressWaveData* self = calloc(1, sizeof(TCompressWaveData));
    if (!self) return NULL;
#if 0
    self->Data = NULL;
#endif
    self->RH = NULL;
    self->FWavePosition = 0;
    self->FWaveLength   = 0;
    self->FVolume = PW_MAXVOLUME;
    self->FSetVolume = PW_MAXVOLUME;
    self->Ffade    = 0;
    self->FEndLoop = CW_FALSE;
    self->FPlay    = CW_FALSE;
    self->NowRendering = CW_FALSE;
    TCompressWaveData_SetCipherCode(self, 0);

    return self;
}

//-----------------------------------------------------------
//free
void TCompressWaveData_Free(TCompressWaveData* self) {
    if (!self)
        return;

    //EXTRA: presumably for threading but OG lib doesn't properly set self to false on all errors
#if 0
    //sync
    while (self->NowRendering) {
        ;
    }
#endif
    //free
    if (self->RH != NULL)
        THuff_Free(self->RH);
#if 0
    if (self->Data != NULL)
        TMemoryStream_Free(self->Data);
#endif
    free(self);
}


//-----------------------------------------------------------
//outpus 44100/16bit/stereo waveform to designed buffer

static void TCompressWaveData_Rendering_ReadPress(TCompressWaveData* self, int32_t* RFlg, int32_t* LFlg) {
    if (self->Hed.Channel == 2) {
        *RFlg = THuff_Read(self->RH);          //STEREO
        *LFlg = THuff_Read(self->RH);
    }
    else {
        *RFlg = THuff_Read(self->RH);          //MONO
        *LFlg = *RFlg;
    }
}

static void TCompressWaveData_Rendering_WriteWave(TCompressWaveData* self, int16_t** buf1, int32_t RVol, int32_t LVol) {
    TLRWRITEBUFFER bbb = {0};

    if (self->Hed.Sample == 44100) {        //44100 STEREO/MONO
        bbb.RBuf = RVol;
        bbb.LBuf = LVol;
        (*buf1)[0] = bbb.RBuf;
        (*buf1)[1] = bbb.LBuf;
        (*buf1) += 2;
    }
    if (self->Hed.Sample == 22050) {        //22050 STEREO/MONO
        bbb.RBuf = (self->RBackBuf + RVol) / 2;
        bbb.LBuf = (self->LBackBuf + LVol) / 2;
        (*buf1)[0] = bbb.RBuf;
        (*buf1)[1] = bbb.LBuf;
        (*buf1) += 2;

        bbb.RBuf = RVol;
        bbb.LBuf = LVol;
        (*buf1)[0] = bbb.RBuf;
        (*buf1)[1] = bbb.LBuf;
        (*buf1) += 2;

        self->RBackBuf = RVol;
        self->LBackBuf = LVol;
    }
}

int TCompressWaveData_Rendering(TCompressWaveData* self, int16_t* buf, uint32_t Len) {
    int result;
    int32_t RFlg, LFlg, RVol, LVol;
    int i, aaa;
    int16_t* buf1;
    int32_t PressLength, WaveStep;


    self->NowRendering = CW_TRUE;
    result = CW_FALSE;
#if 0
    if (self->Data == NULL) {
        self->NowRendering = CW_FALSE;
        return result; //exit;
    }
#endif

    //fadeout song stop
    if ((self->FVolume < 1) && (self->FSetVolume < 1)) {
        self->FPlay = CW_FALSE;
    }
    //if (abs(self->FSetVolume - self->FVolume) < self->Ffade) {
    //    self->FPlay = CW_FALSE;
    //}

    //stop if FPlay (play flag) wasn't set
    if (self->FPlay == CW_FALSE) {
        self->NowRendering = CW_FALSE;
        return result; //exit;
    }

    //pre processing
    RVol = self->Fvv1;
    LVol = self->Fvv2;
    if (self->Hed.Sample == 44100)
        WaveStep = 4;
    else
        WaveStep = 8;

    PressLength = (int32_t)Len / WaveStep;

    //expansion processing
    buf1 = buf;
    for (i = 0; i < PressLength; i++) {

        //crossed over？
        if (self->FWavePosition > self->FWaveLength) {
            if (self->FEndLoop == CW_TRUE) { //playback with loop?
                TCompressWaveData_Previous(self);
            }
            else {  //in case of playback without loop
                self->FPlay = CW_FALSE;
                return result; //exit
            }
        }

        //loop related
        if (self->Hed.LoopCount > self->FLoop) {
            //if position is loop start, hold current flag/state
            //shr 3 matches 8 bit aligment
            if ((self->Hed.LoopStart >> 3) == (self->FWavePosition >> 3)) {
                TCompressWaveData_GetLoopState(self);
            }
            //if reached loop end do loop.
            if ((self->Hed.LoopEnd >> 3) == (self->FWavePosition >> 3)) {
                if (self->Hed.LoopCount != 255)
                    self->FLoop++;
                TCompressWaveData_SetLoopState(self);
            }
        }

        //read
        TCompressWaveData_Rendering_ReadPress(self, &RFlg, &LFlg);
        self->Faa1 = self->Faa1 + self->Hed.Tbl[RFlg];
        self->Faa2 = self->Faa2 + self->Hed.Tbl[LFlg];
        self->Fvv1 = self->Fvv1 + self->Faa1;
        self->Fvv2 = self->Fvv2 + self->Faa2;

        //volume adjustment
        aaa = self->FSetVolume - self->FVolume;
        if (abs(aaa) < self->Ffade) {
            self->FVolume = self->FSetVolume;
        }
        else {
            if (aaa > 0)
                self->FVolume = self->FVolume + self->Ffade;
            else
                self->FVolume = self->FVolume - self->Ffade;
        }

        //threshold calcs (due to overflow)
        if (self->Fvv1 > +32760) {
            self->Fvv1 = +32760;
            self->Faa1 = 0;
        }
        if (self->Fvv1 < -32760) {
            self->Fvv1 = -32760;
            self->Faa1 = 0;
        }
        if (self->Fvv2 > +32760) {
            self->Fvv2 = +32760;
            self->Faa2 = 0;
        }
        if (self->Fvv2 < -32760) {
            self->Fvv2 = -32760;
            self->Faa2 = 0;
        }

        aaa = (self->FVolume >> 20);
        RVol = self->Fvv1 * aaa / 256;
        LVol = self->Fvv2 * aaa / 256;

        //expand to buffer
        TCompressWaveData_Rendering_WriteWave(self, &buf1, RVol, LVol);
        //advance playback position
        self->FWavePosition += WaveStep;
    }

    //remainder calcs
    //depending on buffer lenght remainder may happen
    //example: 44100 / 4 = 11025...OK    44100 / 8 = 5512.5...NG
    // in that case appear as noise
    if (Len % 8 == 4) {
        TCompressWaveData_Rendering_WriteWave(self, &buf1, RVol, LVol);
    }

    self->NowRendering = CW_FALSE;
    result = CW_TRUE;
    return result;
}


//-----------------------------------------------------------
//read compressed file from stream

static void TStream_Read_PRESSWAVEDATAHED(TStream* self, PRESSWAVEDATAHED* Hed) {
    uint8_t buf[0x538];
    int i, len;

    read_streamfile(buf, self->Position, sizeof(buf), self->File);
    self->Position += sizeof(buf);

    memcpy(Hed->HedChar, buf + 0x00, 8);
    Hed->Channel   = get_u32le(buf + 0x08);
    Hed->Sample    = get_u32le(buf + 0x0c);
    Hed->Bit       = get_u32le(buf + 0x10);
    for (i = 0; i < 256; i++) {
        Hed->Tbl[i] = get_s32le(buf + 0x14 + i * 0x04);
    }
    Hed->UnPressSize    = get_u64le(buf + 0x418);
    Hed->LoopStart      = get_u64le(buf + 0x420);
    Hed->LoopEnd        = get_u64le(buf + 0x428);
    Hed->LoopCount      = get_u8   (buf + 0x430);

    len = get_u8  (buf + 0x431);
    memcpy(Hed->MusicTitle, buf + 0x432, len);
    len = get_u8 (buf + 0x4B1);
    memcpy(Hed->MusicArtist, buf + 0x4B2, len);

    /* 0x538: huffman table */
    /* 0x948: data start */
}

int TCompressWaveData_LoadFromStream(TCompressWaveData* self, STREAMFILE* ss) {
    int result = CW_FALSE;
    TStream data = {0};

    if (ss == NULL)
        return result;
#if 0
    if (self->Data != NULL)
        TMemoryStream_Free(self->Data);
#endif

    data.File = ss; //data = TMemoryStream.Create;
    data.Size = get_streamfile_size(ss); //data.SetSize(ss.Size);
    //data.CopyFrom(ss,0);

    //get header info
    data.Position = 0;

    TStream_Read_PRESSWAVEDATAHED(&data, &self->Hed); //data.Read(Hed,sizeof(PRESSWAVEDATAHED));
    self->FWaveLength = self->Hed.UnPressSize;
    if (self->RH != NULL)
        THuff_Free(self->RH);
    self->RH = THuff_Create(&data);
    if (!self->RH) return result;

    THuff_SetCipherCode(self->RH, 0x00);
    THuff_BeginRead(self->RH);

    //initialize playback flag
    TCompressWaveData_Stop(self);
    TCompressWaveData_SetVolume(self, 1.0, 0.0);
    result = CW_TRUE;
    return result;
}

//------------------------------------------------------------------------------
//temp pause
void TCompressWaveData_Pause(TCompressWaveData* self) {
    self->FPlay = CW_FALSE;
}

//-----------------------------------------------------------
//sets volume
void TCompressWaveData_SetVolume(TCompressWaveData* self, float vol, float fade) {
    float aaa;

    //EXTRA: C float seemingly can't store PW_MAXVOLUME (268435455 becomes 268435456.0), so must cast to double
    // to get proper results. Otherwise volume gets slightly different vs original (no casting needed there).
    // vol=1.0, fade=0.0 is the same as default params.

    aaa = vol;
    //set volume threshold
    if (aaa > 1.0f) aaa = 1.0f;
    if (aaa < 0.0f) aaa = 0.0f;
    //calc volume increse
    if (fade < 0.01f) {  //with fade value
        self->Ffade = 0;
        self->FVolume = (int32_t)round((double)aaa * (double)PW_MAXVOLUME);
        self->FSetVolume = self->FVolume;
    }
    else {              //without fade value
        self->Ffade = (int32_t)round((double)PW_MAXVOLUME / (double)fade / 44100);
        self->FSetVolume = (int32_t)round((double)aaa * (double)PW_MAXVOLUME);
    }
}

//-----------------------------------------------------------
//returns fade value
float TCompressWaveData_GetFade(TCompressWaveData* self) {
    if ((self->Ffade == 0) || (abs(self->FVolume - self->FSetVolume) == 0)) {
        return 0; //exit;
    }
    return (abs(self->FVolume - self->FSetVolume)/44100) / self->Ffade;
}

//-----------------------------------------------------------
//returns volume value
float TCompressWaveData_GetVolume(TCompressWaveData* self) {
    return self->FVolume / PW_MAXVOLUME;
}

//------------------------------------------------------------------------------
//returns volume after fade
float TCompressWaveData_GetSetVolume(TCompressWaveData* self) {
    return self->FSetVolume / PW_MAXVOLUME;
}

//------------------------------------------------------------------------------
//returns play time (current position). unit is secs
float TCompressWaveData_GetPlayTime(TCompressWaveData* self) {
    return self->FWavePosition / (44100*4);
}

//-----------------------------------------------------------
//returns song length. unit is secs
float TCompressWaveData_GetTotalTime(TCompressWaveData* self) {
    return self->FWaveLength / (44100*4);
}

//-----------------------------------------------------------
//play stop command. returns song to beginning
void TCompressWaveData_Stop(TCompressWaveData* self) {
    //play flags to initial state
    self->FWavePosition = 0;
    self->Fvv1 = 0;
    self->Faa1 = 0;
    self->Fvv2 = 0;
    self->Faa2 = 0;
    self->LBackBuf = 0;
    self->RBackBuf = 0;
    TCompressWaveData_SetVolume(self, 1.0, 0);
    self->FPlay = CW_FALSE;
    self->FLoop = 0;
#if 0
    if (self->Data == NULL)
        return;
#endif

    //EXTRA: presumably for threading but OG lib doesn't properly set self to false on all errors
#if 0
    //sync
    while (self->NowRendering) {
        ;
    }
#endif

    //return to initial location
    THuff_MoveBeginPosition(self->RH);
}

//-----------------------------------------------------------
//returns song to beginning. difference vs STOP is that fade isn't initialized
void TCompressWaveData_Previous(TCompressWaveData* self) {
    //play flags to initial state
    self->FWavePosition = 0;
    self->Fvv1 = 0;
    self->Faa1 = 0;
    self->Fvv2 = 0;
    self->Faa2 = 0;
    self->LBackBuf = 0;
    self->RBackBuf = 0;
    self->FLoop = 0;

#if 0
    if (self->Data == NULL)
        return;
#endif
    //return to initial location
    THuff_MoveBeginPosition(self->RH);
}

//------------------------------------------------------------
//starts song playback
void TCompressWaveData_Play(TCompressWaveData* self, int loop) {
    self->FPlay    = CW_TRUE;
    self->FEndLoop = loop;
    if ((self->FVolume == 0) && (self->FSetVolume == 0))
        TCompressWaveData_SetVolume(self, 1.0,0);
}

//------------------------------------------------------------
//set parameters for looping
//--------------------------------------------------------------
//record encoded file position
//since it uses huffman needs to held those flags too
void TCompressWaveData_GetLoopState(TCompressWaveData* self) {
    self->LPFaa1 = self->Faa1;
    self->LPFaa2 = self->Faa2;
    self->LPFvv1 = self->Fvv1;
    self->LPFvv2 = self->Fvv2;
    THuff_GetPositionData(self->RH, &self->PosData);
}

//--------------------------------------------------------------
//return to recorded encoded file position
void TCompressWaveData_SetLoopState(TCompressWaveData* self) {
    self->Faa1 = self->LPFaa1;
    self->Faa2 = self->LPFaa2;
    self->Fvv1 = self->LPFvv1;
    self->Fvv2 = self->LPFvv2;
    THuff_SetPositionData(self->RH, &self->PosData);
    self->FWavePosition = self->Hed.LoopStart;
}

//-----------------------------------------------------------
//sets cipher code
void TCompressWaveData_SetCipherCode(TCompressWaveData* self, uint32_t Num) {
    self->CipherCode = Num;
}

