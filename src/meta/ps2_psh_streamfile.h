#ifndef _PS2_PSH_STREAMFILE_H_
#define _PS2_PSH_STREAMFILE_H_
#include "../streamfile.h"

typedef struct {
    off_t null_offset;
} ps2_psh_io_data;

static size_t ps2_psh_io_read(STREAMFILE *streamfile, uint8_t *dest, off_t offset, size_t length, ps2_psh_io_data* data) {
    size_t bytes_read;
    int i;

    bytes_read = streamfile->read(streamfile, dest, offset, length);

    /* PSHs do start at 0x00, but first line is also the header; must null it to avoid clicks */
    if (offset < data->null_offset) {
        for (i = 0; i < data->null_offset - offset; i++) {
            dest[i] = 0;
        }
    }

    return bytes_read;
}

static STREAMFILE* setup_ps2_psh_streamfile(STREAMFILE *streamFile, off_t start_offset, size_t data_size) {
    STREAMFILE *temp_streamFile = NULL, *new_streamFile = NULL;
    ps2_psh_io_data io_data = {0};
    size_t io_data_size = sizeof(ps2_psh_io_data);

    io_data.null_offset = 0x10;

    /* setup custom streamfile */
    new_streamFile = open_wrap_streamfile(streamFile);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_io_streamfile(temp_streamFile, &io_data,io_data_size, ps2_psh_io_read,NULL);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    return temp_streamFile;

fail:
    close_streamfile(temp_streamFile);
    return NULL;
}

#endif /* _PS2_PSH_STREAMFILE_H_ */
