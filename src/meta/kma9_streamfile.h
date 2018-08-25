#ifndef _KM9_STREAMFILE_H_
#define _KM9_STREAMFILE_H_
#include "../streamfile.h"


typedef struct {
    /* config */
    int stream_number;
    int stream_count;
    size_t interleave_size;
    off_t stream_offset;
    size_t stream_size;

    /* state */
    off_t logical_offset;   /* offset that corresponds to physical_offset */
    off_t physical_offset;  /* actual file offset */

    size_t skip_size;       /* size to skip from a block start to reach data start */
    size_t data_size;       /* logical size of the block  */

    size_t logical_size;
} kma9_io_data;


static size_t kma9_io_read(STREAMFILE *streamfile, uint8_t *dest, off_t offset, size_t length, kma9_io_data* data) {
    size_t total_read = 0;

    /* ignore bad reads */
    if (offset < 0 || offset > data->logical_size) {
        return 0;
    }

    /* previous offset: re-start as we can't map logical<>physical offsets
     * (kinda slow as it trashes buffers, but shouldn't happen often) */
    if (offset < data->logical_offset) {
        data->logical_offset = 0x00;
        data->physical_offset = data->stream_offset;
        data->data_size = 0;
    }

    /* read blocks, one at a time */
    while (length > 0) {

        /* ignore EOF */
        if (data->logical_offset >= data->logical_size) {
            break;
        }

        /* process new block */
        if (data->data_size == 0) {
            data->skip_size = data->interleave_size * data->stream_number;
            data->data_size = data->interleave_size;
        }

        /* move to next block */
        if (offset >= data->logical_offset + data->data_size) {
            data->physical_offset += data->interleave_size*data->stream_count;
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

            offset += bytes_done;
            total_read += bytes_done;
            length -= bytes_done;
            dest += bytes_done;

            if (bytes_done != to_read || bytes_done == 0) {
                break; /* error/EOF */
            }
        }

    }

    return total_read;
}

static size_t kma9_io_size(STREAMFILE *streamfile, kma9_io_data* data) {
    off_t physical_offset = data->stream_offset;
    off_t  max_physical_offset = get_streamfile_size(streamfile);
    size_t logical_size = 0;

    if (data->logical_size)
        return data->logical_size;

    /* get size of the logical stream */
    while (physical_offset < max_physical_offset) {
        logical_size += data->interleave_size;
        physical_offset += data->interleave_size*data->stream_count;
    }

    if (logical_size > max_physical_offset)
        return 0;
    if (logical_size != data->stream_size)
        return 0;
    data->logical_size = logical_size;
    return data->logical_size;
}


/* Prepares custom IO for KMA9, which interleaves ATRAC9 frames */
static STREAMFILE* setup_kma9_streamfile(STREAMFILE *streamFile, off_t stream_offset, size_t stream_size, size_t interleave_size, int stream_number, int stream_count) {
    STREAMFILE *temp_streamFile = NULL, *new_streamFile = NULL;
    kma9_io_data io_data = {0};
    size_t io_data_size = sizeof(kma9_io_data);

    io_data.stream_number = stream_number;
    io_data.stream_count = stream_count;
    io_data.stream_offset = stream_offset;
    io_data.stream_size = stream_size;
    io_data.interleave_size = interleave_size;
    io_data.physical_offset = stream_offset;
    io_data.logical_size = kma9_io_size(streamFile, &io_data); /* force init */

    if (io_data.logical_size == 0) {
        VGM_LOG("KMA9: wrong logical size\n");
        goto fail;
    }

    /* setup subfile */
    new_streamFile = open_wrap_streamfile(streamFile);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_io_streamfile(temp_streamFile, &io_data,io_data_size, kma9_io_read,kma9_io_size);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    return temp_streamFile;

fail:
    close_streamfile(temp_streamFile);
    return NULL;
}


#endif /* _KM9_STREAMFILE_H_ */
