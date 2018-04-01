#ifndef _AWC_XMA_STREAMFILE_H_
#define _AWC_XMA_STREAMFILE_H_
#include "../streamfile.h"


typedef struct {
    /* config */
    off_t stream_offset;
    size_t stream_size;
    int channel_count;
    int channel;
    size_t chunk_size;

    /* state */
    off_t logical_offset; /* offset that corresponds to physical_offset */
    off_t physical_offset; /* actual file offset */
    off_t next_block_offset; /* physical offset of the next block start */
    off_t last_offset; /* physical offset of where the last block ended */
    size_t current_data_size;
    size_t current_consumed_size;

    size_t total_size; /* size of the resulting XMA data */
} awc_xma_io_data;


static size_t get_block_header_size(STREAMFILE *streamFile, off_t offset, awc_xma_io_data *data);
static size_t get_repeated_data_size(STREAMFILE *streamFile, off_t new_offset, off_t last_offset);
static size_t get_block_skip_count(STREAMFILE *streamFile, off_t offset, int channel);

/* Reads plain XMA data of a single stream. Each block has a header and channels have different num_samples/frames.
 * Channel data is separate within the block (first all frames of ch0, then ch1, etc), padded, and sometimes
 * the last few frames of a channel are repeated in the new block (marked with the "discard samples" field). */
static size_t awc_xma_io_read(STREAMFILE *streamfile, uint8_t *dest, off_t offset, size_t length, awc_xma_io_data* data) {
    size_t total_read = 0;
    size_t frame_size = 0x800;

    /* ignore bad reads */
    if (offset < 0 || offset > data->total_size) {
        return 0;
    }

    /* previous offset: re-start as we can't map logical<>physical offsets
     * (kinda slow as it trashes buffers, but shouldn't happen often) */
    if (offset < data->logical_offset) {
        data->logical_offset = 0x00;
        data->physical_offset = data->stream_offset;
        data->next_block_offset = 0;
        data->last_offset = 0;
        data->current_data_size = 0;
        data->current_consumed_size = 0;
    }

    /* read from block, moving to next when all data is consumed */
    while (length > 0) {
        size_t to_read, bytes_read;

        /* new block */
        if (data->current_data_size == 0) {
            size_t header_size    = get_block_header_size(streamfile, data->physical_offset, data);
            /* header table entries = frames... I hope */
            size_t skip_size      = get_block_skip_count(streamfile, data->physical_offset, data->channel) * frame_size;
          //size_t skip_size      = read_32bitBE(data->physical_offset + 0x10*data->channel + 0x00, streamfile) * frame_size;
            size_t data_size      = read_32bitBE(data->physical_offset + 0x10*data->channel + 0x04, streamfile) * frame_size;
            size_t repeat_samples = read_32bitBE(data->physical_offset + 0x10*data->channel + 0x08, streamfile);
            size_t repeat_size    = 0;

            /* if there are repeat samples current block repeats some frames from last block, find out size */
            if (repeat_samples && data->last_offset) {
                off_t data_offset = data->physical_offset + header_size + skip_size;
                repeat_size = get_repeated_data_size(streamfile, data_offset, data->last_offset);
            }

            data->next_block_offset = data->physical_offset + data->chunk_size;
            data->physical_offset += header_size + skip_size + repeat_size; /* data start */
            data->current_data_size = data_size - repeat_size; /* readable max in this block */
            data->current_consumed_size = 0;
            continue;
        }

        /* block end, go next */
        if (data->current_consumed_size == data->current_data_size) {
            data->last_offset = data->physical_offset; /* where last block ended */
            data->physical_offset = data->next_block_offset;
            data->current_data_size = 0;
            continue;
        }

        /* requested offset is further along, pretend we consumed data and try again */
        if (offset > data->logical_offset) {
            size_t to_consume = offset - data->logical_offset;
            if (to_consume > data->current_data_size - data->current_consumed_size)
                to_consume = data->current_data_size - data->current_consumed_size;

            data->physical_offset += to_consume;
            data->logical_offset += to_consume;
            data->current_consumed_size += to_consume;
            continue;
        }

        /* clamp reads up to this block's end */
        to_read = (data->current_data_size - data->current_consumed_size);
        if (to_read > length)
            to_read = length;
        if (to_read == 0)
            return total_read; /* should never happen... */

        /* finally read and move buffer/offsets */
        bytes_read = read_streamfile(dest, data->physical_offset, to_read, streamfile);
        total_read += bytes_read;
        if (bytes_read != to_read)
            return total_read; /* couldn't read fully */

        dest += bytes_read;
        offset += bytes_read;
        length -= bytes_read;

        data->physical_offset += bytes_read;
        data->logical_offset += bytes_read;
        data->current_consumed_size += bytes_read;
    }

    return total_read;
}

static size_t awc_xma_io_size(STREAMFILE *streamfile, awc_xma_io_data* data) {
    off_t physical_offset, max_physical_offset, last_offset;
    size_t frame_size = 0x800;
    size_t total_size = 0;

    if (data->total_size)
        return data->total_size;

    physical_offset = data->stream_offset;
    max_physical_offset = data->stream_offset + data->stream_size;
    last_offset = 0;

    /* read blocks and sum final size */
    while (physical_offset < max_physical_offset) {
        size_t header_size    = get_block_header_size(streamfile, physical_offset, data);
        /* header table entries = frames... I hope */
        size_t skip_size      = get_block_skip_count(streamfile, physical_offset, data->channel) * frame_size;
      //size_t skip_size      = read_32bitBE(physical_offset + 0x10*data->channel + 0x00, streamfile) * frame_size;
        size_t data_size      = read_32bitBE(physical_offset + 0x10*data->channel + 0x04, streamfile) * frame_size;
        size_t repeat_samples = read_32bitBE(physical_offset + 0x10*data->channel + 0x08, streamfile);
        size_t repeat_size    = 0;

        /* if there are repeat samples current block repeats some frames from last block, find out size */
        if (repeat_samples && last_offset) {
            off_t data_offset = physical_offset + header_size + skip_size;
            repeat_size = get_repeated_data_size(streamfile, data_offset, last_offset);
        }

        last_offset = physical_offset + header_size + skip_size + data_size;
        total_size += data_size - repeat_size;
        physical_offset += data->chunk_size;
    }

    data->total_size = total_size;
    return data->total_size;
}


/* Prepares custom IO for AWC XMA, which is interleaved XMA in AWC blocks */
static STREAMFILE* setup_awc_xma_streamfile(STREAMFILE *streamFile, off_t stream_offset, size_t stream_size, size_t chunk_size, int channel_count, int channel) {
    STREAMFILE *temp_streamFile = NULL, *new_streamFile = NULL;
    awc_xma_io_data io_data = {0};
    size_t io_data_size = sizeof(awc_xma_io_data);

    io_data.stream_offset = stream_offset;
    io_data.stream_size = stream_size;
    io_data.chunk_size = chunk_size;
    io_data.channel_count = channel_count;
    io_data.channel = channel;
    io_data.physical_offset = stream_offset;

    /* setup subfile */
    new_streamFile = open_wrap_streamfile(streamFile);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_io_streamfile(temp_streamFile, &io_data,io_data_size, awc_xma_io_read,awc_xma_io_size);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    //todo maybe should force to read filesize once
    return temp_streamFile;

fail:
    close_streamfile(temp_streamFile);
    return NULL;
}

/* block header size, aligned/padded to 0x800 */
static size_t get_block_header_size(STREAMFILE *streamFile, off_t offset, awc_xma_io_data *data) {
    size_t header_size = 0;
    int i;
    int entries = data->channel_count;

    for (i = 0; i < entries; i++) {
        header_size += 0x10;
        header_size += read_32bitBE(offset + 0x10*i + 0x04, streamFile) * 0x04; /* entries in the table */
    }

    if (header_size % 0x800) /* padded */
        header_size +=  0x800 - (header_size % 0x800);

    return header_size;
}


/* find data that repeats in the beginning of a new block at the end of last block */
static size_t get_repeated_data_size(STREAMFILE *streamFile, off_t new_offset, off_t last_offset) {
    uint8_t new_frame[0x800];/* buffer to avoid fseek back and forth */
    size_t frame_size = 0x800;
    off_t offset;
    int i;

    /* read block first frame */
    if (read_streamfile(new_frame,new_offset, frame_size,streamFile) != frame_size)
        goto fail;

    /* find the frame in last bytes of prev block */
    offset = last_offset - 0x4000; /* typical max is 1 frame of ~0x800, no way to know exact size */
    while (offset < last_offset) {
        /* compare frame vs prev block data */
        for (i = 0; i < frame_size; i++) {
            if ((uint8_t)read_8bit(offset+i,streamFile) != new_frame[i])
                break;
        }

        /* frame fully compared? */
        if (i == frame_size)
            return last_offset - offset;
        else
            offset += i+1;
    }

fail:
    VGM_LOG("AWC: can't find repeat size, new=0x%08lx, last=0x%08lx\n", new_offset, last_offset);
    return 0; /* keep on truckin' */
}

/* header has a skip value, but somehow it's sometimes bigger than expected (WHY!?!?) so just sum all */
static size_t get_block_skip_count(STREAMFILE *streamFile, off_t offset, int channel) {
    size_t skip_count = 0;
    int i;

    //skip_size = read_32bitBE(offset + 0x10*channel + 0x00, streamFile); /* wrong! */
    for (i = 0; i < channel; i++) {
        skip_count += read_32bitBE(offset + 0x10*i + 0x04, streamFile); /* number of frames of this channel */
    }

    return skip_count;
}


#endif /* _AWC_XMA_STREAMFILE_H_ */
