#ifndef _AIX_STREAMFILE_H_
#define _AIX_STREAMFILE_H_
#include "../streamfile.h"


typedef struct {
    /* config */
    off_t stream_offset;
    size_t stream_size;
    int layer_number;

    /* state */
    off_t logical_offset;       /* fake offset */
    off_t physical_offset;      /* actual offset */
    size_t block_size;          /* current size */
    size_t skip_size;           /* size from block start to reach data */
    size_t data_size;           /* usable size in a block */

    size_t logical_size;
} aix_io_data;


static size_t aix_io_read(STREAMFILE *streamfile, uint8_t *dest, off_t offset, size_t length, aix_io_data* data) {
    size_t total_read = 0;


    /* re-start when previous offset (can't map logical<>physical offsets) */
    if (data->logical_offset < 0 || offset < data->logical_offset) {
        data->physical_offset = data->stream_offset;
        data->logical_offset = 0x00;
        data->data_size = 0;
    }

    /* read blocks */
    while (length > 0) {

        /* ignore EOF */
        if (offset < 0 || data->physical_offset >= data->stream_offset + data->stream_size) {
            break;
        }

        /* process new block */
        if (data->data_size == 0) {
            uint32_t block_id = read_u32be(data->physical_offset+0x00, streamfile);
            data->block_size  = read_u32be(data->physical_offset+0x04, streamfile) + 0x08;

            /* check valid block "AIXP" id, knowing that AIX segments end with "AIXE" block too */
            if (block_id != 0x41495850 || data->block_size == 0 || data->block_size == 0xFFFFFFFF) {
                break;
            }

            /* read target layer, otherwise skip to next block and try again */
            if (read_s8(data->physical_offset+0x08, streamfile) == data->layer_number) {
                /* 0x09(1): layer count */
                data->data_size = read_s16be(data->physical_offset+0x0a, streamfile);
                /* 0x0c: -1 */
                data->skip_size = 0x10;
            }

            /* strange AIX in Tetris Collection (PS2) with padding before ADX start (no known flag) */
            if (data->logical_offset == 0x00 &&
                    read_u32be(data->physical_offset + 0x10, streamfile) == 0 &&
                    read_u16be(data->physical_offset + data->block_size - 0x28, streamfile) == 0x8000) {
                data->data_size = 0x28;
                data->skip_size = data->block_size - 0x28;
            }
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

static size_t aix_io_size(STREAMFILE *streamfile, aix_io_data* data) {
    uint8_t buf[1];

    if (data->logical_size)
        return data->logical_size;

    /* force a fake read at max offset, to get max logical_offset (will be reset next read) */
    aix_io_read(streamfile, buf, 0x7FFFFFFF, 1, data);
    data->logical_size = data->logical_offset;

    return data->logical_size;
}

/* Handles deinterleaving of AIX blocked layer streams */
static STREAMFILE* setup_aix_streamfile(STREAMFILE *streamFile, off_t stream_offset, size_t stream_size, int layer_number, const char* extension) {
    STREAMFILE *temp_streamFile = NULL, *new_streamFile = NULL;
    aix_io_data io_data = {0};
    size_t io_data_size = sizeof(aix_io_data);

    io_data.stream_offset = stream_offset;
    io_data.stream_size = stream_size;
    io_data.layer_number = layer_number;
    io_data.logical_offset = -1; /* force reset */

    /* setup subfile */
    new_streamFile = open_wrap_streamfile(streamFile);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_io_streamfile(new_streamFile, &io_data,io_data_size, aix_io_read,aix_io_size);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_buffer_streamfile(new_streamFile,0);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    if (extension) {
        new_streamFile = open_fakename_streamfile(temp_streamFile, NULL,extension);
        if (!new_streamFile) goto fail;
        temp_streamFile = new_streamFile;
    }

    return temp_streamFile;

fail:
    close_streamfile(temp_streamFile);
    return NULL;
}

#endif /* _AIX_STREAMFILE_H_ */
