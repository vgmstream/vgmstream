#include <string.h> // memcpy
#include "reader_put.h"

void put_u8(uint8_t* buf, uint8_t v) {
    buf[0] = v;
}

void put_u16le(uint8_t* buf, uint16_t v) {
    buf[0] = (uint8_t)((v >>  0) & 0xFF);
    buf[1] = (uint8_t)((v >>  8) & 0xFF);
}

void put_u32le(uint8_t* buf, uint32_t v) {
    buf[0] = (uint8_t)((v >>  0) & 0xFF);
    buf[1] = (uint8_t)((v >>  8) & 0xFF);
    buf[2] = (uint8_t)((v >> 16) & 0xFF);
    buf[3] = (uint8_t)((v >> 24) & 0xFF);
}

void put_u16be(uint8_t* buf, uint16_t v) {
    buf[0] = (uint8_t)((v >>  8) & 0xFF);
    buf[1] = (uint8_t)((v >>  0) & 0xFF);
}

void put_u32be(uint8_t* buf, uint32_t v) {
    buf[0] = (uint8_t)((v >> 24) & 0xFF);
    buf[1] = (uint8_t)((v >> 16) & 0xFF);
    buf[2] = (uint8_t)((v >>  8) & 0xFF);
    buf[3] = (uint8_t)((v >>  0) & 0xFF);
}

void put_s8(uint8_t* buf, int8_t v) {
    put_u8(buf, v);
}

void put_s16le(uint8_t* buf, int16_t v) {
    put_u16le(buf, v);
}

void put_s32le(uint8_t* buf, int32_t v) {
    put_u32le(buf, v);
}

void put_s16be(uint8_t* buf, int16_t v) {
    put_u16be(buf, v);
}

void put_s32be(uint8_t* buf, int32_t v) {
    put_u32be(buf, v);
}

void put_data(uint8_t* buf, void* v, int v_size) {
    memcpy   (buf, v, v_size);
}
