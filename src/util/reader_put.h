#ifndef _READER_PUT_H
#define _READER_PUT_H

#include <stdint.h>

void put_u8(uint8_t* buf, uint8_t v);
void put_u16le(uint8_t* buf, uint16_t v);
void put_u32le(uint8_t* buf, uint32_t v);
void put_u16be(uint8_t* buf, uint16_t v);
void put_u32be(uint8_t* buf, uint32_t v);

void put_s8(uint8_t* buf, int8_t v);
void put_s16le(uint8_t* buf, int16_t v);
void put_s32le(uint8_t* buf, int32_t v);
void put_s16be(uint8_t* buf, int16_t v);
void put_s32be(uint8_t* buf, int32_t v);

void put_data(uint8_t* buf, void* v, int v_size);

#endif
