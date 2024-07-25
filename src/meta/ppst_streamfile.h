#ifndef _PPST_STREAMFILE_H_
#define _PPST_STREAMFILE_H_
#include "../streamfile.h"


typedef struct {
    off_t start_physical_offset; /* interleaved data start, for this substream */
    size_t interleave_block_size; /* max size that can be read before encountering other substreams */
    size_t stride_size; /* step size between interleave blocks (interleave*channels) */
    size_t stream_size; /* final size of the deinterleaved substream */
} ppst_io_data;


/* Handles deinterleaving of complete files, skipping portions or other substreams. */
static size_t ppst_io_read(STREAMFILE *streamfile, uint8_t *dest, off_t offset, size_t length, ppst_io_data* data) {
    size_t total_read = 0;


    while (length > 0) {
        size_t to_read;
        size_t length_available;
        off_t block_num;
        off_t intrablock_offset;
        off_t physical_offset;

        if (offset >= data->stream_size)
            return total_read;

        block_num = offset / data->interleave_block_size;
        intrablock_offset = offset % data->interleave_block_size;
        physical_offset = data->start_physical_offset + block_num*data->stride_size + intrablock_offset;
        length_available = data->interleave_block_size - intrablock_offset;
        if (length_available > data->stream_size - offset)
            length_available = data->stream_size - offset;

        if (length < length_available) {
            to_read = length;
        }
        else {
            to_read = length_available;
        }

        if (to_read > 0) {
            size_t bytes_read;

            bytes_read = read_streamfile(dest, physical_offset, to_read, streamfile);
            total_read += bytes_read;

            if (bytes_read != to_read) {
                return total_read;
            }

            dest += bytes_read;
            offset += bytes_read;
            length -= bytes_read;
        }
    }

    return total_read;
}

static size_t ppst_io_size(STREAMFILE *streamfile, ppst_io_data* data) {
    return data->stream_size;
}


static STREAMFILE* setup_ppst_streamfile(STREAMFILE *streamFile, off_t start_offset, size_t interleave_block_size, size_t stride_size, size_t stream_size) {
    STREAMFILE *temp_streamFile = NULL, *new_streamFile = NULL;
    ppst_io_data io_data = {0};
    size_t io_data_size = sizeof(ppst_io_data);

    io_data.start_physical_offset = start_offset;
    io_data.interleave_block_size = interleave_block_size;
    io_data.stride_size = stride_size;
    io_data.stream_size = stream_size;

    /* setup subfile */
    new_streamFile = open_wrap_streamfile(streamFile);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_io_streamfile(temp_streamFile, &io_data,io_data_size, ppst_io_read,ppst_io_size);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_buffer_streamfile(new_streamFile,0);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_fakename_streamfile(temp_streamFile, NULL,"at3");
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    return temp_streamFile;

fail:
    close_streamfile(temp_streamFile);
    return NULL;
}

#endif /* _SCD_STREAMFILE_H_ */
