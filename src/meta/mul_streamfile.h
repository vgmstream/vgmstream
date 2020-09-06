#ifndef _MUL_STREAMFILE_H_
#define _MUL_STREAMFILE_H_
#include "deblock_streamfile.h"


static void block_callback(STREAMFILE* sf, deblock_io_data* data) {
    uint32_t (*read_u32)(off_t,STREAMFILE*) = data->cfg.big_endian ? read_u32be : read_u32le;

    /* Blocks have base header + sub-blocks with track sub-header.
     * Some blocks don't contain all channels (instead they begin next block). */

    if (data->chunk_size && data->chunk_size < 0x10) {
        /* padding after all sub-blocks */
        data->block_size = data->chunk_size;
        data->data_size = 0;
        data->skip_size = 0;
        data->chunk_size = 0;
    }
    else if (data->chunk_size) {
        /* audio block sub-headers, ignore data for other tracks */
        uint32_t track_size   = read_u32(data->physical_offset + 0x00, sf);
        uint32_t track_number = read_u32(data->physical_offset + 0x04, sf);
        /* 0x08: dummy (may contain un-init'd data) */
        /* 0x0c: dummy (may contain un-init'd data) */

        data->block_size = 0x10 + track_size;
        data->data_size = 0;
        data->skip_size = 0;

        if (track_number == data->cfg.track_number) {
            data->data_size = track_size;
            data->skip_size = 0x10;
        }

        data->chunk_size -= data->block_size;
    }
    else {
        /* base block header */
        uint32_t block_type = read_u32(data->physical_offset + 0x00, sf);
        uint32_t block_size = read_u32(data->physical_offset + 0x04, sf);
        /* 0x08: dummy */
        /* 0x0c: dummy */

        /* blocks are padded after all sub-blocks */
        if (block_size % 0x10) {
            block_size = block_size + 0x10 - (block_size % 0x10);
        }

        data->data_size = 0;
        data->skip_size = 0;

        if (block_type == 0x00 && block_size != 0) {
            /* audio block */
            data->block_size = 0x10;
            data->chunk_size = block_size;
        }
        else {
            /* non-audio block (or empty audio block) */
            data->block_size = block_size + 0x10;
        }
    }
}

/* Deinterleaves MUL streams */
static STREAMFILE* setup_mul_streamfile(STREAMFILE* sf, off_t offset, int big_endian, int track_number, int track_count, const char* extension) {
    STREAMFILE *new_sf = NULL;
    deblock_config_t cfg = {0};

    cfg.stream_start = offset;
    cfg.big_endian = big_endian;
    cfg.track_number = track_number;
    cfg.track_count = track_count;
    cfg.block_callback = block_callback;

    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_deblock_streamfile_f(new_sf, &cfg);
    if (extension)
        new_sf = open_fakename_streamfile_f(new_sf, NULL, extension);
    return new_sf;
}

#endif /* _MUL_STREAMFILE_H_ */
