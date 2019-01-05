#ifndef _VSV_STREAMFILE_H_
#define _VSV_STREAMFILE_H_
#include "../streamfile.h"

typedef struct {
    off_t null_offset;
} vsv_io_data;

static size_t vsv_io_read(STREAMFILE *streamfile, uint8_t *dest, off_t offset, size_t length, vsv_io_data* data) {
    size_t bytes_read;
    int i;

    bytes_read = streamfile->read(streamfile, dest, offset, length);

    /* VSVs do start at 0x00, but first line is also the header; must null it to avoid clicks */
    if (offset < data->null_offset) {
        int max = data->null_offset - offset;
        if (max > bytes_read)
            max = bytes_read;

        for (i = 0; i < max; i++) {
            dest[i] = 0;
        }
    }
    /* VSV also has last 0x800 block with a PS-ADPCM flag of 0x10 (incorrect), but it's ignored by the decoder */

    return bytes_read;
}

static STREAMFILE* setup_vsv_streamfile(STREAMFILE *streamFile, off_t start_offset, size_t data_size) {
    STREAMFILE *temp_streamFile = NULL, *new_streamFile = NULL;
    vsv_io_data io_data = {0};
    size_t io_data_size = sizeof(vsv_io_data);

    io_data.null_offset = 0x10;

    /* setup custom streamfile */
    new_streamFile = open_wrap_streamfile(streamFile);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_io_streamfile(temp_streamFile, &io_data,io_data_size, vsv_io_read,NULL);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    return temp_streamFile;

fail:
    close_streamfile(temp_streamFile);
    return NULL;
}

#endif /* _VSV_STREAMFILE_H_ */
