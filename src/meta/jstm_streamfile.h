#ifndef _JSTM_STREAMFILE_H_
#define _JSTM_STREAMFILE_H_
#include "../streamfile.h"


typedef struct {
    off_t start;
} jstm_io_data;

static size_t jstm_io_read(STREAMFILE *sf, uint8_t *dest, off_t offset, size_t length, jstm_io_data* data) {
    int i;
    size_t bytes = read_streamfile(dest, offset, length, sf);

    /* decrypt data (xor) */
    for (i = 0; i < bytes; i++) {
        if (offset + i >= data->start) {
            dest[i] ^= 0x5A;
        }
    }

    return bytes;
}

/* decrypts JSTM stream */
static STREAMFILE* setup_jstm_streamfile(STREAMFILE *sf, off_t start) {
    STREAMFILE *new_sf = NULL;
    jstm_io_data io_data = {0};

    io_data.start = start;

    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_streamfile_f(new_sf, &io_data, sizeof(jstm_io_data), jstm_io_read, NULL);
    return new_sf;
}

#endif /* _JSTM_STREAMFILE_H_ */
