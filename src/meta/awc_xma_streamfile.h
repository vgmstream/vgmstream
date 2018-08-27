#ifndef _AWC_XMA_STREAMFILE_H_
#define _AWC_XMA_STREAMFILE_H_
#include "../streamfile.h"


typedef struct {
    /* config */
    int channel;
    int channel_count;
    size_t block_size;
    off_t stream_offset;
    size_t stream_size;

    /* state */
    off_t logical_offset;   /* offset that corresponds to physical_offset */
    off_t physical_offset;  /* actual file offset */

    size_t skip_size;       /* size to skip from a block start to reach data start */
    size_t data_size;       /* logical size of the block  */

    size_t logical_size;
} awc_xma_io_data;


static size_t get_block_header_size(STREAMFILE *streamFile, off_t offset, awc_xma_io_data *data);
static size_t get_repeated_data_size(STREAMFILE *streamFile, off_t next_offset, size_t repeat_samples);
static size_t get_block_skip_count(STREAMFILE *streamFile, off_t offset, int channel);

/* Reads plain XMA data of a single stream. Each block has a header and channels have different num_samples/frames.
 * Channel data is separate within the block (first all frames of ch0, then ch1, etc), padded, and sometimes
 * the last few frames of a channel are repeated in the new block (marked with the "discard samples" field). */
static size_t awc_xma_io_read(STREAMFILE *streamfile, uint8_t *dest, off_t offset, size_t length, awc_xma_io_data* data) {
    size_t total_read = 0;
    size_t frame_size = 0x800;

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
            size_t header_size    = get_block_header_size(streamfile, data->physical_offset, data);
            /* header table entries = frames... I hope */
            size_t others_size    = get_block_skip_count(streamfile, data->physical_offset, data->channel) * frame_size;
          //size_t skip_size      = read_32bitBE(data->physical_offset + 0x10*data->channel + 0x00, streamfile) * frame_size;
            size_t data_size      = read_32bitBE(data->physical_offset + 0x10*data->channel + 0x04, streamfile) * frame_size;
            size_t repeat_samples = read_32bitBE(data->physical_offset + 0x10*data->channel + 0x08, streamfile);
            size_t repeat_size    = 0;


            /* if there are repeat samples current block repeats some frames from last block, find out size */
            if (repeat_samples) {
                off_t data_offset = data->physical_offset + header_size + others_size;
                repeat_size = get_repeated_data_size(streamfile, data_offset, repeat_samples);
            }

            data->skip_size = header_size + others_size + repeat_size;
            data->data_size = data_size - repeat_size;
        }

        /* move to next block */
        if (offset >= data->logical_offset + data->data_size) {
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

static size_t awc_xma_io_size(STREAMFILE *streamfile, awc_xma_io_data* data) {
    off_t physical_offset, max_physical_offset;
    size_t frame_size = 0x800;
    size_t logical_size = 0;

    if (data->logical_size)
        return data->logical_size;

    physical_offset = data->stream_offset;
    max_physical_offset = data->stream_offset + data->stream_size;

    /* get size of the logical stream */
    while (physical_offset < max_physical_offset) {
        size_t header_size    = get_block_header_size(streamfile, physical_offset, data);
        /* header table entries = frames... I hope */
        size_t skip_size      = get_block_skip_count(streamfile, physical_offset, data->channel) * frame_size;
      //size_t skip_size      = read_32bitBE(physical_offset + 0x10*data->channel + 0x00, streamfile) * frame_size;
        size_t data_size      = read_32bitBE(physical_offset + 0x10*data->channel + 0x04, streamfile) * frame_size;
        size_t repeat_samples = read_32bitBE(physical_offset + 0x10*data->channel + 0x08, streamfile);
        size_t repeat_size    = 0;

        /* if there are repeat samples current block repeats some frames from last block, find out size */
        if (repeat_samples) {
            off_t data_offset = physical_offset + header_size + skip_size;
            repeat_size = get_repeated_data_size(streamfile, data_offset, repeat_samples);
        }

        logical_size += data_size - repeat_size;
        physical_offset += data->block_size;
    }

    data->logical_size = logical_size;
    return data->logical_size;
}


/* Prepares custom IO for AWC XMA, which is interleaved XMA in AWC blocks */
static STREAMFILE* setup_awc_xma_streamfile(STREAMFILE *streamFile, off_t stream_offset, size_t stream_size, size_t block_size, int channel_count, int channel) {
    STREAMFILE *temp_streamFile = NULL, *new_streamFile = NULL;
    awc_xma_io_data io_data = {0};
    size_t io_data_size = sizeof(awc_xma_io_data);

    io_data.channel = channel;
    io_data.channel_count = channel_count;
    io_data.stream_offset = stream_offset;
    io_data.stream_size = stream_size;
    io_data.block_size = block_size;
    io_data.physical_offset = stream_offset;
    io_data.logical_size = awc_xma_io_size(streamFile, &io_data); /* force init */

    if (io_data.logical_size > io_data.stream_size) {
        VGM_LOG("AWC XMA: wrong logical size\n");
        goto fail;
    }

    /* setup subfile */
    new_streamFile = open_wrap_streamfile(streamFile);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_io_streamfile(temp_streamFile, &io_data,io_data_size, awc_xma_io_read,awc_xma_io_size);
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
static size_t get_repeated_data_size(STREAMFILE *streamFile, off_t next_offset, size_t repeat_samples) {
    const size_t frame_size = 0x800;
    const size_t samples_per_subframe = 512;
    size_t samples_this_frame;
    uint8_t subframes;

    //todo: fix this
    /* Repeat samples are the number of decoded samples to discard, but in this streamfile we can't do that.
     * Also XMA is VBR, and may encode silent frames with up to 63 subframes yet we may have few repeat samples.
     * We could find out how many subframes of 512 samples to skip, then adjust the XMA frame header, though
     * output will be slightly off since subframes are related.
     *
     * For now just skip a full frame depending on the number of subframes vs repeat samples.
     * Most files work ok-ish but channels may desync slightly. */

    subframes = ((uint8_t)read_8bit(next_offset,streamFile) >> 2) & 0x3F; /* peek into frame header */
    samples_this_frame = subframes*samples_per_subframe;
    if (repeat_samples >= (int)(samples_this_frame*0.13)) { /* skip mosts */
        return frame_size;
    }
    else {
        return 0;
    }
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
