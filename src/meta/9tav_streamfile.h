#ifndef _9TAV_STREAMFILE_H_
#define _9TAV_STREAMFILE_H_
#include "../streamfile.h"


typedef struct {
    /* config */
    off_t stream_offset;
    size_t stream_size;
    size_t track_size;
    int track_number;
    int track_count;
    int skip_count;
    int read_count;
    size_t frame_size;
    size_t interleave_count;
    size_t interleave_last_count;

    /* state */
    off_t logical_offset;       /* fake offset */
    off_t physical_offset;      /* actual offset */
    size_t block_size;          /* current size */
    size_t skip_size;           /* size from block start to reach data */
    size_t data_size;           /* usable size in a block */

    size_t logical_size;
} ntav_io_data;


static size_t ntav_io_read(STREAMFILE *streamfile, uint8_t *dest, off_t offset, size_t length, ntav_io_data* data) {
    size_t total_read = 0;


    /* re-start when previous offset (can't map logical<>physical offsets) */
    if (data->logical_offset < 0 || offset < data->logical_offset) {
        data->physical_offset = data->stream_offset;
        data->logical_offset = 0x00;
        data->data_size = 0;
        data->skip_size = 0;
        data->read_count = 0;
        data->skip_count = data->interleave_count * data->track_number;
        //VGM_LOG("0 o=%lx, sc=%i\n", data->physical_offset, data->skip_count);
    }

    /* read blocks */
    while (length > 0) {
        //VGM_LOG("1 of=%lx, so=%lx, sz=%x, of2=%lx, log=%lx\n", data->physical_offset, data->stream_offset, data->stream_size, offset, data->logical_offset);

        /* ignore EOF */
        if (offset < 0 || data->physical_offset >= data->stream_offset + data->stream_size) {
            //VGM_LOG("9 o=%lx, so=%lx, sz=%x, of2=%lx, log=%lx\n", data->physical_offset, data->stream_offset, data->stream_size, offset, data->logical_offset);
            //VGM_LOG("eof\n");
            break;
        }


        /* process new block */
        if (data->data_size == 0) {
            /* not very exact compared to real blocks but ok enough */
            if (read_32bitLE(data->physical_offset, streamfile) == 0x00) {
                data->block_size = 0x10;
                //VGM_LOG("1 o=%lx, lo=%lx skip\n", data->physical_offset, data->logical_offset);
            }
            else {
                data->block_size = data->frame_size;

                //VGM_LOG("2 o=%lx, lo=%lx, skip=%i, read=%i\n", data->physical_offset, data->logical_offset, data->skip_count, data->read_count);

                /* each track interleaves NTAV_INTERLEAVE frames, but can contain padding in between,
                 * so must read one by one up to max */

                if (data->skip_count == 0 && data->read_count == 0) {
                    data->read_count = data->interleave_count;
                }

                if (data->skip_count) {
                    data->skip_count--;
                }

                if (data->read_count) {
                    data->data_size = data->block_size;
                    data->read_count--;

                    if (data->read_count == 0) {
                        if (data->logical_offset + data->interleave_count * data->frame_size > data->track_size)
                            data->skip_count = data->interleave_last_count * (data->track_count - 1);
                        else
                            data->skip_count = data->interleave_count * (data->track_count - 1);
                    }
                }

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

static size_t ntav_io_size(STREAMFILE *streamfile, ntav_io_data* data) {
    uint8_t buf[1];

    if (data->logical_size)
        return data->logical_size;

    /* force a fake read at max offset, to get max logical_offset (will be reset next read) */
    ntav_io_read(streamfile, buf, 0x7FFFFFFF, 1, data);
    data->logical_size = data->logical_offset;

    return data->logical_size;
}

/* Handles deinterleaving of 9TAV blocked streams. Unlike other games using .sdt,
 * KCEJ blocks have a data_size field and rest is padding.  Even after that all blocks start
 * with 0 (skipped) and there are padding blocks that start with LE 0xDEADBEEF.
 * This streamfile handles 9tav extracted like regular sdt and remove padding manually. */
static STREAMFILE* setup_9tav_streamfile(STREAMFILE *streamFile, off_t stream_offset, size_t track_size, int track_number, int track_count) {
    STREAMFILE *temp_streamFile = NULL, *new_streamFile = NULL;
    ntav_io_data io_data = {0};
    size_t io_data_size = sizeof(ntav_io_data);
    size_t last_size;

    io_data.stream_offset = stream_offset;
    io_data.stream_size = get_streamfile_size(streamFile) - stream_offset;
    io_data.track_size = track_size;
    io_data.track_number = track_number;
    io_data.track_count = track_count;
    io_data.frame_size = 0x40;
    io_data.interleave_count = 256;
    last_size = track_size % (io_data.interleave_count * io_data.frame_size);
    if (last_size)
        io_data.interleave_last_count = last_size / io_data.frame_size;
    io_data.logical_offset = -1; /* force state reset */

    /* setup subfile */
    new_streamFile = open_wrap_streamfile(streamFile);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_io_streamfile(new_streamFile, &io_data,io_data_size, ntav_io_read,ntav_io_size);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_buffer_streamfile(new_streamFile,0);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    return temp_streamFile;

fail:
    close_streamfile(temp_streamFile);
    return NULL;
}

#endif /* _9TAV_STREAMFILE_H_ */
