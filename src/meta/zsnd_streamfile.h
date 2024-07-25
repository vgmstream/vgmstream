#ifndef _ZSND_STREAMFILE_H_
#define _ZSND_STREAMFILE_H_
#include "../streamfile.h"

typedef struct {
    off_t max_offset;
} zsnd_io_data;

static size_t zsnd_io_read(STREAMFILE *streamfile, uint8_t *dest, off_t offset, size_t length, zsnd_io_data* data) {
    size_t bytes_read, bytes_to_do;
    int i;

    /* clamp reads */
    bytes_to_do = length;
    if (offset > data->max_offset)
        offset = data->max_offset;
    if (offset + length > data->max_offset)
        bytes_to_do = data->max_offset - offset;

    bytes_read = streamfile->read(streamfile, dest, offset, bytes_to_do);

    /* pretend we got data after max_offset */
    if (bytes_read < length) {
        for (i = bytes_read; i < length; i++) {
            dest[i] = 0;
        }
        bytes_read = length;
    }

    return bytes_read;
}

/* ZSND removes last interleave data from the file if blank, but codecs still need to read after it.
 * This data could be from next stream or from EOF, so return blank data instead. */
static STREAMFILE* setup_zsnd_streamfile(STREAMFILE *streamFile, off_t start_offset, size_t stream_size) {
    STREAMFILE *temp_streamFile = NULL, *new_streamFile = NULL;
    zsnd_io_data io_data = {0};
    size_t io_data_size = sizeof(zsnd_io_data);

    io_data.max_offset = start_offset + stream_size;

    /* setup custom streamfile */
    new_streamFile = open_wrap_streamfile(streamFile);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_io_streamfile(temp_streamFile, &io_data,io_data_size, zsnd_io_read,NULL);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    return temp_streamFile;

fail:
    close_streamfile(temp_streamFile);
    return NULL;
}

#endif /* _ZSND_STREAMFILE_H_ */
