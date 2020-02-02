#ifndef _AIX_STREAMFILE_H_
#define _AIX_STREAMFILE_H_
#include "deblock_streamfile.h"

//todo block size must check >= stream_size

static void block_callback(STREAMFILE *sf, deblock_io_data *data) {
    uint32_t block_id = read_u32be(data->physical_offset + 0x00, sf);
    data->block_size  = read_u32be(data->physical_offset + 0x04, sf) + 0x08;

    /* check "AIXP" id, (AIX segments end with "AIXE" too) */
    if (block_id != 0x41495850) {
        return;
    }

    /* read target layer, otherwise ignore block */
    if (read_s8(data->physical_offset + 0x08, sf) == data->cfg.track_number) {
        /* 0x09(1): layer count */
        data->data_size = read_s16be(data->physical_offset + 0x0a, sf);
        /* 0x0c: -1 */
        data->skip_size = 0x10;
    }

    /* strange AIX in Tetris Collection (PS2) with padding before ADX start (no known flag) */
    if (data->logical_offset == 0x00 &&
            read_u32be(data->physical_offset + 0x10, sf) == 0 &&
            read_u16be(data->physical_offset + data->block_size - 0x28, sf) == 0x8000) {
        data->data_size = 0x28;
        data->skip_size = data->block_size - 0x28;
    }
}

/* Deinterleaves AIX layers */
static STREAMFILE* setup_aix_streamfile(STREAMFILE *sf, off_t stream_offset, size_t stream_size, int layer_number, const char* extension) {
    STREAMFILE *new_sf = NULL;
    deblock_config_t cfg = {0};

    cfg.stream_start = stream_offset;
    cfg.stream_size = stream_size;
    cfg.track_number = layer_number;
    cfg.block_callback = block_callback;

    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_deblock_streamfile_f(new_sf, &cfg);
    //new_sf = open_buffer_streamfile_f(new_sf, 0);
    if (extension)
        new_sf = open_fakename_streamfile_f(new_sf, NULL, extension);
    return new_sf;
}

#endif /* _AIX_STREAMFILE_H_ */
