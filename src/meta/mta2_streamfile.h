#ifndef _MTA2_STREAMFILE_H_
#define _MTA2_STREAMFILE_H_
#include "../streamfile.h"

typedef struct {
    /* config */
    int big_endian;
    uint32_t target_type;
    off_t stream_offset;
    size_t stream_size;

    /* state */
    off_t logical_offset;       /* fake offset */
    off_t physical_offset;      /* actual offset */
    size_t block_size;          /* current size */
    size_t skip_size;           /* size from block start to reach data */
    size_t data_size;           /* usable size in a block */

    size_t logical_size;
} mta2_io_data;


static size_t mta2_io_read(STREAMFILE *sf, uint8_t *dest, off_t offset, size_t length, mta2_io_data* data) {
    size_t total_read = 0;
    uint32_t (*read_u32)(off_t,STREAMFILE*) = data->big_endian ? read_u32be : read_u32le;


    /* re-start when previous offset (can't map logical<>physical offsets) */
    if (data->logical_offset < 0 || offset < data->logical_offset) {
        ;VGM_LOG("IO restart: offset=%lx + %x, po=%lx, lo=%lx\n", offset, length, data->physical_offset, data->logical_offset);
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
            uint32_t block_type, block_size, block_track;

            block_type  = read_u32(data->physical_offset+0x00, sf); /* subtype and type */
            block_size  = read_u32(data->physical_offset+0x04, sf);
          //block_unk   = read_u32(data->physical_offset+0x08, streamfile); /* usually 0 except for 0xF0 'end' block */
            block_track = read_u32(data->physical_offset+0x0c, sf);

            if (block_type != data->target_type || block_size == 0xFFFFFFFF)
                break;

            data->block_size = block_size;
            data->skip_size = 0x10;
            data->data_size = block_size - data->skip_size;
            /* no audio data (padding block), but write first (header) */
            if (block_track == 0 && data->logical_offset > 0)
                data->data_size = 0;
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

static size_t mta2_io_size(STREAMFILE *streamfile, mta2_io_data* data) {
    uint8_t buf[1];

    if (data->logical_size > 0)
        return data->logical_size;

    /* force a fake read at max offset, to get max logical_offset (will be reset next read) */
    mta2_io_read(streamfile, buf, 0x7FFFFFFF, 1, data);
    data->logical_size = data->logical_offset;

    return data->logical_size;
}

/* Handles removing KCE Japan-style blocks in MTA2 streams
 * (these blocks exist in most KCEJ games and aren't actually related to audio) */
static STREAMFILE* setup_mta2_streamfile(STREAMFILE *sf, off_t stream_offset, int big_endian, const char *extension) {
    STREAMFILE *new_sf = NULL;
    mta2_io_data io_data = {0};
    uint32_t (*read_u32)(off_t,STREAMFILE*) = big_endian ? read_u32be : read_u32le;


    /* blocks must start with a 'new sub-stream' id */
    if (read_u32(stream_offset+0x00, sf) != 0x00000010)
        return NULL;

    io_data.target_type = read_u32(stream_offset + 0x0c, sf);
    io_data.stream_offset = stream_offset + 0x10;
    io_data.stream_size = get_streamfile_size(sf) - io_data.stream_offset;
    io_data.big_endian = big_endian;
    io_data.logical_offset = -1; /* force phys offset reset */

    /* setup subfile */
    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_streamfile_f(new_sf, &io_data, sizeof(mta2_io_data), mta2_io_read, mta2_io_size);
    if (extension)
        new_sf = open_fakename_streamfile_f(new_sf, NULL, extension);
    return new_sf;
}

#endif /* _MTA2_STREAMFILE_H_ */
