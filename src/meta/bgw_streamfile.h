#ifndef _BGW_STREAMFILE_H_
#define _BGW_STREAMFILE_H_
#include "../streamfile.h"


#define BGW_KEY_MAX (0xC0 * 2)

typedef struct {
    uint8_t key[BGW_KEY_MAX];
    size_t key_size;
} bgw_decryption_data;

/* Encrypted ATRAC3 info from Moogle Toolbox (https://sourceforge.net/projects/mogbox/) */
static size_t bgw_decryption_read(STREAMFILE* sf, uint8_t* dest, off_t offset, size_t length, bgw_decryption_data* data) {
    size_t  bytes_read = sf->read(sf, dest, offset, length);

    /* decrypt data (xor) */
    for (int i = 0; i < bytes_read; i++) {
        dest[i] ^= data->key[(offset + i) % data->key_size];
    }

    //TODO: a few files (music069.bgw, music071.bgw, music900.bgw) have the last frames unencrypted,
    // though they are blank and encoder ignores wrongly decrypted frames and outputs blank samples as well.
    // Only in files that don't loop?

    return bytes_read;
}

static STREAMFILE* setup_bgw_atrac3_streamfile(STREAMFILE* sf, off_t subfile_offset, size_t subfile_size, size_t frame_size, int channels) {
    STREAMFILE* new_sf = NULL;
    bgw_decryption_data io_data = {0};
    size_t io_data_size = sizeof(bgw_decryption_data);

    /* setup decryption with key (first frame + modified channel header) */
    size_t key_size = frame_size * channels;
    if (key_size <= 0 || key_size > BGW_KEY_MAX)
        goto fail;

    io_data.key_size = read_streamfile(io_data.key, subfile_offset, key_size, sf);
    if (io_data.key_size != key_size)
        goto fail;

    for (int ch = 0; ch < channels; ch++) {
        uint32_t xor = get_u32be(io_data.key + frame_size * ch);
        put_u32be(io_data.key + frame_size * ch, xor ^ 0xA0024E9F);
    }

    /* setup subfile */
    new_sf = open_wrap_streamfile_f(sf);
    new_sf = open_clamp_streamfile_f(new_sf, subfile_offset,subfile_size);
    new_sf = open_io_streamfile_f(new_sf, &io_data,io_data_size, bgw_decryption_read,NULL);
    return new_sf;
fail:
    close_streamfile(new_sf);
    return NULL;
}

#endif
