#ifndef _TXTH_STREAMFILE_H_
#define _TXTH_STREAMFILE_H_
#include "../streamfile.h"


typedef struct {
    /* config */
    off_t stream_offset;
    size_t stream_size;
    size_t chunk_size;
    int chunk_count;
    int chunk_number;

    /* state */
    off_t logical_offset;       /* fake offset */
    off_t physical_offset;      /* actual offset */
    size_t block_size;          /* current size */
    size_t skip_size;           /* size from block start to reach data */
    size_t data_size;           /* usable size in a block */

    size_t logical_size;
} txth_io_data;


static size_t txth_io_read(STREAMFILE *streamfile, uint8_t *dest, off_t offset, size_t length, txth_io_data* data) {
    size_t total_read = 0;


    /* re-start when previous offset (can't map logical<>physical offsets) */
    if (data->logical_offset < 0 || offset < data->logical_offset) {
        data->physical_offset = data->stream_offset;
        data->logical_offset = 0x00;
        data->data_size = 0;
        data->skip_size = 0;
    }

    /* read blocks */
    while (length > 0) {

        /* ignore EOF */
        if (offset < 0 || data->physical_offset >= data->stream_offset + data->stream_size) {
            break;
        }

        /* process new block */
        if (data->data_size == 0) {
            data->block_size = data->chunk_size * data->chunk_count;
            data->skip_size = data->chunk_size * data->chunk_number;
            data->data_size = data->chunk_size;
        }

        /* move to next block */
        if (data->data_size == 0 || offset >= data->logical_offset + data->data_size) {
            data->physical_offset += data->block_size;
            data->logical_offset += data->data_size;
            data->data_size = 0;
            continue;
        }

        /* read data */
        {
            size_t bytes_consumed, bytes_done, to_read;

            bytes_consumed = offset - data->logical_offset;
            to_read = data->data_size - bytes_consumed;
            if (to_read > length)
                to_read = length;
            bytes_done = read_streamfile(dest, data->physical_offset + data->skip_size + bytes_consumed, to_read, streamfile);

            total_read += bytes_done;
            dest += bytes_done;
            offset += bytes_done;
            length -= bytes_done;

            if (bytes_done != to_read || bytes_done == 0) {
                break; /* error/EOF */
            }
        }
    }

    return total_read;
}

static size_t txth_io_size(STREAMFILE *streamfile, txth_io_data* data) {
    uint8_t buf[1];

    if (data->logical_size)
        return data->logical_size;

    /* force a fake read at max offset, to get max logical_offset (will be reset next read) */
    txth_io_read(streamfile, buf, 0x7FFFFFFF, 1, data);
    data->logical_size = data->logical_offset;

    return data->logical_size;
}

/* Handles deinterleaving of generic chunked streams */
static STREAMFILE* setup_txth_streamfile(STREAMFILE *streamFile, off_t chunk_start, size_t chunk_size, int chunk_count, int chunk_number, int is_opened_streamfile) {
    STREAMFILE *temp_streamFile = NULL, *new_streamFile = NULL;
    txth_io_data io_data = {0};
    size_t io_data_size = sizeof(txth_io_data);

    io_data.stream_offset = chunk_start;
    io_data.stream_size = (get_streamfile_size(streamFile) - chunk_start);
    io_data.chunk_size = chunk_size;
    io_data.chunk_count = chunk_count;
    io_data.chunk_number = chunk_number;
    io_data.logical_size = io_data.stream_size / chunk_count;
    io_data.logical_offset = -1; /* force phys offset reset */


    new_streamFile = streamFile;

    /* setup subfile */
    if (!is_opened_streamfile) {
        /* if streamFile was opened by txth code we MUST close it once done (as it's now "fused"),,
         * otherwise it was external to txth and must be wrapped to avoid closing it */
        new_streamFile = open_wrap_streamfile(new_streamFile);
        if (!new_streamFile) goto fail;
        temp_streamFile = new_streamFile;
    }

    new_streamFile = open_io_streamfile(new_streamFile, &io_data,io_data_size, txth_io_read,txth_io_size);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    //new_streamFile = open_buffer_streamfile(new_streamFile,0);
    //if (!new_streamFile) goto fail;
    //temp_streamFile = new_streamFile;

    return temp_streamFile;

fail:
    close_streamfile(temp_streamFile);
    return NULL;
}

#endif /* _TXTH_STREAMFILE_H_ */
