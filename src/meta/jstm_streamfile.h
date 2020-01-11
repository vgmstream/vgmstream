#ifndef _JSTM_STREAMFILE_H_
#define _JSTM_STREAMFILE_H_
#include "../streamfile.h"


typedef struct {
    off_t start_offset;
} jstm_decryption_data;

static size_t jstm_decryption_read(STREAMFILE *streamfile, uint8_t *dest, off_t offset, size_t length, jstm_decryption_data* data) {
    size_t bytes_read;
    int i;

    bytes_read = streamfile->read(streamfile, dest, offset, length);

    /* decrypt data (xor) */
    for (i = 0; i < bytes_read; i++) {
        if (offset+i >= data->start_offset) {
            dest[i] = dest[i] ^ 0x5A;
        }
    }

    return bytes_read;
}

static STREAMFILE* setup_jstm_streamfile(STREAMFILE *streamFile, off_t start_offset) {
    STREAMFILE *temp_streamFile = NULL, *new_streamFile = NULL;
    jstm_decryption_data io_data = {0};
    size_t io_data_size = sizeof(jstm_decryption_data);

    /* setup decryption */
    io_data.start_offset = start_offset;


    /* setup custom streamfile */
    new_streamFile = open_wrap_streamfile(streamFile);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_io_streamfile(temp_streamFile, &io_data,io_data_size, jstm_decryption_read,NULL);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    return temp_streamFile;

fail:
    close_streamfile(temp_streamFile);
    return NULL;
}

#endif /* _JSTM_STREAMFILE_H_ */
