#include "reader_put.h"

void put_8bit(uint8_t * buf, int8_t i) {
    buf[0] = i;
}

void put_16bitLE(uint8_t * buf, int16_t i) {
    buf[0] = (i & 0xFF);
    buf[1] = i >> 8;
}

void put_32bitLE(uint8_t * buf, int32_t i) {
    buf[0] = (uint8_t)(i & 0xFF);
    buf[1] = (uint8_t)((i >> 8) & 0xFF);
    buf[2] = (uint8_t)((i >> 16) & 0xFF);
    buf[3] = (uint8_t)((i >> 24) & 0xFF);
}

void put_16bitBE(uint8_t * buf, int16_t i) {
    buf[0] = i >> 8;
    buf[1] = (i & 0xFF);
}

void put_32bitBE(uint8_t * buf, int32_t i) {
    buf[0] = (uint8_t)((i >> 24) & 0xFF);
    buf[1] = (uint8_t)((i >> 16) & 0xFF);
    buf[2] = (uint8_t)((i >> 8) & 0xFF);
    buf[3] = (uint8_t)(i & 0xFF);
}
