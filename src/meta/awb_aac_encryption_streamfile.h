#ifndef _AWB_AAC_ENCRYPTION_STREAMFILE_H_
#define _AWB_AAC_ENCRYPTION_STREAMFILE_H_
#include "../streamfile.h"


typedef struct {
    uint16_t key[4];

    uint16_t xor;
    uint16_t add;
    uint16_t mul;
 
    uint32_t curr_offset;
} cri_aac_io_data;


// see criAacCodec_SetDecryptionKey
static void setup_key(uint64_t keycode, cri_aac_io_data* cridata) {
    if (!keycode)
        return;
    uint16_t k0 = 4 * ((keycode >> 0)  & 0x0FFF) | 1;
    uint16_t k1 = 2 * ((keycode >> 12) & 0x1FFF) | 1;
    uint16_t k2 = 4 * ((keycode >> 25) & 0x1FFF) | 1;
    uint16_t k3 = 2 * ((keycode >> 38) & 0x3FFF) | 1;

    cridata->key[0] = k0 ^ k1;
    cridata->key[1] = k1 ^ k2;
    cridata->key[2] = k2 ^ k3;
    cridata->key[3] = ~k3;

    // criatomexacb_generate_aac_decryption_key is slightly different, unsure if used:
  //keydata->key[0] = k0 ^ k3;
  //keydata->key[1] = k2 ^ k3;
  //keydata->key[2] = k2 ^ k3;
  //keydata->key[3] = ~k3;
}

// see criAacCodec_DecryptData
static void decrypt_chunk(cri_aac_io_data* data, uint8_t* dst, uint32_t dst_size) {
    uint16_t seed0 = ~data->key[3];
    uint16_t seed1 = seed0 ^ data->key[2];
    uint16_t seed2 = seed1 ^ data->key[1];
    uint16_t seed3 = seed2 ^ data->key[0];

    uint16_t xor = data->xor;
    uint16_t add = data->add;
    uint16_t mul = data->mul;

    // use as special flag (original code decrypts whole files so values are always reset)
    if (dst == NULL && dst_size == 0) {
        xor = 2 * seed0 | 1;
        add = 2 * seed0 | 1; // not seed1
        mul = 4 * seed2 | 1;
    }

    for (int i = 0; i < dst_size; i++) {

        int curr_i = data->curr_offset + i;
        if (!(uint16_t)curr_i) { // every 0x10000, without modulo
            mul = ((4 * seed2 + seed3 * (mul & 0xFFFC)) & 0xFFFD) | 1;
            add = (2 * seed0 + seed1 * (add & 0xFFFE)) | 1;
        }
        xor = xor * mul + add;

        if (dst != NULL) {
            dst[i] ^= (xor >> 8) & 0xFF;
        }
    }

    data->xor = xor;
    data->add = add;
    data->mul = mul;

    data->curr_offset += dst_size;
}

static void reset_keydata(cri_aac_io_data* data) {
    decrypt_chunk(data, NULL, 0);
}

static void update_key(cri_aac_io_data* data, STREAMFILE* sf, off_t offset) {
    size_t to_skip;

    if (offset < data->curr_offset || offset == 0x00) {
        reset_keydata(data);
        data->curr_offset = 0x00;
        to_skip = offset;
    }
    else {
        to_skip = offset - data->curr_offset;
    }

    if (to_skip == 0)
        return;

    // update key by reading and decrypt all data between current offset + last known key to requested offset
    decrypt_chunk(data, NULL, to_skip);
}

/* XOR depends on decrypted data, meanings having to decrypt linearly to reach some offset. */
static size_t cri_aac_io_read(STREAMFILE* sf, uint8_t* dest, off_t offset, size_t length, cri_aac_io_data* data) {
    size_t bytes;

    // fix key to current offset
    update_key(data,  sf, offset);

    // read and decrypt current data
    bytes = read_streamfile(dest, offset, length, sf);
    decrypt_chunk(data, dest, bytes);

    return bytes;
}

/* decrypts CRI's AAC encryption, from decompilation [Final Fantasy Digital Card Game (Browser)] */
static STREAMFILE* setup_awb_aac_encryption_streamfile(STREAMFILE* sf, uint32_t subfile_offset, uint32_t subfile_size, const char* extension, uint64_t keycode) {
    STREAMFILE* new_sf = NULL;
    cri_aac_io_data io_data = {0};

    setup_key(keycode, &io_data);
    reset_keydata(&io_data);
    io_data.curr_offset = 0x00;

    new_sf = open_wrap_streamfile(sf);
    new_sf = open_clamp_streamfile_f(new_sf, subfile_offset, subfile_size);
    new_sf = open_io_streamfile_f(new_sf, &io_data, sizeof(cri_aac_io_data), cri_aac_io_read, NULL);
    if (extension) {
        new_sf = open_fakename_streamfile_f(new_sf, NULL, extension);
    }
    return new_sf;
}

#endif
