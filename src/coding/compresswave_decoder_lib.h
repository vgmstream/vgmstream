#ifndef _COMPRESSWAVE_DECODER_LIB_H
#define _COMPRESSWAVE_DECODER_LIB_H
#include "../streamfile.h"

typedef struct TCompressWaveData TCompressWaveData;

void TCompressWaveData_GetLoopState(TCompressWaveData* this);
void TCompressWaveData_SetLoopState(TCompressWaveData* this);

TCompressWaveData* TCompressWaveData_Create(void);
void TCompressWaveData_Free(TCompressWaveData* this);
int TCompressWaveData_Rendering(TCompressWaveData* this, int16_t* buf, uint32_t Len);
int TCompressWaveData_LoadFromStream(TCompressWaveData* this, STREAMFILE* ss);
void TCompressWaveData_SetCipherCode(TCompressWaveData* this, uint32_t Num);

void TCompressWaveData_Play(TCompressWaveData* this, int loop);
void TCompressWaveData_Stop(TCompressWaveData* this);
void TCompressWaveData_Previous(TCompressWaveData* this);
void TCompressWaveData_Pause(TCompressWaveData* this);
void TCompressWaveData_SetVolume(TCompressWaveData* this, float vol, float fade);
float TCompressWaveData_GetVolume(TCompressWaveData* this);
float TCompressWaveData_GetSetVolume(TCompressWaveData* this);
float TCompressWaveData_GetFade(TCompressWaveData* this);
float TCompressWaveData_GetPlayTime(TCompressWaveData* this);
float TCompressWaveData_GetTotalTime(TCompressWaveData* this);

#endif /*_COMPRESSWAVE_DECODER_LIB_H */
