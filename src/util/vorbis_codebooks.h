#ifndef _VORBIS_CODEBOOKS_H_
#define _VORBIS_CODEBOOKS_H_

#include <stdint.h>
#include "../streamfile.h"


typedef struct {
    uint32_t id;
    uint32_t size;
    const uint8_t* codebooks;
} vcb_info_t;


int vcb_load_codebook_array(uint8_t* buf, int buf_size, uint32_t setup_id, const vcb_info_t* list, int list_length);
//, STREAMFILE* sf
#endif
