#ifndef _NUS3BANK_STREAMFILE_H_
#define _NUS3BANK_STREAMFILE_H_
#include "../streamfile.h"


static uint32_t swap_endian32(uint32_t v) {
    return ((v & 0xff000000) >> 24u) |
           ((v & 0x00ff0000) >>  8u) |
           ((v & 0x0000ff00) <<  8u) |
           ((v & 0x000000ff) << 24u);
}


#define KEY_MAX_SIZE  0x1000

typedef struct {
    uint8_t key[KEY_MAX_SIZE];
    int key_len;
} io_data_t;

static size_t io_read(STREAMFILE *sf, uint8_t *dest, off_t offset, size_t length, io_data_t *data) {
    size_t bytes = read_streamfile(dest, offset, length, sf);

    /* decrypt data (xor) */
    if (offset < data->key_len) {
        for (int i = 0; i < bytes; i++) {
            if (offset + i < data->key_len)
                dest[i] ^= data->key[offset + i];
        }
    }

    return bytes;
}

/* decrypts RIFF streams in NUS3BANK */
static STREAMFILE* setup_nus3bank_streamfile(STREAMFILE* sf, off_t start) {
    STREAMFILE* new_sf = NULL;
    io_data_t io_data = {0};

    /* setup key */
    {
        uint32_t base_key, chunk_key;
        uint8_t buf[KEY_MAX_SIZE];
        uint32_t chunk_type, chunk_size;
        int pos, data_pos;

       /* Header is XORed with a base key and a derived chunk type/size key, while chunk data is XORed with
        * unencrypted data itself, so we need to find where "data" starts then do another pass to properly set key.
        * Original code handles RIFF's "data" and also BNSF's "sdat" too, encrypted BNSF aren't known though. */

        int bytes = read_streamfile(buf, start, sizeof(buf), sf);
        if (bytes < 0x800) return NULL; // files of 1 XMA block do exist, but not less

        base_key = 0x0763E951;
        chunk_type = get_u32be(buf + 0x00) ^ base_key;
        chunk_size = get_u32be(buf + 0x04) ^ base_key;
        if (chunk_type != get_id32be("RIFF"))
            return NULL;

        chunk_key = base_key ^ (((chunk_size >> 16u) & 0x0000FFFF) | ((chunk_size << 16u) & 0xFFFF0000)); // ROTr 16 size

        /* find "data" */
        pos = 0x0c;
        data_pos = 0;
        while (pos < sizeof(buf)) {
            chunk_type = get_u32be(buf + pos + 0x00) ^ chunk_key;
            chunk_size = get_u32be(buf + pos + 0x04) ^ chunk_key;
            chunk_size = swap_endian32(chunk_size);
            pos += 0x08;

            if (chunk_type == get_id32be("data")) {
                data_pos = pos;
                break;
            }

            if (pos + chunk_size > sizeof(buf) - 0x08) {
                VGM_LOG("NUS3 SF: header too big\n");
                return NULL; // max around 0x400
            }

            pos += chunk_size;
        }


        /* setup key */
        put_u32be(io_data.key + 0x00, base_key);
        put_u32be(io_data.key + 0x04, base_key);
        put_u32be(io_data.key + 0x08, chunk_key);
        pos = 0x0c; // after WAVE

        while (pos < data_pos) {
            chunk_type = get_u32be(buf + pos + 0x00) ^ chunk_key;
            chunk_size = get_u32be(buf + pos + 0x04) ^ chunk_key;
            chunk_size = swap_endian32(chunk_size);

            put_u32be(io_data.key + pos + 0x00, chunk_key);
            put_u32be(io_data.key + pos + 0x04, chunk_key);
            pos += 0x08;

            if (pos >= data_pos)
                break;

            /* buf must contain data of at least chunk_size */
            if (data_pos + chunk_size >= sizeof(buf)) {
                VGM_LOG("NUS3 SF: chunk too big\n");
                return NULL;
            }

            memcpy(io_data.key + pos, buf + data_pos, chunk_size);

            pos += chunk_size;
        }

        io_data.key_len = data_pos;
    }


    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_streamfile(new_sf, &io_data, sizeof(io_data_t), io_read, NULL);
    return new_sf;
}

#endif
