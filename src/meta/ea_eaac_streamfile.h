#ifndef _EA_EAAC_STREAMFILE_H_
#define _EA_EAAC_STREAMFILE_H_
#include "../streamfile.h"


typedef struct {
    /* state */
    off_t logical_offset; /* offset that corresponds to physical_offset */
    off_t physical_offset; /* actual file offset */

    /* config */
    int version;
    int codec;
    off_t start_offset;
    size_t total_size; /* size of the resulting substream */
} eaac_io_data;


/* Reads skipping EA's block headers, so the resulting data is smaller or larger than physical data.
 * physical/logical_offset should always be at the start of a block and only advance when a block is fully done */
static size_t eaac_io_read(STREAMFILE *streamfile, uint8_t *dest, off_t offset, size_t length, eaac_io_data* data) {
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
        uint32_t block_flag, block_size, data_size, skip_size;

        block_flag =    read_8bit(data->physical_offset+0x00,streamfile);
        block_size = read_32bitBE(data->physical_offset+0x00,streamfile) & 0x00FFFFFF;

        if (data->version == 1 && block_flag == 0x48) {
            data->physical_offset += block_size;
            continue; /* skip header block */
        }
        if (data->version == 1 && block_flag == 0x45)
            return total_read; /* stop on last block (always empty) */

        switch(data->codec) {
#if 0
            case 0x03:
                data_size = block_size - ???;
                extra_size = (data_size % 0x800); /* deflated padding */


                skip_size = 0x08 + 0x04*data->stream_count;
                break;
#endif

            case 0x05: /* EALayer3 v1 */
            case 0x06: /* EALayer3 v2 "PCM" */
            case 0x07: /* EALayer3 v2 "Spike" */
                data_size = block_size - 0x08;
                skip_size = 0x08;
                break;

            case 0x0a: /* EATrax */
                data_size = read_32bitBE(data->physical_offset+0x04,streamfile); /* should be block_size - 0x08 */
                skip_size = 0x08;
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

        if (data->version == 0 && block_flag == 0x80)
            break; /* stop on last block */
    }

    return total_read;
}

static size_t eaac_io_size(STREAMFILE *streamfile, eaac_io_data* data) {
    off_t physical_offset, max_physical_offset;
    size_t total_size = 0;

    if (data->total_size)
        return data->total_size;

    physical_offset = data->start_offset;
    max_physical_offset = get_streamfile_size(streamfile) - data->start_offset;

    /* get size of the underlying, non-blocked data */
    while (physical_offset < max_physical_offset) {
        uint32_t block_flag, block_size, data_size;

        block_flag =    read_8bit(physical_offset+0x00,streamfile);
        block_size = read_32bitBE(physical_offset+0x00,streamfile) & 0x00FFFFFF;

        if (data->version == 0 && block_flag != 0x00 && block_flag != 0x80)
            break; /* data/end block expected */

        if (data->version == 1 && block_flag == 0x48) {
            physical_offset += block_size;
            continue; /* skip header block */
        }
        if (data->version == 1 && block_flag == 0x45)
            break; /* stop on last block (always empty) */
        if (data->version == 1 && block_flag != 0x44)
            break; /* data block expected */

        switch(data->codec) {
#if 0
            case 0x03:
                data_size = block_size - ???;
                data_size += (data_size % 0x800); /* deflated padding */
                break;
#endif
            case 0x05: /* EALayer3 v1 */
            case 0x06: /* EALayer3 v2 "PCM" */
            case 0x07: /* EALayer3 v2 "Spike" */
                data_size = block_size - 0x08;
                break;

            case 0x0a: /* EATrax */
                data_size = read_32bitBE(physical_offset+0x04,streamfile); /* should be block_size - 0x08 */
                break;

            default:
                return 0;
        }

        physical_offset += block_size;
        total_size += data_size;

        if (data->version == 0 && block_flag == 0x80)
            break; /* stop on last block */
    }

    data->total_size = total_size;
    return data->total_size;
}


/* Prepares custom IO for some blocked EAAudioCore formats, that need clean reads without block headers:
 * - EA-XMA: deflated XMA in multistreams (separate 2ch frames)
 * - EALayer3: MPEG granule 1 can go in the next block (in V2"P" mainly, others could use layout blocked_sns)
 * - EATrax: ATRAC9 frames can be split between blooks
 */
static STREAMFILE* setup_eaac_streamfile(STREAMFILE *streamFile, int version, int codec, off_t start_offset, size_t total_size) {
    STREAMFILE *temp_streamFile = NULL, *new_streamFile = NULL;
    eaac_io_data io_data = {0};
    size_t io_data_size = sizeof(eaac_io_data);

    io_data.version = version;
    io_data.codec = codec;
    io_data.start_offset = start_offset;
    io_data.total_size = total_size; /* optional */
    io_data.physical_offset = start_offset;

    /* setup subfile */
    new_streamFile = open_wrap_streamfile(streamFile);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_io_streamfile(temp_streamFile, &io_data,io_data_size, eaac_io_read,eaac_io_size);
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

#endif /* _EA_EAAC_STREAMFILE_H_ */
