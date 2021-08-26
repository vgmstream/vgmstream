#include "deblock_streamfile.h"
#include "../util/log.h"

//todo move to utils or something

static void block_callback_default(STREAMFILE* sf, deblock_io_data* data) {
    data->block_size = data->cfg.chunk_size;
    data->skip_size = data->cfg.skip_size;
    data->data_size = data->block_size - data->skip_size;
    
    //;VGM_LOG("DEBLOCK: of=%lx, bs=%lx, ss=%lx, ds=%lx\n", data->physical_offset, data->block_size, data->skip_size, data->data_size);
}

static size_t deblock_io_read(STREAMFILE* sf, uint8_t* dest, off_t offset, size_t length, deblock_io_data* data) {
    size_t total_read = 0;

    //;VGM_LOG("DEBLOCK: of=%lx, sz=%x, po=%lx\n", offset, length, data->physical_offset);

    /* re-start when previous offset (can't map logical<>physical offsets) */
    if (data->logical_offset < 0 || offset < data->logical_offset) {
        ;VGM_LOG("DEBLOCK: restart offset=%lx + %x, po=%lx, lo=%lx\n", offset, length, data->physical_offset, data->logical_offset);
        data->physical_offset = data->cfg.stream_start;
        data->logical_offset = 0x00;
        data->block_size = 0;
        data->data_size = 0;
        data->skip_size = 0;
        data->chunk_size = 0;

        data->step_count = data->cfg.step_start;
        //data->read_count = data->cfg.read_count;
    }

    /* read blocks */
    while (length > 0) {

        /* ignore EOF */
        if (offset < 0 ||
                (data->physical_offset >= data->cfg.stream_start + data->physical_size) ||
                (data->logical_size > 0 && offset > data->logical_size)) {
            break;
        }

        /* process new block */
        if (data->data_size <= 0) {
            data->cfg.block_callback(sf, data);

            if (data->block_size <= 0) {
                VGM_LOG("DEBLOCK: block size not set at %lx\n", data->physical_offset);
                break;
            }
        }

#if 1
        if (data->step_count > 0) {
            data->step_count--;
            data->physical_offset += data->block_size;
            data->data_size = 0;
            continue;
        }
#else
        /* handle blocks from multiple streams */
        {
            if (data->step_count > 0) {
                data->step_count--;
                data->data_size = 0; /* step over this block */
            }
            else if (data->read_count) {//must detect when blocks has been read
                data->read_count--; /* read this block */

                /* reset */
                if (data->step_count == 0 && data->read_count == 0) {
                    data->step_count = data->cfg.step_count;
                    data->read_count = data->cfg.read_count;
                }
            }
        }
#endif
        /* move to next block */
        if (data->data_size == 0 || offset >= data->logical_offset + data->data_size) {
            data->physical_offset += data->block_size;
            data->logical_offset += data->data_size;
            data->data_size = 0;

            data->step_count = data->cfg.step_count;
            //VGM_LOG("ignore at %lx + %lx, skips=%i\n", data->physical_offset, data->block_size, data->step_count);
            continue;
        }

        //;VGM_LOG("accept at %lx + %lx, skips=%i\n", data->physical_offset, data->block_size, data->step_count);

        /* read block data */
        {
            size_t bytes_consumed, bytes_done, to_read;

            bytes_consumed = offset - data->logical_offset;
            to_read = data->data_size - bytes_consumed;
            if (to_read > length)
                to_read = length;
            bytes_done = read_streamfile(dest, data->physical_offset + data->skip_size + bytes_consumed, to_read, sf);

            if (data->cfg.read_callback) {
                data->cfg.read_callback(dest, data, bytes_consumed, bytes_done);
            }

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

static size_t deblock_io_size(STREAMFILE* sf, deblock_io_data* data) {
    uint8_t buf[0x04];

    if (data->logical_size)
        return data->logical_size;

    if (data->cfg.logical_size) {
        data->logical_size = data->cfg.logical_size;
        return data->logical_size;
    }

    /* force a fake read at max offset, to get max logical_offset (will be reset next read) */
    deblock_io_read(sf, buf, 0x7FFFFFFF, 1, data);
    data->logical_size = data->logical_offset;
    
    //todo tests:
    //if (logical_size > max_physical_offset)
    //    return 0;
    //if (logical_size != data->stream_size)
    //    return 0;

    
    return data->logical_size;
}

/* generic "de-blocker" helper for streams divided in blocks that have weird interleaves, their
 * decoder can't easily use blocked layout, or some other weird feature. It "filters" data so
 * reader only sees clean data without blocks. Must pass setup config and a callback that sets
 * sizes of a single block. */
STREAMFILE* open_io_deblock_streamfile_f(STREAMFILE* sf, deblock_config_t *cfg) {
    STREAMFILE* new_sf = NULL;
    deblock_io_data io_data = {0};

    /* prepare data */
    io_data.cfg = *cfg; /* memcpy */

    if (io_data.cfg.block_callback == NULL)
        io_data.cfg.block_callback = block_callback_default;

    if (io_data.cfg.stream_start < 0)
        goto fail;
    if (io_data.cfg.step_start < 0 || io_data.cfg.step_count < 0)
        goto fail;

    if (io_data.cfg.step_count > 0) {
        io_data.cfg.step_count--;
    }
/*
    if (io_data.cfg.read_count == 0)
        io_data.cfg.read_count = 1;
*/
    io_data.physical_size = io_data.cfg.stream_size;
    if (io_data.physical_size > get_streamfile_size(sf) + io_data.cfg.stream_start || io_data.physical_size == 0)
        io_data.physical_size = get_streamfile_size(sf) - io_data.cfg.stream_start;
    io_data.physical_end = io_data.cfg.stream_start + io_data.physical_size;

    io_data.logical_offset = -1; /* read reset */

    //TODO: other validations

    /* setup subfile */
    new_sf = open_io_streamfile_f(sf, &io_data, sizeof(deblock_io_data), deblock_io_read, deblock_io_size);
    return new_sf;
fail:
    VGM_LOG("DEBLOCK: bad init\n");
    close_streamfile(sf);
    return NULL;
}
