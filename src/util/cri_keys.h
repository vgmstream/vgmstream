#ifndef _CRI_KEYS_H_
#define _CRI_KEYS_H_

#include <stdint.h>

/* common CRI key helpers */

void cri_key8_derive(const char* key8, uint16_t* p_key1, uint16_t* p_key2, uint16_t* p_key3);
void cri_key9_derive(uint64_t key9, uint16_t subkey, uint16_t* p_key1, uint16_t* p_key2, uint16_t* p_key3);

int cri_key8_valid_keystring(uint8_t* buf, int buf_size);

#endif /* _CRI_KEYS_H_ */
