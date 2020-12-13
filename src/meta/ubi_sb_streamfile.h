#ifndef _UBI_SB_STREAMFILE_H_
#define _UBI_SB_STREAMFILE_H_
#include "../streamfile.h"
#include "ubi_sb_garbage_streamfile.h"


typedef struct {
    /* config */
    off_t stream_offset;
    off_t stream_size;
    int layer_number;
    int layer_count;
    int layer_max;
    int big_endian;
    int layer_hijack;

    /* internal config */
    off_t header_next_start;    /* offset to header field */
    off_t header_sizes_start;   /* offset to header table */
    off_t header_data_start;    /* offset to header data */
    off_t block_next_start;     /* offset to block field */
    off_t block_sizes_start;    /* offset to block table */
    off_t block_data_start;     /* offset to block data */
    size_t header_size;         /* derived */

    /* state */
    off_t logical_offset;       /* fake offset */
    off_t physical_offset;      /* actual offset */
    size_t block_size;          /* current size */
    size_t next_block_size;     /* next size */
    size_t skip_size;           /* size from block start to reach data */
    size_t data_size;           /* usable size in a block */

    size_t logical_size;
} ubi_sb_io_data;


static size_t ubi_sb_io_read(STREAMFILE* sf, uint8_t* dest, off_t offset, size_t length, ubi_sb_io_data* data) {
    uint32_t(*read_u32)(off_t, STREAMFILE*) = data->big_endian ? read_u32be : read_u32le;
    size_t total_read = 0;
    int i;


    /* re-start when previous offset (can't map logical<>physical offsets) */
    if (data->logical_offset < 0 || offset < data->logical_offset) {
        data->physical_offset = data->stream_offset;
        data->logical_offset = 0x00;
        data->data_size = 0;

        /* process header block (slightly different and data size may be 0) */
        {
            data->block_size = data->header_size;
            data->next_block_size = read_u32(data->physical_offset + data->header_next_start, sf);

            if (data->header_sizes_start) {
                data->skip_size = data->header_data_start;
                for (i = 0; i < data->layer_number; i++) {
                    data->skip_size += read_u32(data->physical_offset + data->header_sizes_start + i*0x04, sf);
                }
                data->data_size = read_u32(data->physical_offset + data->header_sizes_start + data->layer_number*0x04, sf);
            }

            if (data->data_size == 0) {
                data->physical_offset += data->block_size;
            }
        }
    }


    /* read blocks */
    while (length > 0) {

        /* ignore EOF */
        if (offset < 0 || data->physical_offset >= data->stream_offset + data->stream_size) {
            break;
        }

        /* process new block */
        if (data->data_size == 0) {
            data->block_size = data->next_block_size;
            if (data->block_next_start) /* not set when fixed block size */
                data->next_block_size = read_u32(data->physical_offset + data->block_next_start, sf);

            data->skip_size = data->block_data_start;
            for (i = 0; i < data->layer_number; i++) {
                data->skip_size += read_u32(data->physical_offset + data->block_sizes_start + i*0x04, sf);
            }
            data->data_size = read_u32(data->physical_offset + data->block_sizes_start + data->layer_number*0x04, sf);
        }

        /* move to next block */
        if (offset >= data->logical_offset + data->data_size) {
            if (data->block_size == 0 || data->block_size == 0xFFFFFFFF)
                break;
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

static size_t ubi_sb_io_size(STREAMFILE* sf, ubi_sb_io_data* data) {
    uint8_t buf[1];

    if (data->logical_size)
        return data->logical_size;

    /* force a fake read at max offset, to get max logical_offset (will be reset next read) */
    ubi_sb_io_read(sf, buf, 0x7FFFFFFF, 1, data);
    data->logical_size = data->logical_offset;

    return data->logical_size;
}


static int ubi_sb_io_init(STREAMFILE* sf, ubi_sb_io_data* data) {
    uint32_t(*read_u32)(off_t, STREAMFILE*) = data->big_endian ? read_u32be : read_u32le;
    off_t offset = data->stream_offset; 
    uint32_t version;
    int i;

    if (data->stream_offset + data->stream_size > get_streamfile_size(sf)) {
        VGM_LOG("UBI SB: bad size\n");
        goto fail;
    }

    /* Layers have a main header, then headered blocks with data.
     * We configure stuff to unify parsing of all variations. */
    version = read_u32(offset+0x00, sf);

    /* it was bound to happen... orz */
    if (data->layer_hijack == 1 && version == 0x000B0008)
        version = 0xFFFF0007;

    switch(version) {
        case 0x00000002: /* Splinter Cell */
            /* - layer header
             * 0x04: layer count
             * 0x08: stream size
             * 0x0c: block header size
             * 0x10: block size (fixed)
             * 0x14: min layer size?
             * - block header
             * 0x00: block number
             * 0x04: block offset
             * 0x08+(04*N): layer size per layer
             * 0xNN: layer data per layer */
            data->layer_max = read_u32(offset+0x04, sf);

            data->header_next_start     = 0x10;
            data->header_sizes_start    = 0;
            data->header_data_start     = 0x18;

            data->block_next_start      = 0;
            data->block_sizes_start     = 0x08;
            data->block_data_start      = 0x08 + data->layer_max*0x04;
            break;

        case 0x00000003: /* Rainbow Six 3 */
            /* - layer header
             * 0x04: layer count
             * 0x08: stream size
             * 0x0c: block header size
             * 0x10: block size (fixed)
             * 0x14: min layer data?
             * 0x18: size of header sizes and headers
             * 0x1c+(04*N): header size per layer
             * - block header
             * 0x00: block number
             * 0x04: block offset
             * 0x08+(04*N): layer size per layer
             * 0xNN: layer data per layer */
            data->layer_max = read_u32(offset+0x04, sf);

            data->header_next_start     = 0x10;
            data->header_sizes_start    = 0x1c;
            data->header_data_start     = 0x1c + data->layer_max*0x04;

            data->block_next_start      = 0;
            data->block_sizes_start     = 0x08;
            data->block_data_start      = 0x08 + data->layer_max*0x04;
            break;

        case 0x00000004: /* Prince of Persia: Sands of Time, Batman: Rise of Sin Tzu */
            /* - layer header
             * 0x04: layer count
             * 0x08: stream size
             * 0x0c: block count
             * 0x10: block header size
             * 0x14: block size (fixed)
             * 0x18: min layer data?
             * 0x1c: size of header sizes and headers
             * 0x20+(04*N): header size per layer
             * - block header
             * 0x00: block number
             * 0x04: block offset
             * 0x08: always 0x03
             * 0x0c+(04*N): layer size per layer
             * 0xNN: layer data per layer */
            data->layer_max = read_u32(offset+0x04, sf);

            data->header_next_start     = 0x14;
            data->header_sizes_start    = 0x20;
            data->header_data_start     = 0x20 + data->layer_max*0x04;

            data->block_next_start      = 0;
            data->block_sizes_start     = 0x0c;
            data->block_data_start      = 0x0c + data->layer_max*0x04;
            break;

        case 0x00000007: /* Splinter Cell: Essentials, Splinter Cell 3D */
            /* - layer header
             * 0x04: config?
             * 0x08: layer count
             * 0x0c: stream size
             * 0x10: block count
             * 0x14: block header size
             * 0x18: block size (fixed)
             * 0x1c+(04*8): min layer data? for 8 layers (-1 after layer count)
             * 0x3c: size of header sizes and headers
             * 0x40+(04*N): header size per layer
             * 0xNN: header data per layer
             * - block header
             * 0x00: block number
             * 0x04: block offset
             * 0x08: always 0x03
             * 0x0c+(04*N): layer size per layer
             * 0xNN: layer data per layer */
            data->layer_max = read_u32(offset+0x08, sf);

            data->header_next_start     = 0x18;
            data->header_sizes_start    = 0x40;
            data->header_data_start     = 0x40 + data->layer_max*0x04;

            data->block_next_start      = 0;
            data->block_sizes_start     = 0x0c;
            data->block_data_start      = 0x0c + data->layer_max*0x04;
            break;

        case 0xFFFF0007: /* Ghost Recon Advanced Warfighter (X360) */
            /* - layer header
             * 0x04: config?
             * 0x08: layer count
             * 0x0c: stream size
             * 0x10: block count
             * 0x14: block header size
             * 0x18: block size (fixed)
             * 0x1c+(04*11): min layer data? for 11 layers (-1 after layer count)
             * 0x48: size of header sizes and headers
             * 0x4c+(04*N): header size per layer
             * 0xNN: header data per layer
             * - block header
             * 0x00: block number
             * 0x04: block offset
             * 0x08: always 0x03
             * 0x0c+(04*N): layer size per layer
             * 0xNN: layer data per layer */
            data->layer_max = read_u32(offset+0x08, sf);

            data->header_next_start     = 0x18;
            data->header_sizes_start    = 0x4c;
            data->header_data_start     = 0x4c + data->layer_max*0x04;

            data->block_next_start      = 0;
            data->block_sizes_start     = 0x0c;
            data->block_data_start      = 0x0c + data->layer_max*0x04;
            break;

        case 0x00040008: /* Assassin's Creed */
        case 0x000B0008: /* Open Season, Surf's Up, TMNT, Splinter Cell HD */
        case 0x000C0008: /* Splinter Cell: Double Agent */
        case 0x00100008: /* Rainbow Six 2 */
            /* - layer header
             * 0x04: config?
             * 0x08: layer count
             * 0x0c: blocks count
             * 0x10: block header size
             * 0x14: size of header sizes and headers/data
             * 0x18: next block size
             * 0x1c+(04*N): layer header size
             * 0xNN: header data per layer
             * - block header:
             * 0x00: always 0x03
             * 0x04: next block size
             * 0x08+(04*N): layer size per layer
             * 0xNN: layer data per layer */
            data->layer_max = read_u32(offset+0x08, sf);

            data->header_next_start     = 0x18;
            data->header_sizes_start    = 0x1c;
            data->header_data_start     = 0x1c + data->layer_max*0x04;

            data->block_next_start      = 0x04;
            data->block_sizes_start     = 0x08;
            data->block_data_start      = 0x08 + data->layer_max*0x04;
            break;

        case 0x00100009: /* Splinter Cell: Pandora Tomorrow HD, Prince of Persia 2008, Scott Pilgrim */
            /* - layer header
             * 0x04: config?
             * 0x08: layer count
             * 0x0c: blocks count
             * 0x10: block header size
             * 0x14: size of header sizes and headers/data
             * 0x18: next block size
             * 0x1c+(04*10): usable size per layer
             * 0x5c+(04*N): layer header size
             * 0xNN: header data per layer
             * - block header:
             * 0x00: always 0x03
             * 0x04: next block size
             * 0x08+(04*N): layer size per layer
             * 0xNN: layer data per layer */
            data->layer_max = read_u32(offset+0x08, sf);

            data->header_next_start     = 0x18;
            data->header_sizes_start    = 0x5c;
            data->header_data_start     = 0x5c + data->layer_max*0x04;

            data->block_next_start      = 0x04;
            data->block_sizes_start     = 0x08;
            data->block_data_start      = 0x08 + data->layer_max*0x04;
            break;

        default: 
            VGM_LOG("UBI SB: unknown layer header %08x\n", version);
            goto fail;
    }

    /* get base size to simplify later parsing */
    data->header_size = data->header_data_start;
    if (data->header_sizes_start) {
        for (i = 0; i < data->layer_max; i++) {
            data->header_size += read_u32(offset + data->header_sizes_start + i*0x04, sf);
        }
    }

    /* force read header block */
    data->logical_offset = -1;
    
    /* just in case some headers may use less layers that stream has */
    VGM_ASSERT(data->layer_count != data->layer_max, "UBI SB: non-matching layer counts\n");
    if (data->layer_count > data->layer_max) {
        VGM_LOG("UBI SB: layer count bigger than layer max\n");
        goto fail;
    }

    /* Common layer quirks:
     * - layer format depends on its own version and not on platform or DARE engine version
     * - codec header may be in the layer header, or in the first block
     * - stream size doesn't include padding
     * - block number goes from 1 to block_count
     * - block offset is relative to layer start
     * - blocks data size varies between blocks and between layers in the same block
     * - "config?" is a small value that varies between streams of the same game
     * - next block size is 0 at last block
     * - both Ubi SB and Ubi BAO use same-version layers
     */

    return 1;
fail:
    return 0;
}


/* Handles deinterleaving of Ubisoft's headered+blocked 'multitrack' streams */
static STREAMFILE* setup_ubi_sb_streamfile(STREAMFILE* sf, off_t stream_offset, size_t stream_size, int layer_number, int layer_count, int big_endian, int layer_hijack) {
    STREAMFILE* new_sf = NULL;
    ubi_sb_io_data io_data = {0};

    io_data.stream_offset = stream_offset;
    io_data.stream_size = stream_size;
    io_data.layer_number = layer_number;
    io_data.layer_count = layer_count;
    io_data.big_endian = big_endian;
    io_data.layer_hijack = layer_hijack;

    if (!ubi_sb_io_init(sf, &io_data))
        goto fail;

    /* setup subfile */
    new_sf = open_wrap_streamfile(sf);
    if (layer_hijack == 2 && is_garbage_stream(sf)) {
        new_sf = setup_ubi_sb_garbage_streamfile_f(new_sf);
        //io_data.stream_size = get_garbage_stream_size(stream_offset, stream_size); //todo must do relative calcs, doesn't seem needed
    }
    new_sf = open_io_streamfile_f(new_sf, &io_data, sizeof(ubi_sb_io_data), ubi_sb_io_read,ubi_sb_io_size);
    new_sf = open_buffer_streamfile_f(new_sf,0);
    return new_sf;
fail:
    return NULL;
}

#endif /* _UBI_SB_STREAMFILE_H_ */
