#include "compresswave_decoder_lib.h"
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

static void TStream_Read_Uint32(TStream* this, uint32_t* value) {
    uint8_t buf[0x4] = {0};

    read_streamfile(buf, this->Position, sizeof(buf), this->File);
    this->Position += 0x4;

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
static void THuff_InitHuffTree(THuff* this); //initializes tree
static int THuff_InsertHuffNode(THuff* this, int v, int w, TNodeState s, int b1, int b2); //add node to tree
static void THuff_MakeHuffTree(THuff* this);

//related to single bit IO
static void THuff_BeginBitIO(THuff* this);
static void THuff_EndBitIO(THuff* this);
static int THuff_ReadBit(THuff* this);
static uint32_t THuff__ROR(uint32_t src, uint32_t shift);

static THuff* THuff_Create(TStream* buf); // creation
static void THuff_Free(THuff* this); // release the power
static void THuff_SetCipherCode(THuff* this, uint32_t msk); // encryption mask bits
//functions for reading
static void THuff_BeginRead(THuff* this);
static int THuff_Read(THuff* this);

#if 0
static int64_t THuff_GetFileSize(THuff* this); // get file size before encoding
static int THuff_GetEOF(THuff* this); // EOF detection
#endif
static void THuff_MoveBeginPosition(THuff* this); // return to initial state
static void THuff_GetPositionData(THuff* this, THuffPositionData* s);   // secret
static void THuff_SetPositionData(THuff* this, THuffPositionData* s);

//------------------------------------------------------------------------------
//create
static THuff* THuff_Create(TStream* buf) {
    THuff* this = malloc(sizeof(THuff));
    if (!this) return NULL;

    //define stream
    this->Buff = *buf;

    //initialization
    THuff_InitHuffTree(this);
    memcpy(this->Hed.HedChar, "HUF\0", 0x4);
    this->Hed.Version  = 1;
    this->Hed.FileSize = 0;

    //set cipher bits
    this->CipherBuf = 0;
    THuff_SetCipherCode(this, 0x0);

    //mode
    this->Mode = 0;

    return this;
}

//------------------------------------------------------------------------------
//free
static void THuff_Free(THuff* this) {
    if (this == NULL) return;
    if (this->Mode == 2)
        THuff_EndBitIO(this);
    free(this);
}

//------------------------------------------------------------------------------
//init tree structure (unused state)
static void THuff_InitHuffTree(THuff* this) {
    int i;

    for (i = 0; i < 512; i++) {
        this->Node[i].State = nsEmpty;
    }
}

//------------------------------------------------------------------------------
//add node to huffman tree
static int THuff_InsertHuffNode(THuff* this, int v, int w, TNodeState s, int b1, int b2) {
    int result = 0;
    int i;

    i = 0;
    while ((this->Node[i].State != nsEmpty) && (i < 512)) {
        i++;
    }

    if (i == 512) {
        result = -1;
        return result; //exit;
    }

    this->Node[i].Value = v & 0xFF; //BYTE(v);
    this->Node[i].Weight = w;
    this->Node[i].State  = s;
    this->Node[i].Link[0] = b1;
    if (this->Node[i].Link[0] > 511) {
        return -1;//? //halt;
    }
    this->Node[i].Link[1] = b2;
    if (this->Node[i].Link[1] > 511) {
        return -1;//? //halt;
    }
    //return entry number
    result = i;
    return result;
}

//------------------------------------------------------------------------------
//reads and expands huffman-encoded data
static int THuff_Read(THuff* this) {
    int i;

    i = this->Root;
    while (this->Node[i].State != nsLeaf) {
        i = this->Node[i].Link[THuff_ReadBit(this)];
    }

    return this->Node[i].Value;
}

//------------------------------------------------------------------------------
//creates fork code from tree

//finds node of lowest weight
static int THuff_MakeHuffTree_SerchMinNode(THuff* this, int* tNode) {
    int ii, aaa1, aaa2;

    aaa1 = 0xFFFFFFF;
    aaa2 = 0;
    for (ii = 0 ; ii < 256; ii++) {
        if (tNode[ii] != -1) {
            if (this->Node[tNode[ii]].Weight < aaa1) {
                aaa2 = ii;
                aaa1 = this->Node[tNode[ii]].Weight;
            }
        }
    }
    return aaa2;
}

//finds closest node
static int THuff_MakeHuffTree_SerchNearNode(THuff* this, int* tNode, int pos) {
    int ii, aaa1, aaa2;

    aaa1 = 0xFFFFFFF;
    aaa2 = 0;
    for (ii = 0 ; ii < 256; ii++) {
        if (tNode[ii] != -1) {
            if ((abs(this->Node[tNode[ii]].Weight - this->Node[tNode[pos]].Weight) < aaa1) && (pos != ii)) {
                aaa2 = ii;
                aaa1 = this->Node[tNode[ii]].Weight;
            }
        }
    }
    return aaa2;
}

static void THuff_MakeHuffTree_MakeHuffCodeFromTree(THuff* this, uint8_t* tCode1, int* tCodePos, int pos) {
    int ii, aaa1;

    if (this->Node[pos].State == nsLeaf) { //found
        tCode1[*tCodePos] = 0xFF;
        aaa1 = this->Node[pos].Value;
        for (ii = 0; ii < 256; ii++) {
            this->Code[aaa1][ii] = tCode1[ii];
        }
    }
    else { //not
        if (this->Node[pos].Link[0] != -1) {
            tCode1[*tCodePos] = 0;
            (*tCodePos)++;
            THuff_MakeHuffTree_MakeHuffCodeFromTree(this, tCode1, tCodePos, this->Node[pos].Link[0]);
        }

        if (this->Node[pos].Link[1] != -1) {
            tCode1[*tCodePos] = 1;
            (*tCodePos)++;
            THuff_MakeHuffTree_MakeHuffCodeFromTree(this, tCode1, tCodePos, this->Node[pos].Link[1]);
        }
    }

    (*tCodePos)--;
}

// creates huffman tree/codes from apparance rate (0..255)
static void THuff_MakeHuffTree(THuff* this) {
    int i, aa1, aa2, aa3;
    int tCodePos;
    uint8_t tCode1[257];
#if 0
    uint8_t tCode2[257];
#endif
    int tNode[257];

    //initializes huffman tree
    THuff_InitHuffTree(this);
    for (i = 0; i < 256; i++) {
        tNode[i] = -1;
        tCode1[i] = 0;
#if 0
        tCode2[i] = 0;
#endif
    }

    //adds child nodes + comparison target nodes
    for (i = 0; i < 256; i++) {
        tNode[i] = THuff_InsertHuffNode(this, i, this->Hed.HistGraph[i], nsLeaf, -1, -1);
    }

    //creates optimal tree
    for (i = 0; i < 256 - 1; i++) {
        //find smallest node
        aa1 = THuff_MakeHuffTree_SerchMinNode(this, tNode);
        //find value closest to smallest node
        aa2 = THuff_MakeHuffTree_SerchNearNode(this, tNode, aa1);
        //make new node joining both together
        aa3 = THuff_InsertHuffNode(this, -1, this->Node[tNode[aa1]].Weight + this->Node[tNode[aa2]].Weight, nsBranch, tNode[aa1], tNode[aa2]);
        //remove aa1/2 from comparison target nodes.
        tNode[aa1] = -1;
        tNode[aa2] = -1;
        //add created node to comparison target nodes
        tNode[aa1] = aa3;
    }

    //finally make added node top of the tree
    this->Root = aa3;

    //create stack for data expansion from tree info
    tCodePos = 0;
    THuff_MakeHuffTree_MakeHuffCodeFromTree(this, tCode1, &tCodePos, this->Root);
}

//------------------------------------------------------------------------------
//bit IO start process
static void THuff_BeginBitIO(THuff* this) {
    this->IoCount = 0;
    this->BitBuf = 0;
    this->BitCount = 32;
#if 0
    this->BitWriteLen = 0;
#endif
    this->CipherBuf = 0;
}

//------------------------------------------------------------------------------
//bit IO end process
static void THuff_EndBitIO(THuff* this) {
#if 0
    if (this->IoCount == 2 && this->BitCount > 0) {
        this->BitBuf = this->BitBuf ^ this->CipherBuf;
        TStream_Write(this->Buff, BitBuf,4);
    }
#endif
    THuff_BeginBitIO(this);
}

//------------------------------------------------------------------------------
//read 1 bit from file
static int THuff_ReadBit(THuff* this) {
    int result;
    uint32_t aaa;

    if (this->BitCount == 32) {
        this->IoCount = 1; //ReadMode
        if (this->Buff.Position < this->Buff.Size) {
            //read
            TStream_Read_Uint32(&this->Buff, &aaa); //Buff.Read(aaa,sizeof(DWORD));
            this->BitBuf = aaa ^ this->CipherBuf;

            //decryption phase
            this->CipherBuf = THuff__ROR(this->CipherBuf, aaa & 7);
            this->CipherBuf = this->CipherBuf ^ this->CipherList[aaa & 7];
        }
        this->BitCount = 0;
    }

    //return 1 bit
    result = this->BitBuf & 1;
    this->BitBuf = this->BitBuf >> 1;

    //advance BitCount
    this->BitCount++;

    return result;
}

//------------------------------------------------------------------------------
//starts reading encoded data from stream

static void TStream_Read_THuffHedState(TStream* this, THuffHedState* Hed) {
    uint8_t buf[0x410];
    int i;

    read_streamfile(buf, this->Position, sizeof(buf), this->File);
    this->Position += sizeof(buf);

    /* 0x00: string size (always 3) */
    memcpy(Hed->HedChar, buf+0x01, 0x03);
    Hed->Version = get_u32le(buf+0x04);
    for (i = 0; i < 256; i++) {
        Hed->HistGraph[i] = get_u32le(buf+0x08 + i*0x04);
    }
    Hed->FileSize = get_u64le(buf+0x408); /* seems always 0 */
}

static void THuff_BeginRead(THuff* this) {
    TStream_Read_THuffHedState(&this->Buff, &this->Hed); //Buff.Read(Hed,sizeof(THuffHedState));
    THuff_MakeHuffTree(this);
    this->BeginPos = this->Buff.Position;
    THuff_BeginBitIO(this);
    this->Mode = 1;
}

#if 0
//------------------------------------------------------------------------------
//get file size before encoding
static int64_t THuff_GetFileSize(THuff* this) {
    return this->Hed.FileSize;
}

//------------------------------------------------------------------------------
//EOF detection
static int THuff_GetEOF(THuff* this) {
    if (this->Buff.Position < this->Buff.Size)
        return CW_FALSE;
    else
        return CW_TRUE;
}
#endif
//------------------------------------------------------------------------------
//return to initial positon
static void THuff_MoveBeginPosition(THuff* this) {
    THuff_EndBitIO(this);
    this->Buff.Position = this->BeginPos;
    THuff_BeginBitIO(this);
}

//------------------------------------------------------------------------------
static void THuff_GetPositionData(THuff* this, THuffPositionData* s) {
    s->BitBuf    = this->BitBuf;
    s->BitCount  = this->BitCount;
    s->StreamPos = this->Buff.Position;
    s->CipherBuf = this->CipherBuf;
}

//------------------------------------------------------------------------------
static void THuff_SetPositionData(THuff* this, THuffPositionData* s) {
    this->BitBuf   = s->BitBuf;
    this->BitCount = s->BitCount;
    this->Buff.Position = s->StreamPos;
    this->CipherBuf = s->CipherBuf;
}

//------------------------------------------------------------------------------
static void THuff_SetCipherCode(THuff* this, uint32_t msk) {
    //creates mask list
    this->CipherList[0] = msk / 3;
    this->CipherList[1] = msk / 17;
    this->CipherList[2] = msk / 7;
    this->CipherList[3] = msk / 5;
    this->CipherList[4] = msk / 3;
    this->CipherList[5] = msk / 11;
    this->CipherList[6] = msk / 13;
    this->CipherList[7] = msk / 19;
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
    TCompressWaveData* this = malloc(sizeof(TCompressWaveData));
    if (!this) return NULL;
#if 0
    this->Data = NULL;
#endif
    this->RH = NULL;
    this->FWavePosition = 0;
    this->FWaveLength   = 0;
    this->FVolume = PW_MAXVOLUME;
    this->FSetVolume = PW_MAXVOLUME;
    this->Ffade    = 0;
    this->FEndLoop = CW_FALSE;
    this->FPlay    = CW_FALSE;
    this->NowRendering = CW_FALSE;
    TCompressWaveData_SetCipherCode(this, 0);

    return this;
}

//-----------------------------------------------------------
//free
void TCompressWaveData_Free(TCompressWaveData* this) {
    if (!this)
        return;

    //EXTRA: presumably for threading but OG lib doesn't properly set this to false on all errors
#if 0
    //sync
    while (this->NowRendering) {
        ;
    }
#endif
    //free
    if (this->RH != NULL)
        THuff_Free(this->RH);
#if 0
    if (this->Data != NULL)
        TMemoryStream_Free(this->Data);
#endif
    free(this);
}


//-----------------------------------------------------------
//outpus 44100/16bit/stereo waveform to designed buffer

static void TCompressWaveData_Rendering_ReadPress(TCompressWaveData* this, int32_t* RFlg, int32_t* LFlg) {
    if (this->Hed.Channel == 2) {
        *RFlg = THuff_Read(this->RH);          //STEREO
        *LFlg = THuff_Read(this->RH);
    }
    else {
        *RFlg = THuff_Read(this->RH);          //MONO
        *LFlg = *RFlg;
    }
}

static void TCompressWaveData_Rendering_WriteWave(TCompressWaveData* this, int16_t** buf1, int32_t RVol, int32_t LVol) {
    TLRWRITEBUFFER bbb = {0};

    if (this->Hed.Sample == 44100) {        //44100 STEREO/MONO
        bbb.RBuf = RVol;
        bbb.LBuf = LVol;
        (*buf1)[0] = bbb.RBuf;
        (*buf1)[1] = bbb.LBuf;
        (*buf1) += 2;
    }
    if (this->Hed.Sample == 22050) {        //22050 STEREO/MONO
        bbb.RBuf = (this->RBackBuf + RVol) / 2;
        bbb.LBuf = (this->LBackBuf + LVol) / 2;
        (*buf1)[0] = bbb.RBuf;
        (*buf1)[1] = bbb.LBuf;
        (*buf1) += 2;

        bbb.RBuf = RVol;
        bbb.LBuf = LVol;
        (*buf1)[0] = bbb.RBuf;
        (*buf1)[1] = bbb.LBuf;
        (*buf1) += 2;

        this->RBackBuf = RVol;
        this->LBackBuf = LVol;
    }
}

int TCompressWaveData_Rendering(TCompressWaveData* this, int16_t* buf, uint32_t Len) {
    int result;
    int32_t RFlg, LFlg, RVol, LVol;
    int i, aaa;
    int16_t* buf1;
    int32_t PressLength, WaveStep;


    this->NowRendering = CW_TRUE;
    result = CW_FALSE;
#if 0
    if (this->Data == NULL) {
        this->NowRendering = CW_FALSE;
        return result; //exit;
    }
#endif

    //fadeout song stop
    if ((this->FVolume < 1) && (this->FSetVolume < 1)) {
        this->FPlay = CW_FALSE;
    }
    //if (abs(this->FSetVolume - this->FVolume) < this->Ffade) {
    //    this->FPlay = CW_FALSE;
    //}

    //stop if FPlay (play flag) wasn't set
    if (this->FPlay == CW_FALSE) {
        this->NowRendering = CW_FALSE;
        return result; //exit;
    }

    //pre processing
    RVol = this->Fvv1;
    LVol = this->Fvv2;
    if (this->Hed.Sample == 44100)
        WaveStep = 4;
    else
        WaveStep = 8;

    PressLength = (int32_t)Len / WaveStep;

    //expansion processing
    buf1 = buf;
    for (i = 0; i < PressLength; i++) {

        //crossed over？
        if (this->FWavePosition > this->FWaveLength) {
            if (this->FEndLoop == CW_TRUE) { //playback with loop?
                TCompressWaveData_Previous(this);
            }
            else {  //in case of playback without loop
                this->FPlay = CW_FALSE;
                return result; //exit
            }
        }

        //loop related
        if (this->Hed.LoopCount > this->FLoop) {
            //if position is loop start, hold current flag/state
            //shr 3 matches 8 bit aligment
            if ((this->Hed.LoopStart >> 3) == (this->FWavePosition >> 3)) {
                TCompressWaveData_GetLoopState(this);
            }
            //if reached loop end do loop.
            if ((this->Hed.LoopEnd >> 3) == (this->FWavePosition >> 3)) {
                if (this->Hed.LoopCount != 255)
                    this->FLoop++;
                TCompressWaveData_SetLoopState(this);
            }
        }

        //read
        TCompressWaveData_Rendering_ReadPress(this, &RFlg, &LFlg);
        this->Faa1 = this->Faa1 + this->Hed.Tbl[RFlg];
        this->Faa2 = this->Faa2 + this->Hed.Tbl[LFlg];
        this->Fvv1 = this->Fvv1 + this->Faa1;
        this->Fvv2 = this->Fvv2 + this->Faa2;

        //volume adjustment
        aaa = this->FSetVolume - this->FVolume;
        if (abs(aaa) < this->Ffade) {
            this->FVolume = this->FSetVolume;
        }
        else {
            if (aaa > 0)
                this->FVolume = this->FVolume + this->Ffade;
            else
                this->FVolume = this->FVolume - this->Ffade;
        }

        //threshold calcs (due to overflow)
        if (this->Fvv1 > +32760) {
            this->Fvv1 = +32760;
            this->Faa1 = 0;
        }
        if (this->Fvv1 < -32760) {
            this->Fvv1 = -32760;
            this->Faa1 = 0;
        }
        if (this->Fvv2 > +32760) {
            this->Fvv2 = +32760;
            this->Faa2 = 0;
        }
        if (this->Fvv2 < -32760) {
            this->Fvv2 = -32760;
            this->Faa2 = 0;
        }

        aaa = (this->FVolume >> 20);
        RVol = this->Fvv1 * aaa / 256;
        LVol = this->Fvv2 * aaa / 256;

        //expand to buffer
        TCompressWaveData_Rendering_WriteWave(this, &buf1, RVol, LVol);
        //advance playback position
        this->FWavePosition += WaveStep;
    }

    //remainder calcs
    //depending on buffer lenght remainder may happen
    //example: 44100 / 4 = 11025...OK    44100 / 8 = 5512.5...NG
    // in that case appear as noise
    if (Len % 8 == 4) {
        TCompressWaveData_Rendering_WriteWave(this, &buf1, RVol, LVol);
    }

    this->NowRendering = CW_FALSE;
    result = CW_TRUE;
    return result;
}


//-----------------------------------------------------------
//read compressed file from stream

static void TStream_Read_PRESSWAVEDATAHED(TStream* this, PRESSWAVEDATAHED* Hed) {
    uint8_t buf[0x538];
    int i, len;

    read_streamfile(buf, this->Position, sizeof(buf), this->File);
    this->Position += sizeof(buf);

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

int TCompressWaveData_LoadFromStream(TCompressWaveData* this, STREAMFILE* ss) {
    int result = CW_FALSE;
    TStream data = {0};

    if (ss == NULL)
        return result;
#if 0
    if (this->Data != NULL)
        TMemoryStream_Free(this->Data);
#endif

    data.File = ss; //data = TMemoryStream.Create;
    data.Size = get_streamfile_size(ss); //data.SetSize(ss.Size);
    //data.CopyFrom(ss,0);

    //get header info
    data.Position = 0;

    TStream_Read_PRESSWAVEDATAHED(&data, &this->Hed); //data.Read(Hed,sizeof(PRESSWAVEDATAHED));
    this->FWaveLength = this->Hed.UnPressSize;
    if (this->RH != NULL)
        THuff_Free(this->RH);
    this->RH = THuff_Create(&data);
    if (!this->RH) return result;

    THuff_SetCipherCode(this->RH, 0x00);
    THuff_BeginRead(this->RH);

    //initialize playback flag
    TCompressWaveData_Stop(this);
    TCompressWaveData_SetVolume(this, 1.0, 0.0);
    result = CW_TRUE;
    return result;
}

//------------------------------------------------------------------------------
//temp pause
void TCompressWaveData_Pause(TCompressWaveData* this) {
    this->FPlay = CW_FALSE;
}

//-----------------------------------------------------------
//sets volume
void TCompressWaveData_SetVolume(TCompressWaveData* this, float vol, float fade) {
    float aaa;

    //EXTRA: C float seemingly can't store PW_MAXVOLUME (268435455 becomes 268435456.0), so must cast to double
    // to get proper results. Otherwise volume gets slightly different vs original (no casting needed there).
    // vol=1.0, fade=0.0 is the same as default params.

    aaa = vol;
    //set volume threshold
    if (aaa > 1.0) aaa = 1.0;
    if (aaa < 0.0) aaa = 0.0;
    //calc volume increse
    if (fade < 0.01) {  //with fade value
        this->Ffade = 0;
        this->FVolume = round(aaa * (double)PW_MAXVOLUME);
        this->FSetVolume = this->FVolume;
    }
    else {              //without fade value
        this->Ffade = round((double)PW_MAXVOLUME / fade / 44100);
        this->FSetVolume = round(aaa * (double)PW_MAXVOLUME);
    }
}

//-----------------------------------------------------------
//returns fade value
float TCompressWaveData_GetFade(TCompressWaveData* this) {
    if ((this->Ffade == 0) || (abs(this->FVolume - this->FSetVolume) == 0)) {
        return 0; //exit;
    }
    return (abs(this->FVolume - this->FSetVolume)/44100) / this->Ffade;
}

//-----------------------------------------------------------
//returns volume value
float TCompressWaveData_GetVolume(TCompressWaveData* this) {
    return this->FVolume / PW_MAXVOLUME;
}

//------------------------------------------------------------------------------
//returns volume after fade
float TCompressWaveData_GetSetVolume(TCompressWaveData* this) {
    return this->FSetVolume / PW_MAXVOLUME;
}

//------------------------------------------------------------------------------
//returns play time (current position). unit is secs
float TCompressWaveData_GetPlayTime(TCompressWaveData* this) {
    return this->FWavePosition / (44100*4);
}

//-----------------------------------------------------------
//returns song length. unit is secs
float TCompressWaveData_GetTotalTime(TCompressWaveData* this) {
    return this->FWaveLength / (44100*4);
}

//-----------------------------------------------------------
//play stop command. returns song to beginning
void TCompressWaveData_Stop(TCompressWaveData* this) {
    //play flags to initial state
    this->FWavePosition = 0;
    this->Fvv1 = 0;
    this->Faa1 = 0;
    this->Fvv2 = 0;
    this->Faa2 = 0;
    this->LBackBuf = 0;
    this->RBackBuf = 0;
    TCompressWaveData_SetVolume(this, 1.0, 0);
    this->FPlay = CW_FALSE;
    this->FLoop = 0;
#if 0
    if (this->Data == NULL)
        return;
#endif

    //EXTRA: presumably for threading but OG lib doesn't properly set this to false on all errors
#if 0
    //sync
    while (this->NowRendering) {
        ;
    }
#endif

    //return to initial location
    THuff_MoveBeginPosition(this->RH);
}

//-----------------------------------------------------------
//returns song to beginning. difference vs STOP is that fade isn't initialized
void TCompressWaveData_Previous(TCompressWaveData* this) {
    //play flags to initial state
    this->FWavePosition = 0;
    this->Fvv1 = 0;
    this->Faa1 = 0;
    this->Fvv2 = 0;
    this->Faa2 = 0;
    this->LBackBuf = 0;
    this->RBackBuf = 0;
    this->FLoop = 0;

#if 0
    if (this->Data == NULL)
        return;
#endif
    //return to initial location
    THuff_MoveBeginPosition(this->RH);
}

//------------------------------------------------------------
//starts song playback
void TCompressWaveData_Play(TCompressWaveData* this, int loop) {
    this->FPlay    = CW_TRUE;
    this->FEndLoop = loop;
    if ((this->FVolume == 0) && (this->FSetVolume == 0))
        TCompressWaveData_SetVolume(this, 1.0,0);
}

//------------------------------------------------------------
//set parameters for looping
//--------------------------------------------------------------
//record encoded file position
//since it uses huffman needs to held those flags too
void TCompressWaveData_GetLoopState(TCompressWaveData* this) {
    this->LPFaa1 = this->Faa1;
    this->LPFaa2 = this->Faa2;
    this->LPFvv1 = this->Fvv1;
    this->LPFvv2 = this->Fvv2;
    THuff_GetPositionData(this->RH, &this->PosData);
}

//--------------------------------------------------------------
//return to recorded encoded file position
void TCompressWaveData_SetLoopState(TCompressWaveData* this) {
    this->Faa1 = this->LPFaa1;
    this->Faa2 = this->LPFaa2;
    this->Fvv1 = this->LPFvv1;
    this->Fvv2 = this->LPFvv2;
    THuff_SetPositionData(this->RH, &this->PosData);
    this->FWavePosition = this->Hed.LoopStart;
}

//-----------------------------------------------------------
//sets cipher code
void TCompressWaveData_SetCipherCode(TCompressWaveData* this, uint32_t Num) {
    this->CipherCode = Num;
}

