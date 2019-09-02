#ifndef _BGW_STREAMFILE_H_
#define _BGW_STREAMFILE_H_
#include "../streamfile.h"


#define BGW_KEY_MAX (0xC0*2)

typedef struct {
    uint8_t key[BGW_KEY_MAX];
    size_t key_size;
} bgw_decryption_data;

/* Encrypted ATRAC3 info from Moogle Toolbox (https://sourceforge.net/projects/mogbox/) */
static size_t bgw_decryption_read(STREAMFILE *streamfile, uint8_t *dest, off_t offset, size_t length, bgw_decryption_data* data) {
    size_t bytes_read;
    int i;

    bytes_read = streamfile->read(streamfile, dest, offset, length);

    /* decrypt data (xor) */
    for (i = 0; i < bytes_read; i++) {
        dest[i] ^= data->key[(offset + i) % data->key_size];
    }

    //todo: a few files (music069.bgw, music071.bgw, music900.bgw) have the last frames unencrypted,
    // though they are blank and encoder ignores wrongly decrypted frames and outputs blank samples as well

    return bytes_read;
}

static STREAMFILE* setup_bgw_atrac3_streamfile(STREAMFILE *streamFile, off_t subfile_offset, size_t subfile_size, size_t frame_size, int channels) {
    STREAMFILE *temp_streamFile = NULL, *new_streamFile = NULL;
    bgw_decryption_data io_data = {0};
    size_t io_data_size = sizeof(bgw_decryption_data);
    int ch;

    /* setup decryption with key (first frame + modified channel header) */
    if (frame_size*channels == 0 || frame_size*channels > BGW_KEY_MAX) goto fail;

    io_data.key_size = read_streamfile(io_data.key, subfile_offset, frame_size*channels, streamFile);
    for (ch = 0; ch < channels; ch++) {
        uint32_t xor = get_32bitBE(io_data.key + frame_size*ch);
        put_32bitBE(io_data.key + frame_size*ch, xor ^ 0xA0024E9F);
    }

    /* setup subfile */
    new_streamFile = open_wrap_streamfile(streamFile);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_clamp_streamfile(temp_streamFile, subfile_offset,subfile_size);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_io_streamfile(temp_streamFile, &io_data,io_data_size, bgw_decryption_read,NULL);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    return temp_streamFile;

fail:
    close_streamfile(temp_streamFile);
    return NULL;
}

#endif /* _BGW_STREAMFILE_H_ */
