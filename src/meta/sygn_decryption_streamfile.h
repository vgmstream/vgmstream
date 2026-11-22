#ifndef _SYGN_DECRYPTION_STREAMFILE_H_
#define _SYGN_DECRYPTION_STREAMFILE_H_

#include "../streamfile.h"
#include "../util/cipher_aes.h"

#define BLOCK_SIZE 0x10

/* decrypts in realtime */
typedef struct {
    uint8_t key[192 / 8];
    uint8_t block[BLOCK_SIZE];
    aes_handle_t* ctx;
} sygn_io_data;

static int sygn_io_init(STREAMFILE* sf, sygn_io_data* data) {

    data->ctx = aes_init_192_ecb();
    if (!data->ctx)
        return -1;

    aes_set_key(data->ctx, data->key);

    return 0;
}

static void sygn_io_close(STREAMFILE* sf, sygn_io_data* data) {
    aes_free(data->ctx);
}


static int read_part(STREAMFILE* sf, uint8_t* dest, off_t offset, size_t length, sygn_io_data* data) {

    off_t offset_rem = offset % BLOCK_SIZE;
    offset -= offset_rem;

    if (offset_rem == 0 && length >= BLOCK_SIZE) /* handled in main */
        return 0;

    /* read one full block, regardless of requested length */
    int bytes = read_streamfile(data->block, offset, BLOCK_SIZE, sf);

    if (bytes < BLOCK_SIZE)
        memset(data->block + bytes, 0, BLOCK_SIZE - bytes);

    aes_decrypt(data->ctx, data->block);

    int max_copy = bytes - offset_rem;
    if (max_copy > length)
        max_copy = length;

    memcpy(dest, data->block + offset_rem, max_copy);

    return max_copy;
}

static int read_main(STREAMFILE* sf, uint8_t* dest, off_t offset, size_t length, sygn_io_data* data) {
    int read = 0;

    off_t offset_rem = offset % BLOCK_SIZE;
    size_t length_rem = length % BLOCK_SIZE;
    length -= length_rem;

    if (offset_rem != 0 || length == 0) /* handled in part */
        return 0;

    int bytes = read_streamfile(dest, offset, length, sf);
    
    while (read < bytes) {
        aes_decrypt(data->ctx, dest);
        dest += BLOCK_SIZE;
        read += BLOCK_SIZE;
    }
    
    return bytes;
}

/* handle block cipher, so arbitrary reads will need to handle partial cases:
 * - reading 0x00 to 0x20: direct decrypt (2 blocks of 0x10)
 * - reading 0x03 to 0x22: handle as 0x00 to 0x10 (head, copy 7 at 0x03), 0x10 to 0x20 (body, direct), and 0x20 to 0x28 (tail, copy 2 at 0x00). */
static size_t sygn_io_read(STREAMFILE* sf, uint8_t* dest, off_t offset, size_t length, sygn_io_data* data) {
    int bytes = 0;


    /* head */
    if (length) {
        int done = read_part(sf, dest, offset, length, data);
        dest += done;
        offset += done;
        length -= done;

        bytes += done;
    }

    /* body */
    if (length) {
        int done = read_main(sf, dest, offset, length, data);
        dest += done;
        offset += done;
        length -= done;

        bytes += done;
    }

    /* tail */
    if (length) {
        int done = read_part(sf, dest, offset, length, data);
        dest += done;
        offset += done;
        length -= done;

        bytes += done;
    }

    return bytes;
}


/* decrypts SYGN blocks using AES192 ECB */
static STREAMFILE* setup_sygn_streamfile(STREAMFILE* sf, const uint8_t* key, uint32_t key_size) {
    STREAMFILE* new_sf = NULL;
    sygn_io_data io_data = {0};
    const char* mask = "Ua#oK3P94vdxX,ft*k-mnjoO"; // same from NuSound's Siren16/22

    if (key_size != sizeof(io_data.key))
        return NULL;

    for (int i = 0; i < key_size; i++) {
        io_data.key[i] = key[i] ^ mask[i];
    }

    /* setup subfile */
    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_streamfile_ex_f(new_sf, &io_data, sizeof(sygn_io_data), sygn_io_read, NULL, sygn_io_init, sygn_io_close);
    return new_sf;
}

#endif
