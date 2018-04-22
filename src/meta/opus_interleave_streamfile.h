#ifndef _OPUS_CAPCOM_STREAMFILE_H_
#define _OPUS_CAPCOM_STREAMFILE_H_
#include "../streamfile.h"


typedef struct {
    /* state */
    off_t logical_offset; /* offset that corresponds to physical_offset */
    off_t physical_offset; /* actual file offset */
    int skip_frames; /* frames to skip from other streams at points */

    /* config */
    int version;
    int streams;
    off_t start_offset; /* pointing to the stream's beginning */
    size_t total_size; /* size of the resulting substream */
} opus_interleave_io_data;


/* Reads skipping other streams */
static size_t opus_interleave_io_read(STREAMFILE *streamfile, uint8_t *dest, off_t offset, size_t length, opus_interleave_io_data* data) {
    size_t total_read = 0;

    /* ignore bad reads */
    if (offset < 0 || offset > data->total_size) {
        return total_read;
    }

    /* previous offset: re-start as we can't map logical<>physical offsets, since it may be VBR
     * (kinda slow as it trashes buffers, but shouldn't happen often) */
    if (offset < data->logical_offset) {
        data->physical_offset = data->start_offset;
        data->logical_offset = 0x00;
    }

    /* read doing one frame at a time */
    while (length > 0) {
        size_t to_read, bytes_read;
        off_t intrablock_offset, intradata_offset;
        uint32_t data_size;

        data_size = read_32bitBE(data->physical_offset+0x00,streamfile);

        //if (offset >= data->total_size) //todo fix
        //    return total_read;

        /* Nintendo Opus header rather than a frame */
        if ((uint32_t)data_size == 0x01000080) {
            data_size = read_32bitLE(data->physical_offset+0x10,streamfile);
            data_size += 0x08;
        }
        else {
            data_size += 0x08;
        }

        /* skip frames from other streams */
        if (data->skip_frames) {
            data->physical_offset += data_size;
            data->skip_frames--;
            continue;
        }

        /* requested offset is outside current block, try next */
        if (offset >= data->logical_offset + data_size) {
            data->physical_offset += data_size;
            data->logical_offset += data_size;
            data->skip_frames = data->streams - 1;
            continue;
        }

        /* reads could fall in the middle of the block */
        intradata_offset = offset - data->logical_offset;
        intrablock_offset = intradata_offset;

        /* clamp reads up to this block's end */
        to_read = (data_size - intradata_offset);
        if (to_read > length)
            to_read = length;
        if (to_read == 0)
            return total_read; /* should never happen... */

        /* finally read and move buffer/offsets */
        bytes_read = read_streamfile(dest, data->physical_offset + intrablock_offset, to_read, streamfile);
        total_read += bytes_read;
        if (bytes_read != to_read)
            return total_read; /* couldn't read fully */

        dest += bytes_read;
        offset += bytes_read;
        length -= bytes_read;

        /* block fully read, go next */
        if (intradata_offset + bytes_read == data_size) {
            data->physical_offset += data_size;
            data->logical_offset += data_size;
            data->skip_frames = data->streams - 1;
        }
    }

    return total_read;
}

static size_t opus_interleave_io_size(STREAMFILE *streamfile, opus_interleave_io_data* data) {
    off_t info_offset;

    if (data->total_size)
        return data->total_size;

    info_offset = read_32bitLE(data->start_offset+0x10,streamfile);
    return read_32bitLE(data->start_offset + info_offset+0x04,streamfile);
}


/* Prepares custom IO for multistream Opus, interleaves 1 packet per stream */
static STREAMFILE* setup_opus_interleave_streamfile(STREAMFILE *streamFile, off_t start_offset, int streams) {
    STREAMFILE *temp_streamFile = NULL, *new_streamFile = NULL;
    opus_interleave_io_data io_data = {0};
    size_t io_data_size = sizeof(opus_interleave_io_data);

    io_data.start_offset = start_offset;
    io_data.streams = streams;
    io_data.physical_offset = start_offset;
    io_data.total_size = opus_interleave_io_size(streamFile, &io_data); /* force init */


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

#endif /* _OPUS_CAPCOM_STREAMFILE_H_ */
