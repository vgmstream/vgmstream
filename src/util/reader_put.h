#ifndef _READER_PUT_H
#define _READER_PUT_H

#include "../streamtypes.h"


void put_8bit(uint8_t* buf, int8_t i);
void put_16bitLE(uint8_t* buf, int16_t i);
void put_32bitLE(uint8_t* buf, int32_t i);
void put_16bitBE(uint8_t* buf, int16_t i);
void put_32bitBE(uint8_t* buf, int32_t i);

/* alias of the above */ //TODO: improve
#define put_u8 put_8bit
#define put_u16le put_16bitLE
#define put_u32le put_32bitLE
#define put_u16be put_16bitBE
#define put_u32be put_32bitBE
#define put_s8 put_8bit
#define put_s16le put_16bitLE
#define put_s32le put_32bitLE
#define put_s16be put_16bitBE
#define put_s32be put_32bitBE

#endif
