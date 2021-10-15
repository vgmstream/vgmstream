#ifndef _TXTH_STREAMFILE_H_
#define _TXTH_STREAMFILE_H_
#include "../streamfile.h"
#include "../util/endianness.h"


typedef struct {
    uint32_t chunk_start;
    uint32_t chunk_size;
    uint32_t chunk_header_size;
    uint32_t chunk_data_size;

    int chunk_count;
    int chunk_number;

    uint32_t chunk_value;
    uint32_t chunk_size_offset;
    int chunk_be;
} txth_io_config_data;

typedef struct {
    /* config */
    txth_io_config_data cfg;
    uint32_t stream_size;

    /* state */
    uint32_t logical_offset;       /* fake offset */
    uint32_t physical_offset;      /* actual offset */
    uint32_t block_size;          /* current size */
    uint32_t skip_size;           /* size from block start to reach data */
    uint32_t data_size;           /* usable size in a block */

    uint32_t logical_size;
} txth_io_data;


static size_t txth_io_read(STREAMFILE* sf, uint8_t* dest, off_t offset, size_t length, txth_io_data* data) {
    size_t total_read = 0;


    /* re-start when previous offset (can't map logical<>physical offsets) */
    if (data->logical_offset < 0 || offset < data->logical_offset) {
        data->physical_offset = data->cfg.chunk_start;
        data->logical_offset = 0x00;
        data->data_size = 0;
        data->skip_size = 0;
    }

    /* read blocks */
    while (length > 0) {

        /* ignore EOF */
        if (offset < 0 || data->physical_offset >= data->cfg.chunk_start + data->stream_size) {
            break;
        }

        /* process new block */
        if (data->data_size == 0) {
            /* base sizes */
            data->block_size = data->cfg.chunk_size * data->cfg.chunk_count;
            data->skip_size = data->cfg.chunk_size * data->cfg.chunk_number;
            data->data_size = data->cfg.chunk_size;

            /* chunk size modifiers */
            if (data->cfg.chunk_header_size) {
                data->skip_size += data->cfg.chunk_header_size;
                data->data_size -= data->cfg.chunk_header_size;
            }
            if (data->cfg.chunk_data_size) {
                data->data_size = data->cfg.chunk_data_size;
            }

            /* chunk size reader (overwrites the above) */
            if (data->cfg.chunk_header_size && data->cfg.chunk_size_offset) {
                read_u32_t read_u32 = data->cfg.chunk_be ? read_u32be : read_u32le;

                data->block_size = read_u32(data->physical_offset + data->cfg.chunk_size_offset, sf);
                data->data_size = data->block_size - data->cfg.chunk_header_size;

                /* skip chunk if doesn't match expected header value */
                if (data->cfg.chunk_value) {
                    uint32_t value = read_u32(data->physical_offset + 0x00, sf);
                    if (value != data->cfg.chunk_value) {
                        data->data_size = 0;
                    }
                }
            }

            /* clamp for games where last block is smaller */ //todo not correct for all cases
            if (data->physical_offset + data->block_size > data->cfg.chunk_start + data->stream_size) {
                data->block_size = (data->cfg.chunk_start + data->stream_size) - data->physical_offset;
                data->skip_size = (data->block_size / data->cfg.chunk_count) * data->cfg.chunk_number;
            }
            if (data->physical_offset + data->data_size > data->cfg.chunk_start + data->stream_size) {
                data->data_size = (data->cfg.chunk_start + data->stream_size) - data->physical_offset;
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
            bytes_done = read_streamfile(dest, data->physical_offset + data->skip_size + bytes_consumed, to_read, sf);

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

static size_t txth_io_size(STREAMFILE* sf, txth_io_data* data) {
    uint8_t buf[1];

    if (data->logical_size)
        return data->logical_size;

    /* force a fake read at max offset, to get max logical_offset (will be reset next read) */
    txth_io_read(sf, buf, 0x7FFFFFFF, 1, data);
    data->logical_size = data->logical_offset;

    return data->logical_size;
}

//todo use deblock streamfile
/* Handles deinterleaving of generic chunked streams */
static STREAMFILE* setup_txth_streamfile(STREAMFILE* sf, txth_io_config_data cfg, int is_opened_streamfile) {
    STREAMFILE* new_sf = NULL;
    txth_io_data io_data = {0};
    size_t io_data_size = sizeof(txth_io_data);

    io_data.cfg = cfg; /* memcpy */
    io_data.stream_size = (get_streamfile_size(sf) - cfg.chunk_start);
    io_data.logical_offset = -1; /* force phys offset reset */
    //io_data.logical_size = io_data.stream_size / cfg.chunk_count; //todo would help with performance but not ok if data_size is set

    /* setup subfile */
    if (!is_opened_streamfile) {
        /* if sf was opened by txth code we MUST close it once done (as it's now "fused"),
         * otherwise it was external to txth and must be wrapped to avoid closing it */
        new_sf = open_wrap_streamfile(sf);
    }
    else {
        new_sf = sf; /* can be closed */
    }

    new_sf = open_io_streamfile(new_sf, &io_data,io_data_size, txth_io_read,txth_io_size);
    new_sf = open_buffer_streamfile_f(new_sf, 0); /* big speedup when used with interleaved codecs */
    return new_sf;
}

#endif /* _TXTH_STREAMFILE_H_ */
