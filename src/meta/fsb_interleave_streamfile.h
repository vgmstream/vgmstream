#ifndef _FSB_INTERLEAVE_STREAMFILE_H_
#define _FSB_INTERLEAVE_STREAMFILE_H_
#include "../streamfile.h"

typedef enum { FSB_INT_CELT } fsb_interleave_codec_t;
typedef struct {
    /* state */
    off_t logical_offset; /* offset that corresponds to physical_offset */
    off_t physical_offset; /* actual file offset */
    int skip_frames; /* frames to skip from other streams at points */

    /* config */
    fsb_interleave_codec_t codec;
    int stream_count;
    int stream_number;
    size_t stream_size;
    off_t start_offset; /* pointing to the stream's beginning */
    size_t total_size; /* size of the resulting substream */
} fsb_interleave_io_data;



/* Reads skipping other streams */
static size_t fsb_interleave_io_read(STREAMFILE *streamfile, uint8_t *dest, off_t offset, size_t length, fsb_interleave_io_data* data) {
    size_t total_read = 0;

    /* ignore bad reads */
    if (offset < 0 || offset > data->total_size) {
        return total_read;
    }

    /* previous offset: re-start as we can't map logical<>physical offsets
     * (kinda slow as it trashes buffers, but shouldn't happen often) */
    if (offset < data->logical_offset) {
        data->physical_offset = data->start_offset;
        data->logical_offset = 0x00;
        data->skip_frames = data->stream_number;
    }

    /* read doing one frame at a time */
    while (length > 0) {
        size_t to_read, bytes_read;
        off_t intrablock_offset, intradata_offset;
        uint32_t data_size;

        if (offset >= data->total_size)
            break;

        /* get current data */
        switch (data->codec) {
            case FSB_INT_CELT:
                data_size = 0x04+0x04+read_32bitLE(data->physical_offset+0x04,streamfile);
                break;

            default:
                return 0;
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
            data->skip_frames = data->stream_count - 1;
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
            break; /* should never happen... */

        /* finally read and move buffer/offsets */
        bytes_read = read_streamfile(dest, data->physical_offset + intrablock_offset, to_read, streamfile);
        total_read += bytes_read;
        if (bytes_read != to_read)
            break; /* couldn't read fully */

        dest += bytes_read;
        offset += bytes_read;
        length -= bytes_read;

        /* block fully read, go next */
        if (intradata_offset + bytes_read == data_size) {
            data->physical_offset += data_size;
            data->logical_offset += data_size;
            data->skip_frames = data->stream_count - 1;
        }
    }

    return total_read;
}

static size_t fsb_interleave_io_size(STREAMFILE *streamfile, fsb_interleave_io_data* data) {
    off_t physical_offset, max_physical_offset;
    size_t total_size = 0;
    int skip_frames = 0;

    if (data->total_size)
        return data->total_size;

    physical_offset = data->start_offset;
    max_physical_offset = physical_offset + data->stream_size;
    skip_frames = data->stream_number;

    /* get size of the underlying stream, skipping other streams
     * (all streams should have the same frame count) */
    while (physical_offset < max_physical_offset) {
        size_t data_size;
        uint32_t id;

        switch(data->codec) {
            case FSB_INT_CELT:
                id = read_32bitBE(physical_offset+0x00,streamfile);
                if (id != 0x17C30DF3) /* incorrect FSB CELT frame sync */
                    data_size = 0;
                else
                    data_size = 0x04+0x04+read_32bitLE(physical_offset+0x04,streamfile);
                break;

            default:
                return 0;
        }

        /* there may be padding at the end, so this doubles as EOF marker */
        if (data_size == 0)
            break;

        /* skip frames from other streams */
        if (skip_frames) {
            physical_offset += data_size;
            skip_frames--;
            continue;
        }

        physical_offset += data_size;
        total_size += data_size;
        skip_frames = data->stream_count - 1;
    }

    data->total_size = total_size;
    return data->total_size;
}


/* Prepares custom IO for multistreams, interleaves 1 packet per stream */
static STREAMFILE* setup_fsb_interleave_streamfile(STREAMFILE *streamFile, off_t start_offset, size_t stream_size, int stream_count, int stream_number, fsb_interleave_codec_t codec) {
    STREAMFILE *temp_streamFile = NULL, *new_streamFile = NULL;
    fsb_interleave_io_data io_data = {0};
    size_t io_data_size = sizeof(fsb_interleave_io_data);

    io_data.start_offset = start_offset;
    io_data.physical_offset = start_offset;
    io_data.skip_frames = stream_number; /* adjust since start_offset points to the first */
    io_data.codec = codec;
    io_data.stream_count = stream_count;
    io_data.stream_number = stream_number;
    io_data.stream_size = stream_size;
    io_data.total_size = fsb_interleave_io_size(streamFile, &io_data); /* force init */

    if (io_data.total_size == 0 || io_data.total_size > io_data.stream_size) {
        VGM_LOG("FSB INTERLEAVE: wrong total_size %x vs %x\n", io_data.total_size,io_data.stream_size);
        goto fail;
    }

    /* setup subfile */
    new_streamFile = open_wrap_streamfile(streamFile);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_io_streamfile(temp_streamFile, &io_data,io_data_size, fsb_interleave_io_read,fsb_interleave_io_size);
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

#endif /* _FSB_INTERLEAVE_STREAMFILE_H_ */
