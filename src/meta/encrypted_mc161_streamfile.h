#ifndef _MC161_STREAMFILE_H_
#define _MC161_STREAMFILE_H_
#include "../streamfile.h"


typedef struct {
    int32_t base_key;
    int32_t curr_key;
    uint32_t curr_offset;
} mc161_io_data;


static void decrypt_chunk(uint8_t* buf, int buf_size, mc161_io_data* data) {
    int i;
    int32_t hash = data->curr_key;


    for (i = 0; i < buf_size; i++) {
        buf[i] = (uint8_t)(buf[i] ^ ((hash >> 8) & 0xFF));
        hash = (int32_t)(hash * 498729871) + (85731 * (int8_t)buf[i]); /* signed */
    }

    data->curr_key = hash;
    data->curr_offset += buf_size;
}

static void update_key(STREAMFILE* sf, off_t offset, mc161_io_data* data) {
    uint8_t buf[0x800];
    size_t bytes;
    size_t to_skip;

    if (offset < data->curr_offset || offset == 0x00) {
        data->curr_key = data->base_key;
        data->curr_offset = 0x00;
        to_skip = offset;
    }
    else {
        to_skip = offset - data->curr_offset;
    }

    /* update key by reading and decrypt all data between current offset + last known key to requested offset */
    while (to_skip > 0) {
        size_t read_size = sizeof(buf);
        if (read_size > to_skip)
            read_size = to_skip;

        bytes = read_streamfile(buf, data->curr_offset, read_size, sf);
        if (!bytes) /* ??? */
            break;

        decrypt_chunk(buf, bytes, data); /* updates curr_offset and key */
        to_skip -= bytes;
    }
}

/* XOR depends on decrypted data, meanings having to decrypt linearly to reach some offset. */
static size_t mc161_io_read(STREAMFILE* sf, uint8_t* dest, off_t offset, size_t length, mc161_io_data* data) {
    size_t bytes;

    /* read and decrypt unneded data */
    update_key(sf, offset, data);

    /* read and decrypt current data */
    bytes = read_streamfile(dest, offset, length, sf);
    decrypt_chunk(dest, bytes, data);

    return bytes;
}

/* String.hashCode() should be equivalent to this on Windows (though implementation-defined), note Java has no unsigned */
static int32_t mc161_get_java_hashcode(STREAMFILE* sf) {
    char filename[1024];
    int i = 0;
    int32_t hash = 0;

    get_streamfile_filename(sf, filename, sizeof(filename));

    while (filename[i] != '\0') {
        hash = 31 * hash + (uint8_t)filename[i];
        i++;
    }

    return hash;
}

/* decrypts Minecraft old streams (some info from: https://github.com/ata4/muscode) */
static STREAMFILE* setup_mc161_streamfile(STREAMFILE* sf) {
    STREAMFILE* new_sf = NULL;
    mc161_io_data io_data = {0};

    io_data.base_key = mc161_get_java_hashcode(sf);
    io_data.curr_key = io_data.base_key;
    io_data.curr_offset = 0x00;

    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_streamfile_f(new_sf, &io_data, sizeof(mc161_io_data), mc161_io_read, NULL);
    new_sf = open_fakename_streamfile_f(new_sf, NULL, "ogg");
    return new_sf;
}

#endif /* _MC161_STREAMFILE_H_ */
