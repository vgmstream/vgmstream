#ifndef _BGM_STREAMFILE_H_
#define _BGM_STREAMFILE_H_
#include "../streamfile.h"


typedef struct {
    uint8_t key[0x100];
    size_t key_len;
    off_t start;
} bgm_io_data;

static size_t bgm_io_read(STREAMFILE *sf, uint8_t *dest, off_t offset, size_t length, bgm_io_data* data) {
    int i, begin, pos;
    size_t bytes = read_streamfile(dest, offset, length, sf);

    /* decrypt data (xor) */
    if (offset + length > data->start) {
        if (offset < data->start) {
            begin = data->start - offset;
            pos = 0;
        }
        else {
            begin = 0;
            pos = offset - data->start;
        }

        for (i = begin; i < bytes; i++) {
            dest[i] ^= data->key[(pos++) % data->key_len];
        }
    }

    return bytes;
}

/* decrypts BGM stream */
static STREAMFILE* setup_bgm_streamfile(STREAMFILE *sf, off_t start, uint8_t* key, int key_len) {
    STREAMFILE *new_sf = NULL;
    bgm_io_data io_data = {0};

    io_data.start = start;
    io_data.key_len = key_len;
    if (key_len > sizeof(io_data.key))
        return NULL;
    memcpy(io_data.key, key, key_len);

    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_streamfile_f(new_sf, &io_data, sizeof(bgm_io_data), bgm_io_read, NULL);
    new_sf = open_fakename_streamfile_f(new_sf, NULL, "wav");
    return new_sf;
}

#endif /* _BGM_STREAMFILE_H_ */
