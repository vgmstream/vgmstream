#ifndef _AWC_DECRYPTION_STREAMFILE_H_
#define _AWC_DECRYPTION_STREAMFILE_H_

#include <stdlib.h>
#include "../streamfile.h"
#include "../util/cipher_xxtea.h"
#include "../util/companion_files.h"
#include "../util.h"

#define MAX_BLOCK_SIZE 0x6e4000 // usually 0x80000, observed max for Nch files ~= 8MB

/* decrypts xxtea blocks */
typedef struct {
    uint32_t data_offset;   // where encryption data starts
    uint32_t data_size;     // encrypted size
    uint32_t block_size;    // xxtea block chunk size (rather big)
    uint32_t key[4];        // 32-bit x4 decryption key
    uint8_t* buf;           // decrypted block
    uint32_t read_offset;   // last read block offset (aligned to data_offset + block_size)
} awcd_io_data;


static int awcd_io_init(STREAMFILE* sf, awcd_io_data* data) {
    /* ktsr keys start with size then random bytes (usually 7), assumed max 0x20 */

    data->buf = malloc(data->block_size);
    if (!data->buf)
        return -1;
    data->read_offset = -1;
    return 0;
}

static void awcd_io_close(STREAMFILE* sf, awcd_io_data* data) {
    free(data->buf);
}

/* reads from current block; offset/length must be within data_offset + data_size (handled externally) */
static int read_block(STREAMFILE* sf, uint8_t* dest, off_t offset, size_t length, awcd_io_data* data) {

    // detect if we requested offset falls within current decrypted block, otherwise read + decrypt
    off_t block_offset = (offset - data->data_offset) / data->block_size * data->block_size + data->data_offset; // closest block
    int block_read = clamp_u32(data->block_size, 0, data->data_size - (block_offset - data->data_offset)); // last block can be smaller
    if (data->read_offset != block_offset) {
        int bytes = read_streamfile(data->buf, block_offset, block_read, sf);
        if (bytes != block_read)
            return 0;
        xxtea_decrypt(data->buf, block_read, data->key);
        data->read_offset = block_offset;
    }

    int buf_pos = offset - block_offset; // within current block
    int to_do = clamp_u32(length, 0, block_read - buf_pos);
    memcpy(dest, data->buf + buf_pos, to_do);

    return to_do;
}

/* xxtea works with big chunks, so depending on requested offset read into buf + decrypt + copy */
static size_t awcd_io_read(STREAMFILE* sf, uint8_t* dest, off_t offset, size_t length, awcd_io_data* data) {
    size_t total_bytes = 0;

    while (length > 0) {
        int bytes;

        if (offset < data->data_offset) {
            // offset outside encrypted data, read normally
            int to_do = clamp_u32(length, 0, data->data_offset - offset);
            bytes = read_streamfile(dest, offset, to_do, sf);
        }
        else if (offset >= data->data_offset + data->data_size) {
            // offset after encrypted data, read normally
            int to_do = length;
            bytes = read_streamfile(dest, offset, to_do, sf);
        }
        else {
            // offset inside encrypted data, read + decrypt
            bytes = read_block(sf, dest, offset, length, data);
        }

        dest += bytes;
        offset += bytes;
        length -= bytes;
        total_bytes += bytes;

        // may be smaller than expected when reading between blocks but shouldn't be 0
        if (bytes == 0) 
            break;
    }

    return total_bytes;
}


/* decrypts AWC blocks (seen in GTA5 PC/PS4) using .awckey + xxtea algorithm (only for target subsong).
 *
 * Reversed from OpenIV.exe 4.1/2023 (see fun_007D5EA8) b/c it was easier than from GTA5.exe itself.
 * OpenIV includes 2 keys, one for PC and other for probably PS4 (since other platforms aren't encrypted);
 * neither seem to be found in GTA5.exe though (packed/derived?). Unlike standard xxtea OpenIV only has decryption.
 * Keys must be provided externally, could autodetect but given T2 suing habits let's err on the side of caution. */
static STREAMFILE* setup_awcd_streamfile(STREAMFILE* sf, uint32_t data_offset, uint32_t data_size, uint32_t block_size) {
    STREAMFILE* new_sf = NULL;
    awcd_io_data io_data = {0};

    uint8_t key[0x10];
    size_t key_size = read_key_file(key, sizeof(key), sf);
    if (key_size != sizeof(key))
        goto fail;

    for (int i = 0; i < sizeof(key) / 4; i++) {
        io_data.key[i] = get_u32be(key + i * 0x04);
    }

    if (data_offset == 0 || data_size == 0)
        goto fail;
    if (block_size == 0) // for non-blocked audio (small streams)
        block_size = data_size;
    if (block_size > MAX_BLOCK_SIZE || (block_size % 0x04) != 0)
        goto fail;

    io_data.data_offset = data_offset;
    io_data.data_size = data_size;
    io_data.block_size = block_size;

    /* setup subfile */
    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_streamfile_ex_f(new_sf, &io_data, sizeof(awcd_io_data), awcd_io_read, NULL, awcd_io_init, awcd_io_close);
    return new_sf;
fail:
    return NULL;
}

#endif
