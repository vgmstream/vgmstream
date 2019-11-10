#ifndef _EA_EAAC_OPUS_STREAMFILE_H_
#define _EA_EAAC_OPUS_STREAMFILE_H_
#include "../streamfile.h"

typedef struct deblock_config_t deblock_config_t;
typedef struct deblock_io_data deblock_io_data;

 struct deblock_config_t {
    /* config (all optional) */
    size_t logical_size;    /* pre-calculated size for performance (otherwise has to read the whole thing) */
    off_t stream_start;     /* data start */
    size_t stream_size;     /* data max */

    size_t chunk_size;      /* some size like a constant interleave */
    size_t skip_size;       /* same */

    int codec;              /* codec or type variations */
    int channels;
    int big_endian;
    uint32_t config;        /* some non-standard config value */

    /* read=blocks from out stream to read) and "steps" (blocks from other streams to skip) */
    int step_start;         /* initial step_count at stream start (often 0) */
    int step_count;         /* number of blocks to step over from other streams */
    int read_count;         /* number of blocks to read from this stream, after steps */

    size_t track_size;
    int track_number;
    int track_count;
    size_t interleave_count;
    size_t interleave_last_count;

    /* callback that setups deblock_io_data state, normally block_size and data_size */
    void (*block_callback)(STREAMFILE *sf, off_t offset, deblock_io_data *data);
} ;


struct  deblock_io_data{
    /* initial config */
    deblock_config_t cfg;

    /* state */
    off_t logical_offset;   /* fake deblocked offset */
    off_t physical_offset;  /* actual file offset */
    off_t block_size;       /* current block (added to physical offset) */
    off_t skip_size;        /* data to skip from block start to reach data (like a header) */
    off_t data_size;        /* usable data in a block (added to logical offset) */
//todo head/foot?
    int step_count;         /* number of blocks to step over */
    int read_count;         /* number of blocks to read */

    size_t logical_size;
    size_t physical_size;
    off_t physical_end;
} ;


static void block_callback_default(STREAMFILE *sf, off_t offset, deblock_io_data *data) {
    data->block_size = data->cfg.chunk_size;
    data->skip_size = data->cfg.skip_size;
    data->data_size = data->block_size - data->skip_size;
}

static size_t deblock_io_read(STREAMFILE *sf, uint8_t *dest, off_t offset, size_t length, deblock_io_data* data) {
    size_t total_read = 0;


    /* re-start when previous offset (can't map logical<>physical offsets) */
    if (data->logical_offset < 0 || offset < data->logical_offset) {
        ;VGM_LOG("DEBLOCK: restart offset=%lx + %x, po=%lx, lo=%lx\n", offset, length, data->physical_offset, data->logical_offset);
        data->physical_offset = data->cfg.stream_start;
        data->logical_offset = 0x00;
        data->block_size = 0;
        data->data_size = 0;
        data->skip_size = 0;

        data->step_count = data->cfg.step_start;
        data->read_count = data->cfg.read_count;
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
            data->cfg.block_callback(sf, offset, data);

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
            //VGM_LOG("ignore at %lx + %lx, skips=%i, reads=%i\n", data->physical_offset, data->block_size, data->step_count, data->read_count);
            continue;
        }

        //VGM_LOG("accept at %lx + %lx, skips=%i, reads=%i\n", data->physical_offset, data->block_size, data->step_count, data->read_count);

        /* read block data */
        {
            size_t bytes_consumed, bytes_done, to_read;

            bytes_consumed = offset - data->logical_offset;
            to_read = data->data_size - bytes_consumed;
            if (to_read > length)
                to_read = length;
            bytes_done = read_streamfile(dest, data->physical_offset + data->skip_size + bytes_consumed, to_read, sf);

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

static size_t deblock_io_size(STREAMFILE *streamfile, deblock_io_data* data) {
    uint8_t buf[0x04];

    if (data->logical_size)
        return data->logical_size;

    if (data->cfg.logical_size) {
        data->logical_size = data->cfg.logical_size;
        return data->logical_size;
    }

    /* force a fake read at max offset, to get max logical_offset (will be reset next read) */
    deblock_io_read(streamfile, buf, 0x7FFFFFFF, 1, data);
    data->logical_size = data->logical_offset;
    return data->logical_size;
}

/* generic "de-blocker" helper for streams divided in blocks that have weird interleaves, their
 * decoder can't easily use blocked layout, or some other weird feature. Must pass a
 * deblock_config_t with setup and a callback that sets sizes of a single block. */
static STREAMFILE* open_io_deblocker_streamfile_f(STREAMFILE *sf, deblock_config_t *cfg) {
    STREAMFILE *new_sf = NULL;
    deblock_io_data io_data = {0};

    /* prepare data */
    io_data.cfg = *cfg; /* memcpy */

    if (io_data.cfg.block_callback == NULL)
        io_data.cfg.block_callback = block_callback_default;

    if (io_data.cfg.stream_start < 0)
        goto fail;
    if (io_data.cfg.step_start < 0 || io_data.cfg.step_count < 0)
        goto fail;

    if (io_data.cfg.read_count == 0)
        io_data.cfg.read_count = 1;

    io_data.physical_size = io_data.cfg.stream_size;
    if (io_data.physical_size > get_streamfile_size(sf) + io_data.cfg.stream_start || io_data.physical_size == 0)
        io_data.physical_size = get_streamfile_size(sf) - io_data.cfg.stream_start;
    io_data.physical_end = io_data.cfg.stream_start + io_data.physical_size;
VGM_LOG("ps=%x, pe=%lx\n", io_data.physical_size, io_data.physical_end);
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

/*****************************************************/

static void block_callback(STREAMFILE *sf, off_t offset, deblock_io_data *data) {
    /* read the whole block, will be skipped for unwanted sub-streams */
    data->block_size = 0x02 + read_u16be(data->physical_offset, sf);
    data->data_size = data->block_size;
    //VGM_LOG("read at %lx + %lx, skips=%i, reads=%i\n", data->physical_offset, data->block_size, data->step_count, data->read_count);
}

static STREAMFILE* open_io_eaac_opus_streamfile_f(STREAMFILE *new_sf, int stream_number, int stream_count) {
    deblock_config_t cfg = {0};

    cfg.step_start = stream_number;
    cfg.step_count = stream_count - 1;
    cfg.block_callback = block_callback;
    /* starts from 0 since new_sf is pre-deblocked */

    /* setup subfile */
    //new_sf = open_wrap_streamfile(sf); /* to be used with others */
    new_sf = open_io_deblocker_streamfile_f(new_sf, &cfg);
    return new_sf;
}

#endif /* _EA_EAAC_OPUS_STREAMFILE_H_ */
