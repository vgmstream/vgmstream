#ifndef _KTSR_STREAMFILE_H_
#define _KTSR_STREAMFILE_H_

#include "../streamfile.h"
#include "../util/log.h"
#include "../util/cipher_blowfish.h"

/* decrypts blowfish in realtime (as done by games) */
typedef struct {
    uint8_t key[0x20];
    uint8_t block[0x08];
    blowfish_ctx* ctx;
} ktsr_io_data;

static int ktsr_io_init(STREAMFILE* sf, ktsr_io_data* data) {
    /* ktsr keys start with size then random bytes (usually 7), assumed max 0x20 */
    if (data->key[0] >= sizeof(data->key) - 1)
        return -1;

    data->ctx = blowfish_init_ecb(data->key + 1, data->key[0]);
    if (!data->ctx)
        return -1;
    return 0;
}

static void ktsr_io_close(STREAMFILE* sf, ktsr_io_data* data) {
    blowfish_free(data->ctx);
}


static int read_part(STREAMFILE* sf, uint8_t* dest, off_t offset, size_t length, ktsr_io_data* data) {

    off_t offset_rem = offset % 0x08;
    offset -= offset_rem;

    if (offset_rem == 0 && length >= 0x08) /* handled in main */
        return 0;

    /* read one full block, regardless of requested length */
    int bytes = read_streamfile(data->block, offset, 0x08, sf);

     /* useless since KTSR data is padded and blocks don't work otherwise but for determinability */
    if (bytes < 0x08)
        memset(data->block + bytes, 0, 0x08 - bytes);

    blowfish_decrypt_ecb(data->ctx, data->block);

    int max_copy = bytes - offset_rem;
    if (max_copy > length)
        max_copy = length;

    memcpy(dest, data->block + offset_rem, max_copy);

    return max_copy;
}

static int read_main(STREAMFILE* sf, uint8_t* dest, off_t offset, size_t length, ktsr_io_data* data) {
    int read = 0;

    off_t offset_rem = offset % 0x08;
    size_t length_rem = length % 0x08;
    length -= length_rem;

    if (offset_rem != 0 || length == 0) /* handled in part */
        return 0;

    int bytes = read_streamfile(dest, offset, length, sf);
    
    while (read < bytes) {
        blowfish_decrypt_ecb(data->ctx, dest);
        dest += 0x08;
        read += 0x08;
    }
    
    return bytes;
}

/* blowfish is a 64-bit block cipher, so arbitrary reads will need to handle partial cases. ex
 * - reading 0x00 to 0x20: direct decrypt (4 blocks of 0x08)
 * - reading 0x03 to 0x07: decrypt 0x00 to 0x08 but copy 4 bytes at 0x03
 * - reading 0x03 to 0x22: handle as 0x00 to 0x08 (head, copy 5 at 0x3), 0x08 to 0x20 (body, direct), and 0x20 to 0x28 (tail, copy 2 at 0x0). */
static size_t ktsr_io_read(STREAMFILE* sf, uint8_t* dest, off_t offset, size_t length, ktsr_io_data* data) {
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



/* Decrypts blowfish KTSR streams */
static STREAMFILE* setup_ktsr_streamfile(STREAMFILE* sf, uint32_t st_offset, bool is_external, uint32_t subfile_offset, uint32_t subfile_size, const char* extension) {
    STREAMFILE* new_sf = NULL;
    ktsr_io_data io_data = {0};

    if (is_external) {
        if (!is_id32be(st_offset + 0x00, sf, "KTSR"))
            return NULL;
        read_streamfile(io_data.key, st_offset + 0x20, sizeof(io_data.key), sf);
    }

    /* setup subfile */
    new_sf = open_wrap_streamfile(sf);

    /* no apparent flag other than key at offset. Only data at subfile is encrypted, so this reader assumes it will be clamped */
    if (io_data.key[0] != 0)
        new_sf = open_io_streamfile_ex_f(new_sf, &io_data, sizeof(ktsr_io_data), ktsr_io_read, NULL, ktsr_io_init, ktsr_io_close);

    new_sf = open_clamp_streamfile_f(new_sf, subfile_offset, subfile_size);
    if (extension)
        new_sf = open_fakename_streamfile_f(new_sf, NULL, extension);
    return new_sf;
}

#endif
