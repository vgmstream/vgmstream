#ifndef _HASHES_H_
#define _HASHES_H_

#include "../streamfile.h"
#include <stdint.h>

uint32_t hash_str_lc(const char* str);
uint32_t hash_sf(STREAMFILE* sf);

#endif
