#ifndef _EA_SCHL_STREAMFILE_H_
#define _EA_SCHL_STREAMFILE_H_
#include "../streamfile.h"


typedef struct {
    /* state */
    off_t logical_offset; /* offset that corresponds to physical_offset */
    off_t physical_offset; /* actual file offset */

    /* config */
    int codec;
    int channels;
    off_t start_offset;
    size_t total_size; /* size of the resulting substream */
} schl_io_data;


/* Reads skipping EA's block headers, so the resulting data is smaller or larger than physical data.
 * physical/logical_offset should always be at the start of a block and only advance when a block is fully done */
static size_t schl_io_read(STREAMFILE *streamfile, uint8_t *dest, off_t offset, size_t length, schl_io_data* data) {
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
    }

    /* read doing one EA block at a time */
    while (length > 0) {
        size_t to_read, bytes_read;
        off_t intrablock_offset, intradata_offset;
        uint32_t block_id, block_size, data_size, skip_size;

        block_id   = (uint32_t)read_32bitBE(data->physical_offset+0x00,streamfile);
        block_size = read_32bitLE(data->physical_offset+0x04,streamfile); /* always LE, hopefully */

        if (block_id == 0x5343456C) /* "SCEl" */
            break; /* end block (no need to look for more SCHl for codecs needed this custom IO) */

        if (block_id != 0x5343446C) { /* "SCDl" */
            data->physical_offset += block_size;
            continue; /* skip non-data blocks */
        }

        switch(data->codec) {
            case 0x1b: /* ATRAC3plus */
                data_size = read_32bitLE(data->physical_offset+0x0c+0x04*data->channels,streamfile);
                skip_size = 0x0c+0x04*data->channels+0x04;
                break;
            default:
                return total_read;
        }

        /* requested offset is outside current block, try next */
        if (offset >= data->logical_offset + data_size) {
            data->physical_offset += block_size;
            data->logical_offset += data_size;
            continue;
        }

        /* reads could fall in the middle of the block */
        intradata_offset = offset - data->logical_offset;
        intrablock_offset = skip_size + intradata_offset;

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
            data->physical_offset += block_size;
            data->logical_offset += data_size;
        }
    }

    return total_read;
}

static size_t schl_io_size(STREAMFILE *streamfile, schl_io_data* data) {
    off_t physical_offset, max_physical_offset;
    size_t total_size = 0;

    if (data->total_size)
        return data->total_size;

    physical_offset = data->start_offset;
    max_physical_offset = get_streamfile_size(streamfile);

    /* get size of the underlying, non-blocked data */
    while (physical_offset < max_physical_offset) {
        uint32_t block_id, block_size, data_size;

        block_id   = (uint32_t)read_32bitBE(physical_offset+0x00,streamfile);
        block_size = read_32bitLE(physical_offset+0x04,streamfile); /* always LE, hopefully */

        if (block_id == 0x5343456C) /* "SCEl" */
            break; /* end block (no need to look for more SCHl for codecs needed this custom IO) */

        if (block_id != 0x5343446C) { /* "SCDl" */
            physical_offset += block_size;
            continue; /* skip non-data blocks */
        }

        switch(data->codec) {
            case 0x1b: /* ATRAC3plus */
                data_size = read_32bitLE(physical_offset+0x0c+0x04*data->channels,streamfile);
                break;
            default:
                return 0;
        }

        physical_offset += block_size;
        total_size += data_size;
    }


    if (total_size > get_streamfile_size(streamfile)) {
        VGM_LOG("EA SCHL: wrong streamfile total_size\n");
        total_size = 0;
    }

    data->total_size = total_size;
    return data->total_size;
}


/* Prepares custom IO for some blocked SCHl formats, that need clean reads without block headers.
 * Basically done to feed FFmpeg clean ATRAC3plus.
 */
static STREAMFILE* setup_schl_streamfile(STREAMFILE *streamFile, int codec, int channels, off_t start_offset, size_t total_size) {
    STREAMFILE *temp_streamFile = NULL, *new_streamFile = NULL;
    schl_io_data io_data = {0};
    size_t io_data_size = sizeof(schl_io_data);

    io_data.codec = codec;
    io_data.channels = channels;
    io_data.start_offset = start_offset;
    io_data.total_size = total_size; /* optional */
    io_data.physical_offset = start_offset;

    /* setup subfile */
    new_streamFile = open_wrap_streamfile(streamFile);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_io_streamfile(temp_streamFile, &io_data,io_data_size, schl_io_read,schl_io_size);
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

#endif /* _EA_SCHL_STREAMFILE_H_ */
