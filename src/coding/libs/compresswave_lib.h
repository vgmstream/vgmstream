#ifndef _COMPRESSWAVE_LIB_H
#define _COMPRESSWAVE_LIB_H
#include "../../streamfile.h"

typedef struct TCompressWaveData TCompressWaveData;

void TCompressWaveData_GetLoopState(TCompressWaveData* self);
void TCompressWaveData_SetLoopState(TCompressWaveData* self);

TCompressWaveData* TCompressWaveData_Create(void);
void TCompressWaveData_Free(TCompressWaveData* self);
int TCompressWaveData_Rendering(TCompressWaveData* self, int16_t* buf, uint32_t Len);
int TCompressWaveData_LoadFromStream(TCompressWaveData* self, STREAMFILE* ss);
void TCompressWaveData_SetCipherCode(TCompressWaveData* self, uint32_t Num);

void TCompressWaveData_Play(TCompressWaveData* self, int loop);
void TCompressWaveData_Stop(TCompressWaveData* self);
void TCompressWaveData_Previous(TCompressWaveData* self);
void TCompressWaveData_Pause(TCompressWaveData* self);
void TCompressWaveData_SetVolume(TCompressWaveData* self, float vol, float fade);
float TCompressWaveData_GetVolume(TCompressWaveData* self);
float TCompressWaveData_GetSetVolume(TCompressWaveData* self);
float TCompressWaveData_GetFade(TCompressWaveData* self);
float TCompressWaveData_GetPlayTime(TCompressWaveData* self);
float TCompressWaveData_GetTotalTime(TCompressWaveData* self);

#endif /*_COMPRESSWAVE_LIB_H */
