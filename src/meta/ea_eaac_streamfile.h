#ifndef _EA_EAAC_EATRAX_STREAMFILE_H_
#define _EA_EAAC_EATRAX_STREAMFILE_H_
#include "../streamfile.h"


typedef struct {
    /* state */
    off_t logical_offset; /* offset that corresponds to physical_offset */
    off_t physical_offset; /* actual file offset */

    size_t total_size; /* size of the resulting substream */
} eatrax_io_data;


/* Reads skipping EA's block headers, so the resulting data is smaller than physical data,
 * and physical_offset is bigger than offset (ex. reads at offset = 0x00 could be at physical_offset = 0x10).
 * physical/logical_offset should always be at the start of a block and only advance when a block is fully done */
static size_t eatrax_io_read(STREAMFILE *streamfile, uint8_t *dest, off_t offset, size_t length, eatrax_io_data* data) {
    size_t total_read = 0;

    /* ignore bad reads */
    if (offset < 0 || offset > data->total_size) {
        return 0;
    }

    /* previous offset: re-start as we can't map logical<>physical offsets
     * (kinda slow as it trashes buffers, but shouldn't happen often) */
    if (offset < data->logical_offset) {
        data->physical_offset = 0x00;
        data->logical_offset = 0x00;
    }

    /* read doing one EA block at a time */
    while (length > 0) {
        size_t to_read, bytes_read;
        off_t intrablock_offset, intradata_offset;
        uint32_t block_size, block_flag, data_size;

        block_flag =    read_8bit(data->physical_offset+0x00,streamfile);
        block_size = read_32bitBE(data->physical_offset+0x00,streamfile) & 0x00FFFFFF;
        data_size  = read_32bitBE(data->physical_offset+0x04,streamfile); /* typically block_size - 0x08 */


        /* skip header block */
        if (block_flag == 0x48) {
            data->physical_offset += block_size;
            continue;
        }
        /* stop on footer block */
        if (block_flag == 0x45) {
            data->physical_offset += block_size;
            return total_read;
        }
        /* data block expected */
        if (block_flag != 0x44) {
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
        intrablock_offset = 0x08 + intradata_offset;

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
            data->physical_offset += block_size;
            data->logical_offset += data_size;
        }
    }

    return total_read;
}

static size_t eatrax_io_size(STREAMFILE *streamfile, eatrax_io_data* data) {
    return data->total_size;
}


/* Prepares custom IO for EATrax, which is simply blocked ATRAC9 data, but blocks
 * may end in the middle of an ATRAC9 frame, so reads remove their headers */
static STREAMFILE* setup_eatrax_streamfile(STREAMFILE *streamFile, size_t total_size) {
    STREAMFILE *temp_streamFile = NULL, *new_streamFile = NULL;
    eatrax_io_data io_data = {0};
    size_t io_data_size = sizeof(eatrax_io_data);

    io_data.total_size = total_size;

    /* setup subfile */
    new_streamFile = open_wrap_streamfile(streamFile);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_io_streamfile(temp_streamFile, &io_data,io_data_size, eatrax_io_read,eatrax_io_size);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    return temp_streamFile;

fail:
    close_streamfile(temp_streamFile);
    return NULL;
}

#endif /* _EA_EAAC_STREAMFILE_H_ */
