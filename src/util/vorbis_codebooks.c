#include "vorbis_codebooks.h"


int vcb_load_codebook_array(uint8_t* buf, int buf_size, uint32_t setup_id, const vcb_info_t* list, int list_length) {

    for (int i = 0; i < list_length; i++) {

        if (list[i].id != setup_id)
            continue;

        if (list[i].size > buf_size) // can't handle
            return 0;

        // found: copy data as-is
        memcpy(buf, list[i].codebooks, list[i].size);
        return list[i].size;
    }

    return 0;
}
