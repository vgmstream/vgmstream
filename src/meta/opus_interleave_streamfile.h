#ifndef _OPUS_INTERLEAVE_STREAMFILE_H_
#define _OPUS_INTERLEAVE_STREAMFILE_H_
#include "../streamfile.h"


typedef struct {
    /* config */
    int streams;
    off_t stream_offset;

    /* state */
    off_t logical_offset;       /* offset that corresponds to physical_offset */
    off_t physical_offset;      /* actual file offset */
    int skip_frames;            /* frames to skip from other streams at points */

    size_t logical_size;
} opus_interleave_io_data;


/* Reads skipping other streams */
static size_t opus_interleave_io_read(STREAMFILE *streamfile, uint8_t *dest, off_t offset, size_t length, opus_interleave_io_data* data) {
    size_t total_read = 0;

    /* ignore bad reads */
    if (offset < 0 || offset > data->logical_size) {
        return total_read;
    }

    /* previous offset: re-start as we can't map logical<>physical offsets (may be VBR) */
    if (offset < data->logical_offset) {
        data->physical_offset = data->stream_offset;
        data->logical_offset = 0x00;
        data->skip_frames = 0;
    }

    /* read doing one frame at a time */
    while (length > 0) {
        size_t data_size;

        /* ignore EOF */
        if (data->logical_offset >= data->logical_size) {
            break;
        }

        /* process block (must be read every time since skip frame sizes may vary) */
        {
            data_size = read_32bitBE(data->physical_offset,streamfile);
            if ((uint32_t)data_size == 0x01000080) //todo not ok if offset between 0 and header_size
                data_size = read_32bitLE(data->physical_offset+0x10,streamfile) + 0x08;
            else
                data_size += 0x08;
        }

        /* skip frames from other streams */
        if (data->skip_frames) {
            data->physical_offset += data_size;
            data->skip_frames--;
            continue;
        }

        /* move to next block */
        if (offset >= data->logical_offset + data_size) {
            data->physical_offset += data_size;
            data->logical_offset += data_size;
            data->skip_frames = data->streams - 1;
            continue;
        }

        /* read data */
        {
            size_t bytes_consumed, bytes_done, to_read;

            bytes_consumed = offset - data->logical_offset;
            to_read = data_size - bytes_consumed;
            if (to_read > length)
                to_read = length;
            bytes_done = read_streamfile(dest, data->physical_offset + bytes_consumed, to_read, streamfile);

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

static size_t opus_interleave_io_size(STREAMFILE *streamfile, opus_interleave_io_data* data) {
    off_t info_offset;

    if (data->logical_size)
        return data->logical_size;

    info_offset = read_32bitLE(data->stream_offset+0x10,streamfile);
    data->logical_size = (0x08+info_offset) + read_32bitLE(data->stream_offset+info_offset+0x04,streamfile);
    return data->logical_size;
}


/* Prepares custom IO for multistream, interleaves 1 packet per stream */
static STREAMFILE* setup_opus_interleave_streamfile(STREAMFILE *streamFile, off_t start_offset, int streams) {
    STREAMFILE *temp_streamFile = NULL, *new_streamFile = NULL;
    opus_interleave_io_data io_data = {0};
    size_t io_data_size = sizeof(opus_interleave_io_data);

    io_data.stream_offset = start_offset;
    io_data.streams = streams;
    io_data.physical_offset = start_offset;
    io_data.logical_size = opus_interleave_io_size(streamFile, &io_data); /* force init */

    /* setup subfile */
    new_streamFile = open_wrap_streamfile(streamFile);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_io_streamfile(temp_streamFile, &io_data,io_data_size, opus_interleave_io_read,opus_interleave_io_size);
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

#endif /* _OPUS_INTERLEAVE_STREAMFILE_H_ */
